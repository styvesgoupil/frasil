/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's lists (Your Private River, the Community River): what both pages build around RiverCards.row.
 *   RiverList.title({tabs, sub, dock})   a page's title, as the FM-1 titles its pages: the subject in tracked capitals
 *                                        with its count, its siblings small beside it, a waterline under it
 *   RiverList.dock({get, send})          GET FROM FM-1 | SEND TO FM-1, the cable's two directions: {el, count(n)}
 *   RiverList.toolbar({filters, sort, st, onChange})   once a list is long: a search over names and descriptions,
 *                                        the filters in the FM-1's words, a sort -> {el, st, n(text)}
 *   RiverList.head(cols) · selbar(n, acts, clear) · end({shown, total, more, note})
 *   RiverList.FILTER (KEY, SCALE, GLINT, BASS, SHARED) · matches(x, st) · sorted(list, sort, at) · SORTS
 *   RiverList.act(icon, word, onclick, o) · send(scenes, o) (river-device.js's dialog) · copyLink(x) · report(x)
 *   RiverList.favButton(x, done)
 *   RiverList.intro(x, o) · meta(x, o) (an opened card's words) · day(iso) (a row's date) · firstLine(text)
 * What people typed is only ever text. */
"use strict";
window.RiverList = (() => {
  const { h, fill, two, I, toast } = RiverCards;
  const A = RiverAPI, D = RiverDevice, S = FB.S, ID = S.ids;
  const CC = "https://creativecommons.org/licenses/by/4.0/";
  const MON = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
  const PAGE = 24;

  /* a row's date: 23 OCT (another year's: OCT 2025) */
  function day(iso) {
    if (!iso) return "";
    const d = new Date(iso);
    return d.getFullYear() === new Date().getFullYear() ? `${two(d.getDate())} ${MON[d.getMonth()]}` : `${MON[d.getMonth()]} ${d.getFullYear()}`;
  }
  const shortDay = (iso) => (iso ? new Date(iso).toLocaleDateString("en-GB", { day: "numeric", month: "short" }) : "");
  const firstLine = (t) => (t || "").split("\n").map((x) => x.trim()).find(Boolean) || "";
  const plural = (n, one, many) => `${n} ${n === 1 ? one : many}`;

  // ---------------------------------------------------------------- the title
  /* tabs [{label, n, href, on, onclick, h1}]: the subject large, its siblings small (links to them); sub: one sentence;
   * dock: at its right */
  function title({ tabs, sub, dock }) {
    const box = h("div", { class: "pt-tabs" });
    for (const x of tabs) {
      const kids = [x.label, x.n != null ? [" ", h("span", { class: "n" }, String(x.n))] : null];
      const cls = "pt-t" + (x.on ? " is-on" : "");
      box.append(x.href ? h("a", { class: cls, href: x.href, "aria-current": x.on ? "true" : null, onclick: x.onclick || null }, kids)
                        : h(x.h1 ? "h1" : "p", { class: cls, id: x.id || null }, kids));
    }
    return h("div", { class: "pt" }, box, sub ? h("p", { class: "pt-sub" }, sub) : null, dock ? h("div", { class: "pt-acts" }, dock) : null);
  }

  // ---------------------------------------------------------------- the dock: the cable's two directions
  function dock({ get, send }) {
    const words = h("span", null, "Send to FM-1");
    const sb = h("button", { type: "button", class: "dock-b", id: "dock-send", onclick: send }, I.send(), words);
    const el = h("div", { class: "dock", role: "group", "aria-label": "Your FM-1" },
      h("button", { type: "button", class: "dock-b", id: "dock-get", onclick: get }, I.get(), h("span", null, "Get from FM-1")), sb);
    return {
      el,
      count(n) {              // the shards ticked: SEND ③ TO FM-1, ice
        sb.classList.toggle("is-hot", n > 0);
        fill(words, ...(n ? ["Send ", h("span", { class: "cnt" }, String(n)), " to FM-1"] : ["Send to FM-1"]));
      },
    };
  }

  // ---------------------------------------------------------------- the toolbar
  const FILTER = {
    key: { key: "root", label: "KEY", options: S.notes.map((w, i) => [String(i), w]) },
    scale: { key: "scale", label: "SCALE", options: S.scales.map((w, i) => [String(i), w]) },
    glint: { key: "type", label: "GLINT", options: S.params[ID.P_SQ_TYPE].texts.map((w, i) => [String(i), w]) },
    bass: { key: "bass", label: "BASS", options: [["seq", "SEQ"], ["none", "NO SEQ"]] },
    shared: { key: "shared", label: "SHARED", options: [["yes", "SHARED"], ["no", "NOT SHARED"]] },
  };
  const SORTS = [["new", "NEWEST"], ["old", "OLDEST"], ["name", "NAME"], ["key", "KEY"]];
  /* filters [FILTER.x]; sort {seg (three words side by side) or a menu, options [[value, word]]}; st {q, f, sort}:
   * changed in place, then onChange(st) */
  function toolbar({ filters, sort, st, onChange }) {
    const q = h("input", { type: "search", maxlength: 100, placeholder: "Find a shard: a name, a word", autocomplete: "off", spellcheck: "false",
                           "aria-label": "Find a shard: a word of its name or its description" });
    q.value = st.q || "";
    let typing = 0;
    const typed = () => { clearTimeout(typing); const v = q.value.trim().slice(0, 100); if (v !== st.q) { st.q = v; onChange(st); } };
    q.addEventListener("input", () => { clearTimeout(typing); typing = setTimeout(typed, 260); });
    q.addEventListener("keydown", (e) => { if (e.key === "Enter") { e.preventDefault(); typed(); } });
    const pick = (label, options, value, set, lit) => {
      const sel = h("select", { class: "f-v", "aria-label": label === "SORT" ? "Order" : label }, options.map(([v, w]) => h("option", { value: v }, w)));
      sel.value = value;
      const paint = () => sel.classList.toggle("is-set", lit && sel.value !== "");
      paint();
      sel.addEventListener("change", () => { set(sel.value); paint(); onChange(st); });
      return h("label", { class: "f" }, h("span", { class: "f-l", "aria-hidden": "true" }, label), sel, I.dn());
    };
    st.f = st.f || {};
    const fs = h("div", { class: "tb-fs" }, filters.map((f) => pick(f.label, [["", "ANY"], ...f.options], st.f[f.key] || "", (v) => { st.f[f.key] = v; }, true)));
    let order;
    if (sort.seg) {
      order = h("div", { class: "tb-sort", role: "group", "aria-label": "Order" }, sort.options.map(([v, w]) =>
        h("button", { type: "button", "data-sort": v, "aria-pressed": String(v === st.sort), onclick: (e) => {
          if (st.sort === v) return;
          st.sort = v;
          order.querySelectorAll("button").forEach((b) => b.setAttribute("aria-pressed", String(b === e.currentTarget)));
          onChange(st);
        } }, w)));
    } else order = pick("SORT", sort.options, st.sort, (v) => { st.sort = v; }, false);
    const n = h("span", { class: "tb-n", role: "status" });
    const el = h("div", { class: "tb", role: "search" }, h("label", { class: "tb-q" }, I.q(), q), fs, h("div", { class: "tb-end" }, n, order));
    return { el, st, n: (t) => { n.textContent = t; } };
  }
  /* one's own lists, filtered here: KEY, SCALE, GLINT's TYPE, BASS's sequence, SHARED; q: each word in the name or
   * the description (as the service's search) */
  function matches(x, st) {
    const f = st.f || {};
    if (f.root && x.root !== +f.root) return false;
    if (f.scale && x.scale !== +f.scale) return false;
    if (f.type && x.type !== +f.type) return false;
    if (f.bass && (f.bass === "seq") !== !!x.bseq) return false;
    if (f.shared && (f.shared === "yes") !== (x.visibility === "public")) return false;
    if (st.q) {
      const t = `${x.name}\n${x.description || ""}`.toLowerCase();
      if (!st.q.toLowerCase().split(/\s+/).every((w) => t.includes(w))) return false;
    }
    return true;
  }
  /* new (the default), old, name, key; at: the date the list counts from */
  function sorted(list, sort, at = "created_at") {
    const a = [...list], t = (x) => Date.parse(x[at] || "") || 0, key = (x) => (x.root ?? 0) * 100 + (x.scale ?? 0);
    if (sort === "old") a.sort((p, q) => t(p) - t(q));
    else if (sort === "name") a.sort((p, q) => p.name.localeCompare(q.name));
    else if (sort === "key") a.sort((p, q) => key(p) - key(q) || p.name.localeCompare(q.name));
    else a.sort((p, q) => t(q) - t(p));
    return a;
  }

  // ---------------------------------------------------------------- the column labels; the selection's bar; the end
  /* cols: [null | {t, sort (its arrow: ↓ or ↑), r (to the right), star}] */
  const head = (cols) => h("div", { class: "lh", "aria-hidden": "true" }, cols.map((c) => (c ? h("span", {
    class: [c.sort ? "is-sort" : "", c.r ? "r" : ""].join(" ").trim() || null }, c.star ? I.star(11) : c.t, c.sort ? " " + c.sort : "") : h("span"))));
  const selbar = (n, acts, clear) => h("div", { class: "selbar", role: "group", "aria-label": `${n} selected` },
    h("b", null, `${n} selected`), h("span", { class: "sep", "aria-hidden": "true" }), acts,
    h("button", { type: "button", class: "a a-txt sb-clear", onclick: clear }, "Clear"));
  /* "24 of 100" and SHOW 24 MORE; or one quiet line (note) */
  function end({ shown, total, more, note }) {
    const left = shown < total;
    return h("div", { class: "list-end" }, note ? h("p", null, note) : left ? h("p", null, `${shown} of ${total}`) : null,
      left && more ? h("button", { type: "button", class: "more", id: "more", onclick: more }, `Show ${Math.min(PAGE, total - shown)} more`) : null);
  }

  // ---------------------------------------------------------------- what to do with a shard
  /* an action in a card's head: o {ice (the main one), on (a toggle's state), txt (plain text), of (what it acts on:
   * its name is "Word: what", the visible word first), act (its kind: the same action found again once its row is
   * drawn again)} */
  function act(icon, word, onclick, o = {}) {
    return h("button", { type: "button", class: "a" + (o.ice ? " a-ice" : "") + (o.on ? " a-on" : "") + (o.txt ? " a-txt" : ""), "data-act": o.act || null,
      "aria-label": o.of ? `${word}: ${o.of}` : null, "aria-pressed": o.on == null ? null : String(!!o.on), onclick: onclick || null }, icon || null, h("span", null, word));
  }
  const NO_MIDI = "Sending needs Chrome or Edge on a computer, or an Android phone.";
  /* scenes into the FM-1's slots: SEND TO FM-1 (river-device.js; none: o.choose, a first step to pick them) */
  function send(list, o = {}) {
    if (!D.hasMidi) { toast(NO_MIDI); return; }
    if (D.busy) { toast("The FM-1 is busy with something else: wait a moment."); return; }
    if (!list.length && !o.choose) return;
    D.place(list, o);
  }
  async function copyLink(x) {
    const link = `${location.origin}/#s=${x.id}`;
    try { await navigator.clipboard.writeText(link); toast("The link is copied: ", h("span", { class: "mono" }, link)); }
    catch (_) { toast("The link: ", h("span", { class: "mono" }, link)); }
  }
  /* a report: what is wrong with it (once per reporter) */
  let rep = null, reporting = null;
  function reportDialog() {
    if (rep) return rep;
    const why = h("textarea", { id: "report-why", maxlength: 500, rows: 4, required: true });
    const say = h("p", { class: "form-say", id: "report-say", role: "alert" });
    const go = h("button", { class: "btn btn-ice", type: "submit", id: "report-send" }, "Send the report");
    const head = h("h2", { class: "sheet-h", id: "report-h" }, "What is wrong with it?");
    const form = h("form", { class: "sheet-in", id: "report-form", novalidate: true },
      h("header", { class: "sheet-head" }, h("p", { class: "label" }, "Report a shard"),
        h("button", { class: "sheet-x", type: "button", "data-close": "", "aria-label": "Close" }, I.x())),
      head, h("p", { class: "sheet-lede" }, "The River’s moderators read every report. Say what is wrong: a name or a description that shouldn’t be here, someone else’s work shared as theirs, anything else."),
      h("label", { class: "field" }, h("span", { class: "field-l" }, "The reason"), why), say,
      h("footer", { class: "sheet-foot" }, h("button", { class: "btn", type: "button", "data-close": "" }, "Cancel"), go));
    const dlg = h("dialog", { class: "sheet sheet-sm", id: "report", "aria-labelledby": "report-h" }, form);
    dlg.addEventListener("click", (e) => { if (e.target === dlg || e.target.closest("[data-close]")) dlg.close(); });
    form.addEventListener("submit", async (e) => {
      e.preventDefault();
      const reason = why.value.trim();
      if (!reason) { say.textContent = "Say what is wrong with it."; return; }
      go.disabled = true;
      try {
        await A.post(`/api/public/shards/${reporting.id}/report`, { reason });
        dlg.close();
        toast("Thank you: the River’s moderators will look at it.");
      } catch (err) { say.textContent = err.message; }
      finally { go.disabled = false; }
    });
    document.body.append(dlg);
    rep = { dlg, why, say, head };
    return rep;
  }
  function report(x) {
    const r = reportDialog();
    reporting = x;
    r.head.textContent = `What is wrong with ${x.name}?`;
    r.why.value = "";
    r.say.textContent = "";
    r.dlg.showModal();
  }
  /* FAVORITE: a bookmark to someone else's shard, not a copy (signed out: the page's invitation, #invite); done(x)
   * with x.favorited and x.favorites as the service says now */
  function favButton(x, done) {
    const b = act(I.star(15, !!x.favorited), "Favorite", null, { on: !!x.favorited, of: x.name, act: "fav" });
    b.addEventListener("click", async () => {
      if (!(await A.me())) { const inv = document.getElementById("invite"); if (inv) inv.showModal(); else location.href = "/yours/#signin"; return; }
      b.disabled = true;
      try {
        const r = x.favorited ? await A.del(`/api/favorites/${x.id}`) : await A.post(`/api/favorites/${x.id}`);
        x.favorited = r.favorited;
        x.favorites = r.favorites;
        b.classList.toggle("a-on", x.favorited);
        b.setAttribute("aria-pressed", String(x.favorited));
        b.querySelector("svg").replaceWith(I.star(15, x.favorited));
        toast(...(x.favorited ? [h("b", null, x.name), " is in your favorites. ", h("a", { class: "link", href: "/yours/#favorites" }, "See them")]
                              : [h("b", null, x.name), " left your favorites."]));
        if (done) done(x);
      } catch (e) { toast(e.message); }
      finally { b.disabled = false; }
    });
    return b;
  }

  // ---------------------------------------------------------------- an opened card's words
  /* one quiet line: who, when, the licence, the firmware, and only the counts that aren't zero. mine: one's own (its
   * slot of origin; shared or not); else someone else's (shared false: a favorite its author took back) */
  function meta(x, { mine = false } = {}) {
    const parts = [], lic = () => h("a", { class: "lic", href: CC }, "CC BY 4.0");
    const pub = mine ? x.visibility === "public" : x.shared !== false;
    if (mine) {
      if (x.origin_author) parts.push(["by ", h("b", null, x.origin_author), `, saved from the Community River on ${A.day(x.created_at)}`]);
      else parts.push(`Added ${A.day(x.created_at)}` + (x.from_slot != null ? ` from slot ${two(x.from_slot + 1)} of your FM-1` : ""));
      if (x.firmware) parts.push(`FRASIL ${x.firmware}`);
      if (pub) parts.push([`shared in the Community River on ${shortDay(x.shared_at)} under `, lic()]);
      if (x.visibility === "hidden") parts.push("hidden by the River’s moderators: it stays yours, out of the River");
    } else {
      const by = ["by ", h("b", null, x.author || "someone")];
      if (x.origin_author && x.origin_author !== x.author) by.push(", after ", h("b", null, x.origin_author));
      parts.push(by);
      parts.push(pub ? [`shared ${A.day(x.shared_at)} under `, lic()] : h("span", { class: "gone" }, "no longer shared"));
      if (x.firmware) parts.push(`FRASIL ${x.firmware}`);
    }
    const counts = [x.plays ? plural(x.plays, "play", "plays") : "", x.sends ? `sent to ${plural(x.sends, "FM-1", "FM-1s")}` : "",
                    x.favorites ? plural(x.favorites, "favorite", "favorites") : ""].filter(Boolean);
    if (counts.length) parts.push(counts.join(", "));
    return h("p", { class: "oc-meta" }, parts.map((p, i) => [i ? [" ", h("span", { class: "dot", "aria-hidden": "true" }, "·"), " "] : null, p]));
  }
  /* a shard has a name and a description, nothing more; one's own without words: "No description. Add one" */
  function intro(x, { mine = false, add = null } = {}) {
    const d = (x.description || "").trim();
    return [d ? h("p", { class: "oc-desc" }, d)
              : mine ? h("p", { class: "oc-none" }, "No description. ", h("button", { type: "button", class: "oc-add", onclick: add }, "Add one")) : null,
            meta(x, { mine })];
  }

  return { PAGE, FILTER, SORTS, day, firstLine, plural, title, dock, toolbar, matches, sorted, head, selbar, end,
           act, send, copyLink, report, favButton, meta, intro, NO_MIDI };
})();
