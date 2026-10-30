/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * FRASIL's admin page: /api/admin/overview (the accounts, the totals), /api/admin/users/<id> (an account's uploads,
 * opened under its row), /api/admin/waitlist (the two lists). Everything users typed is shown as text. */
"use strict";
(() => {
  const $ = (s, r = document) => r.querySelector(s);
  const h = (tag, props = {}, ...kids) => {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(props)) {
      if (v === null || v === undefined || v === false) continue;
      if (k === "class") el.className = v;
      else if (k === "text") el.textContent = v;
      else if (k.startsWith("on")) el.addEventListener(k.slice(2), v);
      else el.setAttribute(k, v === true ? "" : v);
    }
    for (const c of kids.flat()) if (c !== null && c !== undefined && c !== false) el.append(c);
    return el;
  };
  const pad = (n) => String(n).padStart(2, "0");
  const when = (iso, time = false) => {
    if (!iso) return "";
    const d = new Date(iso);
    const day = `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
    return time ? `${day} ${pad(d.getHours())}:${pad(d.getMinutes())}` : day;
  };
  const size = (n) => (n >= 1024 ? `${(n / 1024).toFixed(1)} KB` : `${n} B`);
  const state = $("[data-state]");
  const say = (t) => { state.textContent = t; state.hidden = !t; };

  async function get(path) {
    const r = await fetch(path, { cache: "no-store", credentials: "same-origin" });
    if (r.status === 404) throw new Error("No access.");
    if (!r.ok) throw new Error(`The API answered ${r.status}.`);
    return r.json();
  }

  // ------------------------------------------------------------ the totals
  function totals(o) {
    const t = o.totals, w = o.waitlist;
    const items = [["Accounts", t.users], ["Verified", t.verified], ["Shards", t.shards], ["Public", t.public],
      ["Backups", t.backups], ["iPad list", w.ipad], ["VST list", w.vst]];
    const dl = $("[data-totals]");
    dl.replaceChildren(...items.map(([k, v]) => h("div", {}, h("dt", { text: k }), h("dd", { text: String(v) }))));
    dl.hidden = false;
  }

  // ------------------------------------------------------------ the accounts, an account's uploads under its row
  const COLS = 8;
  let users = [];
  function userRow(u) {
    const name = h("button", { class: "adm-open", type: "button", "aria-expanded": "false" },
      h("span", { text: u.display_name }),
      u.admin && h("i", { class: "tag ice", text: "admin" }), u.banned && h("i", { class: "tag warn", text: "banned" }));
    const tr = h("tr", { class: "adm-u" },
      h("th", { scope: "row" }, name),
      h("td", { class: "mono" }, u.email, !u.verified && h("i", { class: "tag dim", text: "unverified" })),
      h("td", { class: "mono", title: when(u.created_at, true), text: when(u.created_at) }),
      h("td", { text: u.sign_in === "google" ? "Google" : "Password" }),
      h("td", { class: "n", text: String(u.shards.public) }),
      h("td", { class: "n", text: String(u.shards.private + u.shards.hidden) }),
      h("td", { class: "n", text: String(u.backups) }),
      h("td", { class: "n", text: String(u.favorites) }));
    tr.addEventListener("click", () => toggle(tr, name, u));
    return tr;
  }
  async function toggle(tr, btn, u) {
    const next = tr.nextElementSibling;
    if (next && next.classList.contains("adm-d")) {
      next.remove();
      btn.setAttribute("aria-expanded", "false");
      tr.classList.remove("on");
      return;
    }
    btn.setAttribute("aria-expanded", "true");
    tr.classList.add("on");
    const cell = h("td", { colspan: String(COLS) }, h("p", { class: "adm-note", text: "Loading…" }));
    tr.after(h("tr", { class: "adm-d" }, cell));
    try {
      const d = await get(`/api/admin/users/${encodeURIComponent(u.id)}`);
      cell.replaceChildren(detail(d));
    } catch (e) {
      cell.replaceChildren(h("p", { class: "adm-note warn", text: e.message }));
    }
  }
  function detail(d) {
    const shards = d.shards.length
      ? h("table", { class: "adm-t adm-sub" },
        h("thead", {}, h("tr", {}, ["Shard", "Created", "Shared", "", "Plays", "Sends", "Saves", "Favs"].map((t, i) =>
          h("th", { scope: "col", class: i > 3 ? "n" : null, text: t })))),
        h("tbody", {}, d.shards.map((s) => h("tr", {},
          h("th", { scope: "row" }, h("b", { text: s.name }), s.description && h("small", { text: s.description }),
            s.origin_author && h("small", { class: "dim", text: `from ${s.origin_author}` })),
          h("td", { class: "mono", title: when(s.created_at, true), text: when(s.created_at) }),
          h("td", { class: "mono", text: s.public ? when(s.shared_at) : "" }),
          h("td", {}, h("i", { class: `tag ${s.visibility === "public" ? "ice" : s.visibility === "hidden" ? "warn" : "dim"}`,
            text: s.visibility })),
          ...[s.plays, s.sends, s.saves, s.favorites].map((n) => h("td", { class: "n", text: String(n) }))))))
      : h("p", { class: "adm-note", text: "No shards." });
    const backups = d.backups.length
      ? h("ul", { class: "adm-backups", role: "list" }, d.backups.map((b) => h("li", {},
        h("span", { class: "mono", text: when(b.created_at, true) }), h("span", { text: b.note || "—" }),
        h("span", { class: "mono dim", text: [b.firmware && `FRASIL ${b.firmware}`, size(b.size)].filter(Boolean).join(" · ") }))))
      : h("p", { class: "adm-note", text: "No backups." });
    const u = d.user;
    return h("div", { class: "adm-detail" },
      h("p", { class: "adm-meta mono" }, `#${u.id} · joined ${when(u.created_at, true)}` +
        (u.verified ? ` · verified ${when(u.verified_at)}` : " · not verified")),
      h("h4", { text: `UPLOADS · ${d.shards.length}` }), shards,
      h("h4", { text: `BACKUPS · ${d.backups.length}` }), backups);
  }
  function showUsers() {
    const q = $("[data-find]").value.trim().toLowerCase();
    const shown = q ? users.filter((u) => (u.display_name + " " + u.email).toLowerCase().includes(q)) : users;
    const body = $("[data-users]");
    body.replaceChildren(...(shown.length ? shown.map(userRow)
      : [h("tr", {}, h("td", { colspan: String(COLS), class: "adm-note", text: users.length ? "No match." : "No accounts yet." }))]));
    $("[data-n-users]").textContent = q ? `${shown.length} / ${users.length}` : String(users.length);
  }
  $("[data-find]").addEventListener("input", showUsers);

  // ------------------------------------------------------------ the waitlists
  function lists(w) {
    for (const [k, l] of Object.entries(w.lists)) {
      const box = $(`[data-list="${k}"]`);
      if (!box) continue;
      $("[data-count]", box).textContent = String(l.count);
      $("[data-mails]", box).replaceChildren(...(l.items.length
        ? l.items.map((x) => h("li", {}, h("span", { class: "mono", text: x.email }),
          h("time", { class: "mono dim", datetime: x.created_at, title: when(x.created_at, true), text: when(x.created_at) })))
        : [h("li", { class: "adm-note", text: "Nobody yet." })]));
      const btn = $("[data-copy]", box);
      btn.disabled = !l.items.length;
      btn.onclick = () => copy(btn, l.items.map((x) => x.email).join("\n"));
    }
  }
  async function copy(btn, text) {
    const was = btn.textContent;
    try {
      await navigator.clipboard.writeText(text);
    } catch {
      const t = h("textarea", { class: "sr-only", readonly: true });
      t.value = text;
      document.body.append(t);
      t.select();
      document.execCommand("copy");
      t.remove();
    }
    btn.textContent = "Copied";
    setTimeout(() => (btn.textContent = was), 1400);
  }

  // ------------------------------------------------------------ all of it
  async function load() {
    say("Loading…");
    try {
      const [o, w] = await Promise.all([get("/api/admin/overview"), get("/api/admin/waitlist")]);
      users = o.users;
      totals(o);
      showUsers();
      lists(w);
      $("[data-accounts]").hidden = $("[data-lists]").hidden = false;
      say("");
    } catch (e) {
      say(e.message);
    }
  }
  $("[data-reload]").addEventListener("click", load);
  load();
})();
