/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Your Private River (pages/river/yours.html, /yours/): an account on the River's service (server/README.md).
 *   signed out   sign in, an account made (Continue with Google when the service has it: /api/auth/providers),
 *                a forgotten password; the links it emails land on the front page and come here: #verify= (the
 *                session of the account it was sent for: if signed out, kept until signed in), #reset=; #google=
 *   signed in    the title is the switch: SHARDS n · FAVORITES n · BACKUPS n (#shards, #favorites, #backups), with
 *                the dock at its right: GET FROM FM-1 (the FM-1 read by itself, its sixteen slots as a checklist:
 *                the new ones ticked -> POST /api/shards; those in one's River marked; the factory's unticked; and
 *                the whole backup -> POST /api/backups) | SEND TO FM-1 (the shards ticked; none: choose them first).
 *                SHARDS: my library as rows (RiverCards.row): ▶, opened in place, its head the one action zone
 *                (Send to FM-1, Edit, Share/Shared, Copy link once shared, Delete); FAVORITES: others' shards
 *                bookmarked (GET /api/favorites; "no longer shared" when their author took them back), played and
 *                sent like one's own; BACKUPS: their slots' names, restore to the FM-1, download, delete.
 *                A long list (more than 32) gets the toolbar: a search, the filters, a sort, all here in the page;
 *                24 rows, then 24 more. Ticked rows: Download, Share, Delete, Clear. The account itself is the
 *                profile's (river-profile.js)
 * What people typed is only ever text (river-cards.js builds no HTML from strings). */
"use strict";
(() => {
  if (!document.getElementById("auth")) return;
  const { h, fill, two, ticked, openRow, toast, confirm, listWords, I } = RiverCards;
  const A = RiverAPI, D = RiverDevice, S = FB.S, RL = RiverList;
  const $ = (id) => document.getElementById(id);
  const V = D.voice($("voice"));
  const kept = { get: (k) => { try { return sessionStorage.getItem(k); } catch (_) { return null; } },
                 set: (k, v) => { try { if (v == null) sessionStorage.removeItem(k); else sessionStorage.setItem(k, v); } catch (_) { /* this tab only */ } } };
  const NL = S.scene_t.name_len, NAME = new RegExp(`^[A-Z0-9 .#+-]{1,${NL}}$`);
  /* focus where it was: the same action in a row drawn again (its data-act), or its name */
  const refocus = (art, actKind) => { const b = art && ((actKind && art.querySelector(`[data-act="${actKind}"]`)) || art.querySelector(".nm-n")); if (b) b.focus({ preventScroll: true }); };
  let me = null;

  // ================================================================ the links: #verify=, #reset=, #google=, #signin, #register
  let resetToken = null;
  function readHash() {
    const m = /^#(verify|reset|google)=([\w-]{1,200})$/.exec(location.hash);
    if (m) {
      history.replaceState(null, "", location.pathname + location.search);
      if (m[1] === "verify") kept.set("river-verify", m[2]);
      if (m[1] === "reset") resetToken = m[2];
      if (m[1] === "google") V.say(m[2] === "banned" ? "This account is banned from the River." : "Signing in with Google didn’t work. Try again, or sign in with your email.", "err");
    } else if (location.hash === "#register" || location.hash === "#signin") tab(location.hash.slice(1));
  }

  // ================================================================ signed out
  function tab(name) {
    document.querySelectorAll("#auth form").forEach((f) => { f.hidden = f.dataset.tab !== name; });
    document.querySelectorAll("#auth-tabs button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.tab === name)));
    $("auth-tabs").hidden = name === "reset";
    const f = document.querySelector(`#auth form[data-tab="${name}"]`);
    const first = f && f.querySelector("input");
    if (first && !$("auth").hidden) first.focus({ preventScroll: true });
  }
  document.querySelectorAll("#auth-tabs button").forEach((b) => b.addEventListener("click", () => tab(b.dataset.tab)));
  document.querySelectorAll("#auth [data-go]").forEach((b) => b.addEventListener("click", () => tab(b.dataset.go)));
  /* a form sent: its fields as JSON, its own line for what went wrong */
  function form(id, run) {
    const f = $(id), say = f.querySelector(".form-say");
    f.addEventListener("submit", async (e) => {
      e.preventDefault();
      say.textContent = "";
      const btn = f.querySelector("[type=submit]");
      btn.disabled = true;
      try { await run(Object.fromEntries(new FormData(f)), say, f); }
      catch (err) { say.textContent = err.message; }
      finally { btn.disabled = false; }
    });
  }
  form("signin", async (d) => {
    if (!d.email || !d.password) throw new Error("Your email and your password.");
    const u = await A.post("/api/auth/login", { email: d.email, password: d.password });
    $("signin").reset();
    enter(u, `Welcome back, ${u.display_name}.`);
  });
  form("register", async (d) => {
    if (!d.display_name.trim()) throw new Error("Choose a display name: it is shown as the author of what you share.");
    if ((d.password || "").length < 8) throw new Error("A password: 8 characters at least.");
    const u = await A.post("/api/auth/register", { email: d.email, password: d.password, display_name: d.display_name });
    $("register").reset();
    enter(u, ["Your River is ready. A link to confirm your email is on its way to ", h("b", null, u.email), ": you need it to share."]);
  });
  form("f-forgot", async (d, say) => {
    await A.post("/api/auth/forgot", { email: d.email });
    say.textContent = "If an account has this email, a link is on its way: it works for an hour, once.";
  });
  form("f-reset", async (d) => {
    if ((d.password || "").length < 8) throw new Error("A password: 8 characters at least.");
    const u = await A.post("/api/auth/reset", { token: resetToken, password: d.password });
    resetToken = null;
    $("f-reset").reset();
    enter(u, "Your new password is saved, and you are signed in. Your other sessions ended.");
  });

  // ================================================================ in, out
  async function enter(u, words) {
    A.setMe(u);
    show(u);
    if (words) V.say(words, "ok");
    const token = kept.get("river-verify");
    if (token && u) await verify(token);
  }
  async function verify(token) {
    try {
      await A.post("/api/auth/verify", { token });
      kept.set("river-verify", null);
      A.setMe(await A.get("/api/me"));
      show(A.signedIn);
      V.say("Your email is confirmed: you can share shards in the River now.", "ok");
    } catch (e) {
      if (e.status === 400) kept.set("river-verify", null);
      V.say(e.message, "err");
    }
  }
  function show(u) {
    me = u;
    $("auth").hidden = !!u && !resetToken;          // (a reset link: its form first, even signed in)
    $("mine").hidden = !u || !!resetToken;
    if (!u || resetToken) {
      tab(resetToken ? "reset" : document.querySelector("#auth-tabs [aria-pressed=true]").dataset.tab);
      if (kept.get("river-verify") && !resetToken) V.say("Sign in with the account this link was sent to: then your email is confirmed.", "");
      return;
    }
    $("vb").hidden = u.verified;
    $("vb-email").textContent = u.email;
    loadAll();
  }
  $("vb-resend").addEventListener("click", async () => {
    try {
      const r = await A.post("/api/auth/resend");
      if (r.verified) { A.setMe(await A.get("/api/me")); show(A.signedIn); V.say("Your email is confirmed already.", "ok"); }
      else V.say(["A new link is on its way to ", h("b", null, me.email), "."], "ok");
    } catch (e) { V.say(e.message, "err"); }
  });

  // ================================================================ signed in: SHARDS · FAVORITES · BACKUPS
  const TOOLBAR = 32;              // a list longer than this gets the toolbar (search, filters, sort)
  const F = RL.FILTER;
  let subject = /^#(favorites|backups)$/.test(location.hash) ? location.hash.slice(1) : "shards";
  let shards = [], favorites = [], backups = [], loaded = false;
  const st = { shards: { q: "", f: {}, sort: "new" }, favorites: { q: "", f: {}, sort: "new" } };
  let shown = RL.PAGE, view = [], tb = null;
  const sel = new Map();           // the rows ticked (in the subject shown): id -> the shard
  /* each row's scene ("m:" one's own, "f:" a favorite, and its id), built for the shard it shows: the same bytes in
   * one's own copy and a favorite stay two scenes, each knowing its shard and its list (sc.fav) */
  const scenes = new Map();
  const sceneOf = (x, f = fav()) => {
    const key = (f ? "f:" : "m:") + x.id, c = scenes.get(key);
    if (c && c.x === x) return c.sc;
    const sc = A.scene(x);
    if (sc) sc.fav = f;
    scenes.set(key, { x, sc });
    return sc;
  };
  /* is this scene in one's River (one's shards or favorites)? Its sound compared, its name left out (renamed here,
   * it is still the same scene) */
  let known = null;
  const soundOf = (bytes) => A.b64(bytes.subarray(NL));
  function inRiver(sc) {
    if (!sc || !sc.bytes) return false;
    if (!known) known = new Set([...shards, ...favorites].map((x) => { try { return soundOf(A.unb64(x.data)); } catch (_) { return ""; } }));
    return known.has(soundOf(sc.bytes));
  }
  const dock = RL.dock({ get: () => openGet(), send: () => sendSel() });

  async function loadAll() {
    try {
      const [a, b, c] = await Promise.all([A.get("/api/shards"), A.get("/api/favorites"), A.get("/api/backups")]);
      shards = a.items; favorites = b.items; backups = c.items; loaded = true; known = null;
      paint();
      if (/^#(grab|get)$/.test(location.hash)) { history.replaceState(null, "", location.pathname + location.search); openGet(); }
    } catch (e) { V.say(e.message, "err"); }
  }
  async function loadShards() {
    try { shards = (await A.get("/api/shards")).items; known = null; if (subject === "shards") paint(); else paintTitle(); }
    catch (e) { V.say(e.message, "err"); }
  }
  async function loadBackups() {
    try { backups = (await A.get("/api/backups")).items; if (subject === "backups") paint(); else paintTitle(); }
    catch (e) { V.say(e.message, "err"); }
  }

  /* the subject shown: the title (the switch) and its list */
  function go(key, focus) {
    if (key === subject) return;
    subject = key;
    sel.clear();
    shown = RL.PAGE;
    tb = null;
    history.replaceState(null, "", location.pathname + location.search + (key === "shards" ? "" : "#" + key));
    paint();
    if (focus) { const t = document.querySelector("#pt .pt-t.is-on"); if (t) t.focus(); }
  }
  addEventListener("hashchange", () => {
    if (!me) return;
    const m = /^#(shards|favorites|backups|grab|get)$/.exec(location.hash);
    if (!m) return;
    if (m[1] === "grab" || m[1] === "get") { history.replaceState(null, "", location.pathname + location.search); openGet(); }
    else go(m[1]);
  });
  function paint() {
    if (!loaded) return;
    paintTitle();
    const lib = subject !== "backups";
    $("lib-h").textContent = subject === "favorites" ? "Your favorites" : "Your shards";
    $("lib").hidden = !lib;
    $("bk").hidden = lib;
    if (lib) paintList(); else { RiverCards.stopSound(); paintBackups(); }
  }
  function paintTitle() {
    known = null;
    const tab = (key, label, n) => ({ label, n, href: "#" + key, on: subject === key, onclick: (e) => { e.preventDefault(); go(key, true); } });
    const tabs = [tab("shards", "Shards", shards.length)];
    if (favorites.length || subject === "favorites") tabs.push(tab("favorites", "Favorites", favorites.length));
    tabs.push(tab("backups", "Backups", backups.length));
    // (no Web MIDI here, an iPhone's Safari say: one line where the dock stands)
    $("pt").replaceChildren(RL.title({ tabs, dock: D.hasMidi ? dock.el : h("p", { class: "pt-note" }, RL.NO_MIDI) }));
    dock.count(sel.size);
  }

  // ---------------------------------------------------------------- the list: one's shards, one's favorites
  const fav = () => subject === "favorites";
  function paintList() {
    const all = fav() ? favorites : shards, s = st[subject], tools = all.length > TOOLBAR;
    if (tools && !tb) {
      tb = RL.toolbar({ filters: [F.key, F.scale, F.glint, F.bass, ...(fav() ? [] : [F.shared])], sort: { options: RL.SORTS }, st: s,
                        onChange: () => { shown = RL.PAGE; sel.clear(); paintList(); } });   // (a selection never acts on rows out of view)
      $("tb").replaceChildren(tb.el);
    } else if (!tools) { tb = null; $("tb").replaceChildren(); }
    view = RL.sorted(tools ? all.filter((x) => RL.matches(x, s)) : all, tools ? s.sort : "new", fav() ? "favorited_at" : "created_at");
    if (tb) tb.n(`${view.length} ${view.length === 1 ? "shard" : "shards"}`);
    RiverCards.stopSound();
    const box = $("list");
    box.className = "list" + (fav() ? " fav" : "");
    box.replaceChildren(h("div", { class: "list-top" }), ...view.slice(0, shown).map((x, i) => rowOf(x, i)).filter(Boolean));
    paintSel();
    paintEnd(all.length, tools);
  }
  function more() {
    const from = shown, rows = view.slice(from, from + RL.PAGE).map((x, i) => rowOf(x, i)).filter(Boolean);
    shown += RL.PAGE;
    $("list").append(...rows);
    paintEnd((fav() ? favorites : shards).length, !!tb);
    const first = rows[0] && rows[0].querySelector(".nm-n");
    if (first) first.focus({ preventScroll: true });
  }
  function paintEnd(total, tools) {
    const n = view.length;
    let note = null;
    if (!total) note = fav() ? ["No favorites yet. Star the shards you love in the ", h("a", { href: "/" }, "Community River"), ": they wait here, ready to send to your FM-1."]
                             : ["No shards yet. ", D.hasMidi ? h("button", { type: "button", class: "txt-b", onclick: () => openGet() }, "Get them from your FM-1") : "Get them from your FM-1",
                                ", or favorite shards in the ", h("a", { href: "/" }, "Community River"), "."];
    else if (!n) note = ["No shard matches. ", h("button", { type: "button", class: "txt-b", onclick: clearFilters }, "Clear the filters"), "."];
    else if (!tools && n <= shown) note = fav() ? ["A favorite is a bookmark: it stays here, even if its author stops sharing it. Find more in the ", h("a", { href: "/" }, "Community River"), "."]
                                                : ["Get more from your FM-1, or favorite shards in the ", h("a", { href: "/" }, "Community River"), "."];
    $("list-end").replaceChildren(RL.end({ shown: Math.min(shown, n), total: n, more, note }));
  }
  function clearFilters() {
    st[subject] = { q: "", f: {}, sort: st[subject].sort };
    sel.clear();
    tb = null;
    shown = RL.PAGE;
    paintList();
  }
  const HEAD = { new: ["ADDED", "↓"], old: ["ADDED", "↑"], name: ["NAME", "↓"], key: ["KEY", "↓"] };
  /* the column labels, or the selection's bar in their place */
  function paintSel() {
    dock.count(sel.size);
    const top = $("list").querySelector(".list-top");
    if (!top) return;
    if (!view.length) { top.replaceChildren(); return; }
    if (sel.size) {
      top.replaceChildren(RL.selbar(sel.size, [
        RL.act(I.dl(), "Download", downloadSel, { txt: true }),
        fav() ? null : RL.act(I.share(), "Share", shareSel, { txt: true }),
        fav() ? null : RL.act(null, "Delete", deleteSel, { txt: true }),
      ].filter(Boolean), clearSel));
      return;
    }
    const [by, arrow] = HEAD[tb ? st[subject].sort : "new"] || HEAD.new;
    const c = (t, r) => ({ t, r, sort: t === by ? arrow : null });
    top.replaceChildren(RL.head([null, null, c("NAME"), c("WHAT PLAYS"), c("KEY"), c("GLINT"), c("BASS"), fav() ? c("AUTHOR") : null, c("ADDED", true)]));
  }
  /* a shard as a row: ✓, ▶, what plays, KEY, GLINT, BASS, then SHARED (or its author) and the date added; HIDDEN and
   * "no longer shared" also beside its name on a phone (side) and under it on a tablet (mark), where that column goes */
  function rowOf(x, i) {
    const f = fav(), sc = sceneOf(x, f);
    if (!sc) return null;
    const pub = x.visibility === "public", gone = f && x.shared === false, hid = !f && x.visibility === "hidden";
    const art = RiverCards.row(sc, {
      id: (f ? "f-" : "m-") + x.id, key: (f ? "f:" : "m:") + x.id, i, class: f ? "is-fav" : "is-mine",
      tick: { checked: sel.has(x.id), aria: `Select ${x.name}`, onChange: (on) => tick(x, on, art) },
      desc: RL.firstLine(x.description),
      mark: gone ? "NO LONGER SHARED" : hid ? "HIDDEN" : null,
      cells: f ? [byCell(x), h("span", { class: "dt" }, RL.day(x.favorited_at))] : [statusCell(x), h("span", { class: "dt" }, RL.day(x.created_at))],
      side: gone ? h("span", { class: "side-mk" }, "No longer shared") : f ? x.author
          : pub ? h("span", { class: "st dot" }, h("span", { class: "sr-only" }, "Shared")) : hid ? h("span", { class: "side-mk" }, "Hidden") : null,
      actions: f ? favActs(x, sc) : ownActs(x, sc),
      intro: () => RL.intro(x, { mine: !f, add: () => edit(x, true) }),
      edit: f ? { saveAs: (o) => saveAs(o, x), from: { name: x.name, author: x.author || "someone", first: x.origin_author || null } }
              : { saveAs: (o) => saveAs(o, x), save: (bytes) => saveSound(x, bytes), shared: x.visibility === "public", description: x.description },
      onPlay: f && x.shared !== false ? () => A.post(`/api/public/shards/${x.id}/played`).catch(() => {}) : null,
    });
    if (sel.has(x.id)) art.classList.add("kept");
    return art;
  }
  const statusCell = (x) => (x.visibility === "public" ? h("span", { class: "st" }, "SHARED")
    : x.visibility === "hidden" ? h("span", { class: "st is-hidden" }, "HIDDEN")
    : x.origin_author ? h("span", { class: "by" }, h("span", { class: "sr-only" }, "by "), x.origin_author) : h("span"));
  const byCell = (x) => h("span", { class: "by" }, h("span", { class: "sr-only" }, "by "), x.author || "someone", x.shared === false ? h("s", null, h("span", { class: "sr-only" }, ", "), "no longer shared") : null);
  function tick(x, on, art) {
    if (on) sel.set(x.id, x); else sel.delete(x.id);
    ticked(art, on);
    paintSel();
  }
  function clearSel() {
    sel.clear();
    $("list").querySelectorAll(".sr-item.kept").forEach((art) => ticked(art, false));
    paintSel();
    refocus($("list").querySelector(".sr-item"));   // (Clear is gone with the bar: the first row's name)
  }
  /* sent: a favorite still shared is counted, as in the River (each scene knows its shard and its list) */
  const sentCounted = (places) => places.forEach((p) => {
    const sc = p.item, x = sc && sc.shard;
    if (x && sc.fav && x.shared !== false) A.post(`/api/public/shards/${x.id}/sent`).catch(() => {});
  });
  const sendOne = (sc) => RL.send([sc], { onSent: sentCounted, inRiver });
  /* SEND TO FM-1: the shards ticked (16 at most: one per slot); none ticked, choose them first, among one's shards and
   * favorites (the subject shown first) */
  function sendSel() {
    const list = [...sel.values()];
    if (list.length > 16) { toast("16 at most: the FM-1 has sixteen slots."); return; }
    if (list.length) { RL.send(list.map((x) => sceneOf(x)).filter(Boolean), { onSent: sentCounted, inRiver }); return; }
    const items = (arr, f, at) => RL.sorted(arr, "new", at).map((x) => ({ sc: sceneOf(x, f), desc: RL.firstLine(x.description), date: RL.day(x[at]) }))
      .filter((it) => it.sc && it.sc.bytes);
    const lists = [{ key: "shards", label: "Shards", items: items(shards, false, "created_at") }, { key: "favorites", label: "Favorites", items: items(favorites, true, "favorited_at") }];
    if (!lists.some((l) => l.items.length)) { toast(D.hasMidi ? "No shards yet: get some from your FM-1 first." : RL.NO_MIDI); return; }
    if (fav()) lists.reverse();
    RL.send([], { choose: lists, onSent: sentCounted, inRiver });
  }

  // ---------------------------------------------------------------- one's own: its head's actions
  function ownActs(x, sc) {
    const pub = x.visibility === "public", hid = x.visibility === "hidden";
    return [
      RL.act(I.send(15), "Send to FM-1", () => sendOne(sc), { ice: true, of: x.name, act: "send" }),
      RL.act(I.edit(), "Rename", () => edit(x), { of: x.name, act: "edit" }),
      hid ? null : RL.act(I.share(), pub ? "Shared" : "Share", () => share(x), { on: pub, of: x.name, act: "share" }),
      pub ? RL.act(I.link(), "Copy link", () => RL.copyLink(x), { of: x.name, act: "link" }) : null,
      RL.act(null, "Delete", () => remove(x), { txt: true, of: x.name, act: "delete" }),
    ];
  }
  /* a favorite: someone else's (no longer shared: no link, no report) */
  function favActs(x, sc) {
    const shared = x.shared !== false;
    return [
      RL.act(I.send(15), "Send to FM-1", () => sendOne(sc), { ice: true, of: x.name, act: "send" }),
      RL.favButton(x, (y) => { if (!y.favorited) unfavorited(y); }),
      shared ? RL.act(I.link(), "Copy link", () => RL.copyLink(x), { of: x.name, act: "link" }) : null,
      shared ? RL.act(null, "Report", () => RL.report(x), { txt: true, of: x.name, act: "report" }) : null,
    ];
  }
  /* a favorite taken out: its row goes, the next one drawn in its place at the end (24 still shown), focus on the
   * row after it */
  function unfavorited(y) {
    favorites = favorites.filter((x) => x.id !== y.id);
    sel.delete(y.id);
    const art = $(`f-${y.id}`), next = art && (art.nextElementSibling || art.previousElementSibling);
    view = view.filter((x) => x.id !== y.id);
    if (art) { if (RiverCards.soundKey === `f:${y.id}`) RiverCards.stopSound(); art.remove(); }
    if (fav() && shown <= view.length && !$(`f-${view[shown - 1].id}`)) { const r = rowOf(view[shown - 1], shown - 1); if (r) $("list").append(r); }
    paintTitle();
    paintSel();
    paintEnd(favorites.length, !!tb);
    if (art) refocus(next && next.classList.contains("sr-item") ? next : $("list").querySelector(".sr-item"));
  }
  /* one shard's row again, from what the service says now (opened if it was; focus on the same action) */
  function update(x) {
    const i = shards.findIndex((y) => y.id === x.id);
    if (i >= 0) shards[i] = x;
    view = view.map((y) => (y.id === x.id ? x : y));
    if (sel.has(x.id)) sel.set(x.id, x);
    const old = $(`m-${x.id}`), open = old && old.classList.contains("open");
    const was = old && old.contains(document.activeElement) ? (document.activeElement.dataset.act || "") : null;
    const fresh = old && rowOf(x, 0);
    if (old && fresh) {
      if (RiverCards.soundKey === `m:${x.id}`) RiverCards.stopSound();
      old.replaceWith(fresh);
      if (open) openRow(fresh, true, true);
      if (was != null) refocus(fresh, was);
    }
  }
  /* sharing asked first (unsharing isn't): the shard's name, what it means, OK or Cancel */
  const shareOk = (what, n = 1) => confirm({ title: `Share ${what}?`, ok: "OK",
    text: `Sharing your ${n === 1 ? "shard" : "shards"} publicly to the Community River: anyone can play ${n === 1 ? "it" : "them"}, send ${n === 1 ? "it" : "them"} to their FM-1 and favorite ${n === 1 ? "it" : "them"}, with your name, under CC BY 4.0.` });
  async function share(x) {
    const pub = x.visibility === "public";
    if (!pub && !(await shareOk(x.name))) return;
    try {
      const y = await A.post(`/api/shards/${x.id}/${pub ? "unshare" : "share"}`);
      update(y);
      toast(...(pub ? [h("b", null, y.name), " is out of the Community River: it stays yours."]
                    : [h("b", null, y.name), " drifts in the Community River now, with your name. ", h("a", { class: "link", href: `/#s=${y.id}` }, "See it there")]));
    } catch (e) { toast(e.message); }
  }
  async function remove(x) {
    const ok = await confirm({ title: `Delete ${x.name}?`, text: x.visibility === "public" ? "It leaves your shards and the Community River, and the favorites of those who starred it." : "It leaves your shards for good.", ok: "Delete it", danger: true });
    if (!ok) return;
    try {
      const at = view.findIndex((y) => y.id === x.id);
      await A.del(`/api/shards/${x.id}`);
      shards = shards.filter((y) => y.id !== x.id);
      sel.delete(x.id);
      paint();
      refocus($("list").querySelectorAll(".sr-item")[Math.max(0, at)] || $("list").querySelector(".sr-item"));
      toast(h("b", null, x.name), " is deleted.");
    } catch (e) { toast(e.message); }
  }

  // ---------------------------------------------------------------- the shards ticked: download, share, delete
  const today = (d = new Date()) => `${d.getFullYear()}-${two(d.getMonth() + 1)}-${two(d.getDate())}`;
  const slug = (t) => t.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "") || "shard";
  function save(blob, name) {
    const url = URL.createObjectURL(blob), a = h("a", { href: url, download: name });
    document.body.append(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 60000);
  }
  function downloadSel() {
    const list = [...sel.values()].map((x) => sceneOf(x)).filter(Boolean);
    try {
      const name = list.length === 1 ? `${slug(list[0].name)}.${FW.ext}scenes` : `frasil-shards-${today()}.${FW.ext}scenes`;
      save(new Blob([FB.packOf(list, FRASIL_VERSION)], { type: "application/json" }), name);
      toast(h("b", null, name), " is in your downloads: Quick Backup opens it.");
    } catch (e) { toast(e.message); }
  }
  async function shareSel() {
    const list = [...sel.values()].filter((x) => x.visibility === "private");
    if (!list.length) { toast("They are shared already."); return; }
    if (!(await shareOk(list.length === 1 ? list[0].name : `these ${list.length} shards`, list.length))) return;
    const done = [];
    try {
      for (const x of list) { update(await A.post(`/api/shards/${x.id}/share`)); done.push(x.name); }
      toast(`${listWords(done)} ${done.length === 1 ? "drifts" : "drift"} in the Community River now, with your name.`);
    } catch (e) { toast(...(done.length ? [`${listWords(done)} shared; then: `] : []), e.message); }
    finally { paintSel(); }
  }
  async function deleteSel() {
    const list = [...sel.values()], n = list.length, some = list.some((x) => x.visibility === "public");
    const ok = await confirm({ title: `Delete ${n === 1 ? list[0].name : `these ${n} shards`}?`, ok: "Delete them", danger: true,
                               text: some ? "They leave your shards for good; the shared ones leave the Community River too." : "They leave your shards for good." });
    if (!ok) return;
    let gone = 0;
    try { for (const x of list) { await A.del(`/api/shards/${x.id}`); gone++; } }
    catch (e) { toast(e.message); }
    finally {
      const ids = new Set(list.slice(0, gone).map((x) => x.id));
      shards = shards.filter((x) => !ids.has(x.id));
      sel.clear();
      paint();
      refocus($("list").querySelector(".sr-item"));
      if (gone === n) toast(n === 1 ? [h("b", null, list[0].name), " is deleted."] : `${n} shards deleted.`);
    }
  }

  // ---------------------------------------------------------------- the editor's SAVE AS and SAVE (river-edit.js)
  /* a new shard from one edited (one's own, or a favorite: its first author credited): in one's shards, opened */
  async function saveAs({ bytes, name, description }, x) {
    const y = await A.post("/api/shards", { name, description, layout: S.magic.shard, firmware: FRASIL_VERSION, data: A.b64(bytes), origin: x.id });
    shards.unshift(y);
    known = null;
    if (subject !== "shards") go("shards"); else paint();
    const art = $(`m-${y.id}`);
    if (art) openRow(art, true);
    toast(h("b", null, y.name), " is saved in your shards.");
    return art && art.querySelector(".nm-n");     // (the focus there, once the sheet is closed)
  }
  /* one's own shard, its sound replaced by the edited one */
  async function saveSound(x, bytes) {
    const y = await A.patch(`/api/shards/${x.id}`, { data: A.b64(bytes), layout: S.magic.shard, firmware: FRASIL_VERSION });
    update(y);
    known = null;
    toast(h("b", null, y.name), " is saved.");
  }

  // ---------------------------------------------------------------- a shard's name (as the FM-1 shows it) and its description
  let editing = null;
  function edit(x, words) {
    editing = x;
    const f = $("f-edit");
    f.elements.name.value = x.name;
    f.elements.description.value = x.description || "";
    f.querySelector(".form-say").textContent = "";
    $("edit-h").textContent = x.name;
    $("edit").showModal();
    if (words) f.elements.description.focus();
  }
  $("f-edit").elements.name.addEventListener("input", (e) => {
    const t = e.target, at = t.selectionStart;
    t.value = t.value.toUpperCase();
    t.setSelectionRange(at, at);
  });
  $("f-edit").addEventListener("submit", async (e) => {
    e.preventDefault();
    const f = e.target, say = f.querySelector(".form-say"), d = Object.fromEntries(new FormData(f));
    const name = d.name.trim().toUpperCase();
    if (!NAME.test(name)) { say.textContent = `A name: 1 to ${NL} characters, A to Z, 0 to 9, space - . # +`; return; }
    try {
      const y = await A.patch(`/api/shards/${editing.id}`, { name, description: d.description });
      $("edit").close();
      update(y);
      toast(h("b", null, y.name), " is saved.");
    } catch (err) { say.textContent = err.message; }
  });
  $("edit").addEventListener("click", (e) => { if (e.target === $("edit") || e.target.closest("[data-close]")) $("edit").close(); });

  // ================================================================ GET FROM FM-1: the FM-1 read, its slots as a checklist
  /* each slot, as the checklist has it: new (ticked to start), in one's River (marked), factory (unticked), empty
   * (dim), made by an earlier FRASIL (can't be a shard on its own) */
  const kindOf = (sc) => (sc.state === "empty" ? "empty" : sc.state === "factory" ? "factory" : !sc.current ? "old" : inRiver(sc) ? "had" : "new");
  const TAG = { new: "NEW", had: "IN YOUR RIVER", factory: "FACTORY", old: "EARLIER FRASIL", empty: "" };
  let G = null, grab = null;
  const picks = new Set();
  const G_T = "GET FROM FM-1";
  function openGet() {
    if (!D.hasMidi) { toast("Getting from the FM-1 needs Chrome or Edge on a computer, or an Android phone."); return; }
    if (D.busy) { toast("The FM-1 is busy with something else: wait a moment."); return; }
    if (!G) {
      G = D.sheet("get", { onClose: () => { if (!G.dlg.open) G.st = ""; } });   // (a close event comes late: opened again since?)
      G.closable = () => !D.busy || G.st === "reading";
    }
    if (!G.dlg.open) G.dlg.showModal();
    getRead();
  }
  async function getRead() {
    G.st = "reading";
    D.draw(G, { title: G_T, body: D.stage(D.drawing("reading"), { title: "READING THE FM-1", line: D.waterline(null), small: "To see what each of its sixteen slots holds." }),
                foot: [D.btn("Cancel", () => G.close())] });
    let fm = null;
    try { fm = await D.read(); }
    catch (e) {
      if (G.st !== "reading") return;
      const t = D.trouble(e);
      G.st = "trouble";
      D.draw(G, { title: G_T, body: D.stage(D.drawing(t.draw, { plate: t.plate }), { title: t.title, kind: "warn", text: t.text, small: t.small }),
                  foot: [t.install ? D.installBtn() : null, D.btn("Try again", getRead, { ice: true, id: "get-again" })] });
      return;
    }
    if (G.st !== "reading") return;   // (closed meanwhile)
    grab = fm;
    picks.clear();
    grab.scenes.forEach((sc, i) => { if (kindOf(sc) === "new") picks.add(i); });
    G.whole = true;
    getPick();
  }
  function getPick(said) {
    G.st = "pick";
    const row = (sc, i) => {
      const kind = kindOf(sc), can = kind === "new" || kind === "factory";
      const box = can && h("input", { type: "checkbox", checked: picks.has(i), "aria-label": `${sc.name}, slot ${i + 1}${kind === "factory" ? ", factory scene" : ""}` });
      const el = h(can ? "label" : "div", { class: `pk is-${kind}` + (picks.has(i) ? " is-on" : ""), "data-slot": i },
        box ? h("span", { class: "ck" }, box) : h("span"), h("span", { class: "no" }, two(i + 1)),
        h("span", { class: "sn" }, kind === "empty" ? "EMPTY" : sc.name), h("span", { class: "tag" }, TAG[kind]));
      if (box) box.addEventListener("change", () => { if (box.checked) picks.add(i); else picks.delete(i); el.classList.toggle("is-on", box.checked); getFoot(); });
      return el;
    };
    const whole = h("input", { type: "checkbox", id: "get-whole", checked: G.whole });
    G.note = h("input", { id: "get-note", class: "inp", maxlength: 200, placeholder: "A note: before the show, the new drones…", "aria-label": "A note for the whole backup", value: G.noteV || "" });
    const noteBox = h("div", { class: "opt-note" }, G.note);
    noteBox.hidden = !G.whole;
    whole.addEventListener("change", () => { G.whole = whole.checked; noteBox.hidden = !G.whole; getFoot(); });
    G.note.addEventListener("input", () => { G.noteV = G.note.value; });
    G.say = h("p", { class: "g-say", id: "get-say" });
    G.go = D.btn("Save", getSave, { ice: true, id: "get-save", icon: I.get(15) });
    D.draw(G, { title: G_T, focus: false,
      body: [h("div", { class: "g-lead" }, h("p", null, "Choose the scenes to keep as shards."), D.deviceLine(grab)),
        h("div", { class: "sls", role: "group", "aria-label": "The FM-1’s sixteen slots" }, h("div", null, grab.scenes.slice(0, 8).map((sc, i) => row(sc, i))),
          h("div", null, grab.scenes.slice(8).map((sc, i) => row(sc, i + 8)))),
        h("div", { class: "opt" }, h("label", { class: "ck" }, whole),
          h("div", null, h("label", { for: "get-whole" }, h("b", null, "Also store the whole backup")),
            h("p", null, "All sixteen slots and the settings, to restore later from any computer."), noteBox))],
      foot: [G.say, D.btn("Cancel", () => G.close()), G.go] });
    getFoot(said);
    (G.dlg.querySelector(".pk input") || G.go).focus({ preventScroll: true });
  }
  /* the button says exactly what will happen: Save 2 shards and the backup */
  function getFoot(said) {
    const list = [...picks].sort((a, b) => a - b).map((i) => grab.scenes[i]), n = list.length, names = list.map((sc) => h("b", null, sc.name));
    const shardsW = n ? `Save ${RL.plural(n, "shard", "shards")}` : "Save the backup";
    const label = n && G.whole ? `${shardsW} and the backup` : n ? shardsW : G.whole ? "Save the backup" : "Save";
    fill(G.go.querySelector("span"), n && G.whole ? [shardsW, h("span", { class: "long" }, " and the backup")] : label);
    G.go.setAttribute("aria-label", label);
    G.go.disabled = (!n && !G.whole) || D.busy;
    const named = names.flatMap((b, j) => [j ? (j === names.length - 1 ? " and " : ", ") : "", b]);
    fill(G.say, ...(said ? [said] : n && G.whole ? [...named, ", and the backup."] : n ? [...named, "."] : G.whole ? ["The backup only."] : ["Choose the scenes to keep, or the backup."]));
    G.say.classList.toggle("is-err", !!said);
  }
  async function getSave() {
    const list = [...picks].sort((a, b) => a - b).map((i) => grab.scenes[i]), whole = G.whole, note = (G.note.value || "").trim();
    if (!list.length && !whole) return;
    G.go.disabled = true;
    G.st = "saving";
    const done = [];
    try {
      for (const sc of list) {
        fill(G.say, `Saving ${sc.name}…`);
        await A.post("/api/shards", { layout: S.magic.shard, firmware: FRASIL_VERSION, data: A.b64(sc.bytes), from_slot: sc.slot });
        done.push(sc);
      }
      if (whole) { fill(G.say, "Storing the backup…"); await A.post("/api/backups", { image: A.b64(grab.image), note, firmware: FRASIL_VERSION }); }
    } catch (e) {
      await loadShards();
      done.forEach((sc) => picks.delete(sc.slot));
      getPick([done.length ? `${listWords(done.map((x) => x.name))} saved; then: ` : "", e.message]);
      return;
    } finally {
      if (done.length) loadShards();
      if (whole) loadBackups();
    }
    G.noteV = "";
    G.st = "done";
    const names = done.map((sc) => RiverCards.k(sc.name)).flatMap((b, j) => [j ? (j === done.length - 1 ? " and " : ", ") : "", b]);
    const where = done.length ? "shards" : "backups";
    D.draw(G, { title: G_T, size: "dlg-sm",
      body: h("div", { class: "dlg-stage" }, done.length ? RiverCards.shard(done[0], 46, true) : null, D.stage(null, { title: "SAVED", kind: "ice",
        text: done.length ? [...names, done.length === 1 ? " is in your shards" : " are in your shards", whole ? `, and the backup of ${D.clock(grab.at)} is stored.` : "."]
                          : `The backup of ${D.clock(grab.at)} is stored.` }).lastChild),
      foot: [D.btn("Close", () => G.close()), D.btn("See them", () => { G.close(); go(where, true); }, { ice: true, id: "get-see" })] });
  }
  document.querySelectorAll("[data-get]").forEach((b) => b.addEventListener("click", () => openGet()));

  // ================================================================ BACKUPS
  const stamp = (iso) => { const d = new Date(iso); return { day: A.day(iso), time: D.clock(d), file: today(d) }; };
  function paintBackups() {
    const box = $("my-backups");
    if (!backups.length) { box.replaceChildren(h("p", { class: "list-none" }, "No backups stored yet.")); return; }
    box.replaceChildren(...backups.map((b) => {
      const t = stamp(b.created_at), names = (b.scenes || []).map((nm, i) => nm && [two(i + 1), nm]).filter(Boolean);
      const old = (b.scenes || []).every((nm) => nm === null) && b.size > 64;
      const said = old ? "Made by an earlier FRASIL: brought up to date when restored"
                       : names.length ? RL.plural(names.length, "shard", "shards") + " saved" : "Nothing saved in its slots: the factory’s scenes";
      return h("div", { class: "bu", role: "listitem", id: `b-${b.id}` },
        h("div", { class: "bu-when" }, h("b", null, t.day.toUpperCase()), h("span", null, t.time)),
        h("div", { class: "bu-what" },
          h("p", { class: "bu-note" + (b.note ? "" : " none") }, b.note || "No note"),
          names.length ? h("p", { class: "bu-names" }, names.map(([no, nm], j) => [j ? h("span", { class: "sep", "aria-hidden": "true" }, "·") : null, h("i", null, no), " ", nm])) : null,
          h("p", { class: "bu-meta" }, [said, b.firmware && `FRASIL ${b.firmware}`, `${(b.size / 1000).toFixed(1)} kB`].filter(Boolean).join(" · "))),
        h("div", { class: "bu-acts" },
          RL.act(I.send(15), "Restore to FM-1", () => restore(b, t), { of: `the backup of ${t.day}, ${t.time}`, act: "restore" }),
          RL.act(I.dl(), "Download", () => download(b, t), { of: `the backup of ${t.day}, ${t.time}`, act: "download" }),
          RL.act(null, "Delete", () => removeBackup(b, t), { txt: true, of: `the backup of ${t.day}, ${t.time}`, act: "delete" })));
    }));
  }
  const imageOf = async (b) => A.unb64((await A.get(`/api/backups/${b.id}`)).image);
  async function download(b, t) {
    try { save(new Blob([await imageOf(b)], { type: "application/octet-stream" }), `frasil-backup-${t.file}.${FW.ext}`); }
    catch (e) { toast(e.message); }
  }
  async function restore(b, t) {
    if (!D.hasMidi) { toast("Restoring needs Chrome or Edge on a computer, or an Android phone."); return; }
    try { D.restore(await imageOf(b), `The backup of ${t.day}, ${t.time}`); }
    catch (e) { toast(e.message); }
  }
  async function removeBackup(b, t) {
    if (!(await confirm({ title: "Delete this backup?", text: `The backup of ${t.day}, ${t.time}, leaves Your Private River for good.`, ok: "Delete it", danger: true }))) return;
    try { await A.del(`/api/backups/${b.id}`); backups = backups.filter((x) => x.id !== b.id); paint(); toast("The backup is deleted."); }
    catch (e) { toast(e.message); }
  }

  // ================================================================ start
  readHash();
  A.providers().then((p) => { $("auth-google").hidden = !p.google; });
  A.me().then((u) => {
    if (!u && A.offline) V.say(["The River’s service can’t be reached right now. ", A.offline.message], "err");
    show(u);
    const token = kept.get("river-verify");
    if (u && token) verify(token);
  });
})();
