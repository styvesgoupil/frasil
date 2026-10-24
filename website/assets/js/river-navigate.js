/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The Community River (pages/river/navigate.html, the River's front page): every shard shared, from the service's
 * public River (GET /api/public/shards: 24 a page; NEWEST by default, MOST PLAYED and FAVORITES one click away; KEY,
 * SCALE, GLINT, BASS; words in names and descriptions), as rows on the river (RiverCards.row): ▶ (counted:
 * …/played), the author, the favorites; opened in place, its head the one action zone: Send to FM-1 (the slots chosen,
 * the FM-1 read first: …/sent once sent), Favorite (a bookmark: /api/favorites; signed out, an invitation), Copy link,
 * Report. One's own shards there: Send and Copy link only (drawn first, then their Favorite and Report taken out once
 * one's own ids are known). #s=ID: one shard, shared by a link, first and opened; the address names the shard opened.
 * The links the service emails (#verify=, #reset=) and Google's way back (#google=) land here: Your River answers. */
"use strict";
(() => {
  if (!document.getElementById("gal")) return;
  if (/^#(verify|reset|google)=/.test(location.hash)) { location.replace("/yours/" + location.hash); return; }
  const { h, openRow, I } = RiverCards;
  const A = RiverAPI, D = RiverDevice, RL = RiverList, F = RL.FILTER;
  const $ = (id) => document.getElementById(id);
  const CC = "https://creativecommons.org/licenses/by/4.0/";
  const FILTERS = [F.key, F.scale, F.glint, F.bass];
  const SORTS = [["new", "Newest"], ["played", "Most played"], ["favorites", "Favorites"]];
  const V = D.voice($("voice"));

  // ---------------------------------------------------------------- the state, kept in the address (?sort=&q=&root=…)
  const url0 = new URLSearchParams(location.search);
  const st = { q: (url0.get("q") || "").slice(0, 100), f: {}, sort: SORTS.some(([v]) => v === url0.get("sort")) ? url0.get("sort") : "new" };
  for (const f of FILTERS) { const v = url0.get(f.key); if (v != null && f.options.some(([o]) => o === v)) st.f[f.key] = v; }
  const filtered = () => !!(st.q || FILTERS.some((f) => st.f[f.key]));
  function remember() {
    const q = new URLSearchParams();
    if (st.sort !== "new") q.set("sort", st.sort);
    if (st.q) q.set("q", st.q);
    for (const f of FILTERS) if (st.f[f.key]) q.set(f.key, st.f[f.key]);
    const qs = q.toString();
    history.replaceState(null, "", location.pathname + (qs ? "?" + qs : "") + location.hash);
  }
  let page = 0, pages = 1, total = 0, got = 0, riverTotal = null, tb = null;
  /* one's own shards (signed in): shown there without Favorite nor Report. The list doesn't wait for them: each row is
   * drawn at once, and once the ids are known, one's own lose those two (own(art)) */
  let ownIds = null;
  A.me().then((u) => (u ? A.get("/api/shards").then((r) => new Set(r.items.map((x) => x.id)), () => new Set()) : new Set()))
    .then((ids) => { ownIds = ids; document.querySelectorAll("#gal .sr-item, #one-card .sr-item").forEach((art) => own(art)); return ids; });
  function own(art) {
    if (!ownIds || !art._shard || !ownIds.has(art._shard.id)) return;
    art.querySelectorAll('.acts [data-act="fav"], .acts [data-act="report"]').forEach((b) => b.remove());
  }

  // ---------------------------------------------------------------- the title, the toolbar
  function paintTitle() {
    $("pt").replaceChildren(RL.title({ tabs: [{ label: "Shards", n: riverTotal, on: true }],
      sub: ["Scenes shared by FRASIL players under ", h("a", { href: CC }, "CC BY 4.0"), ": play them here, send them to your FM-1."] }));
  }
  function paintTools() {
    if (tb || !(riverTotal || filtered())) return;
    tb = RL.toolbar({ filters: FILTERS, sort: { seg: true, options: SORTS }, st, onChange: () => { remember(); load(false); } });
    $("tb").replaceChildren(tb.el);
  }
  function clearAll() {
    st.q = ""; st.f = {};
    tb = null;
    $("tb").replaceChildren();
    remember();
    load(false);
  }

  // ---------------------------------------------------------------- a shard of the River, as a row
  function rowOf(x, i, featured) {
    const sc = A.scene(x);
    if (!sc) return null;                        // (another FRASIL's layout: not for this page)
    const favN = h("span", { class: "fav-n" }, I.star(12), h("span", { class: "sr-only" }, " favorites: "), h("span", null, String(x.favorites || 0)));
    const send = RL.act(I.send(15), "Send to FM-1", () => RL.send([sc], { onSent: () => A.post(`/api/public/shards/${x.id}/sent`).catch(() => {}) }),
                        { ice: true, of: x.name, act: "send" });
    const link = RL.act(I.link(), "Copy link", () => RL.copyLink(x), { of: x.name, act: "link" });
    const art = RiverCards.row(sc, {
      id: (featured ? "one-" : "s-") + x.id, key: (featured ? "one:" : "s:") + x.id, i, class: "is-public",
      desc: RL.firstLine(x.description),
      cells: [h("span", { class: "by" }, h("span", { class: "sr-only" }, "by "), x.author || "someone"), favN, h("span", { class: "dt" }, RL.day(x.shared_at))],
      side: x.author || "someone",
      actions: [send, RL.favButton(x, (y) => {             // (its count in its row and in its line)
        favN.lastChild.textContent = String(y.favorites);
        const line = document.querySelector(`#${CSS.escape((featured ? "one-" : "s-") + x.id)} .oc-meta`);
        if (line) line.replaceWith(RL.meta(y));
      }), link, RL.act(null, "Report", () => RL.report(x), { txt: true, of: x.name, act: "report" })],
      intro: () => RL.intro(x),
      onPlay: () => A.post(`/api/public/shards/${x.id}/played`).catch(() => {}),
      onOpen: featured ? null : (open) => address(x, open),
    });
    art._shard = x;
    own(art);
    return art;
  }
  /* the address names the shard opened: opened, its #s=; closed, another one still open (or none) */
  function address(x, open) {
    const at = (id) => history.replaceState(null, "", location.pathname + location.search + (id ? `#s=${id}` : ""));
    if (open) { at(x.id); return; }
    if (location.hash !== `#s=${x.id}`) return;
    const other = [...gal.querySelectorAll(".sr-item.open")].find((art) => art._shard && art._shard.id !== x.id);
    at(other ? other._shard.id : null);
  }

  // ---------------------------------------------------------------- the River's shards, 24 at a time
  const gal = $("gal");
  let asking = 0;
  async function load(append) {
    const want = append ? page + 1 : 1, ask = ++asking;
    const q = new URLSearchParams({ sort: st.sort, page: want });
    if (st.q) q.set("q", st.q);
    for (const f of FILTERS) if (st.f[f.key]) q.set(f.key, st.f[f.key]);
    gal.setAttribute("aria-busy", "true");
    try {
      const r = await A.get(`/api/public/shards?${q}`);
      if (ask !== asking) return;
      V.say("", "");
      page = r.page; pages = r.pages; total = r.total;
      if (!filtered()) riverTotal = r.total;
      if (!append) { RiverCards.stopSound(); gal.replaceChildren(h("div", { class: "list-top" })); got = 0; }
      const rows = r.items.map((x, i) => rowOf(x, i, false)).filter(Boolean);
      got += r.items.length;
      gal.append(...rows);
      paint();
      if (append && rows[0]) rows[0].querySelector(".nm-n").focus({ preventScroll: true });
    } catch (e) {
      if (ask !== asking) return;
      V.say(["The River’s shards can’t be reached right now. ", e.message], "err");
    } finally {
      if (ask === asking) gal.removeAttribute("aria-busy");
    }
  }
  function paint() {
    paintTitle();
    paintTools();
    if (tb) tb.n(RL.plural(total, "shard", "shards"));
    const top = gal.querySelector(".list-top"), by = (s) => (st.sort === s ? "↓" : null);
    if (top) top.replaceChildren(...(gal.querySelector(".sr-item") ? [RL.head([null, { t: "NAME" }, { t: "WHAT PLAYS" }, { t: "KEY" }, { t: "GLINT" }, { t: "BASS" },
      { t: "AUTHOR" }, { star: true, r: true, sort: by("favorites") }, { t: "SHARED", r: true, sort: by("new") }])] : []));
    const note = total ? null : filtered() ? ["No shard matches. ", h("button", { type: "button", class: "txt-b", onclick: clearAll }, "Clear the filters"), "."]
      : ["The River is waiting for its first shards: get some from your FM-1 in ", h("a", { href: "/yours/" }, "Your Private River"), ", then share them."];
    $("list-end").replaceChildren(RL.end({ shown: Math.min(got, total), total, more: page < pages ? () => load(true) : null, note }));
  }

  // ---------------------------------------------------------------- one shard, from a link
  async function one() {
    const m = /^#s=([\w-]{1,40})$/.exec(location.hash);
    if (!m) { $("one").hidden = true; return; }
    try {
      const x = await A.get(`/api/public/shards/${m[1]}`), c = rowOf(x, 0, true);
      if (!c) throw new Error("This shard was made by another FRASIL: it can’t be shown here.");
      $("one-card").replaceChildren(c);
      $("one").hidden = false;
      openRow(c, true, true);
      $("one").scrollIntoView({ block: "start" });
    } catch (e) {
      $("one-card").replaceChildren(h("p", { class: "list-none" }, e.status === 404 ? "This shard isn’t in the River any more: its author took it back, or it was removed." : e.message));
      $("one").hidden = false;
    }
  }
  addEventListener("hashchange", () => { if (/^#s=/.test(location.hash) && !$("one").contains(document.activeElement)) one(); });
  for (const dlg of [$("invite")]) dlg.addEventListener("click", (e) => { if (e.target === dlg || e.target.closest("[data-close]")) dlg.close(); });

  paintTitle();
  if (filtered()) A.get("/api/public/shards?page=1").then((r) => { riverTotal = r.total; paintTitle(); paintTools(); }, () => {});
  one();
  load(false);
})();
