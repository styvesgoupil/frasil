/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The profile (pages/river/profile.html, /profile/), the header's name signed in (river-api.js [data-who]): the
 * account on the River's service (server/README.md), titled ACCOUNT (the header already shows the name), one column
 * of rows: the display name (public; CHANGE opens its field), the email, the password (CHANGE…), sign out (here,
 * everywhere), everything as a zip, the privacy notice, the account deleted. Signed out: the way to sign in. */
"use strict";
(() => {
  if (!document.getElementById("pf-out")) return;
  const { toast, confirm } = RiverCards;
  const A = RiverAPI;
  const $ = (id) => document.getElementById(id);
  const V = RiverDevice.voice($("voice"));

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
  /* a row's own form, opened by its action (CHANGE), closed by Cancel or once saved */
  function opener(btn, f) {
    const set = (open) => {
      f.hidden = !open;
      btn.hidden = open;
      btn.setAttribute("aria-expanded", String(open));
      if (open) { f.querySelector(".form-say").textContent = ""; const i = [...f.querySelectorAll("input")].find((x) => x.offsetParent !== null); if (i) i.focus(); }
    };
    $(btn.id).addEventListener("click", () => set(true));
    f.querySelector("[data-cancel]").addEventListener("click", () => { set(false); btn.focus(); });
    return set;
  }
  const nameOpen = opener($("name-go"), $("f-name")), passOpen = opener($("pass-go"), $("f-pass"));

  const since = (iso) => (iso ? new Date(iso).toLocaleDateString("en-GB", { month: "long", year: "numeric" }) : "");
  async function sub(u) {
    let words = since(u.created_at) ? `Since ${since(u.created_at)}` : "";
    try {
      const items = (await A.get("/api/shards")).items, shared = items.filter((x) => x.visibility === "public").length;
      words += `${words ? " · " : ""}${items.length} ${items.length === 1 ? "shard" : "shards"}${shared ? `, ${shared} shared` : ""}`;
    } catch (_) { /* (the date alone) */ }
    $("pf-sub").textContent = words;
  }
  function show(u) {
    $("pf-out").hidden = !!u;
    $("account").hidden = !u;
    $("pf-sub").textContent = "";
    if (!u) return;
    $("pf-name").textContent = u.display_name;
    $("acct-email").textContent = u.email;
    $("acct-verified").textContent = u.verified ? "Confirmed" : "Not confirmed";
    $("acct-verified").classList.toggle("no", !u.verified);
    $("acct-email-n").textContent = u.verified ? "Never shown to anyone." : "Never shown to anyone. The River sent a link to confirm it: you need it to share.";
    $("f-name").elements.display_name.value = u.display_name;
    $("pass-now").hidden = !u.has_password;
    $("pass-v").textContent = u.has_password ? "••••••••••" : "None yet: you sign in with Google";
    $("pass-p").textContent = u.has_password ? "Changing it signs out your other sessions." : "A password lets you sign in with your email too.";
    sub(u);
  }

  async function out(all) {
    try { await A.post("/api/auth/logout", { all: !!all }); } catch (_) { /* gone already */ }
    A.setMe(null);
    show(null);
    V.say(all ? "You are signed out everywhere." : "You are signed out.", "ok");
  }
  $("out-here").addEventListener("click", () => out(false));
  $("out-all").addEventListener("click", () => out(true));

  form("f-name", async (d) => {
    const u = await A.patch("/api/me", { display_name: d.display_name });
    A.setMe(u);
    show(u);
    nameOpen(false);
    $("name-go").focus();                    // (the form is gone: its row's own action)
    toast("Saved: the shards you share show it now.");
  });
  form("f-pass", async (d, say, f) => {
    if ((d.new_password || "").length < 8) throw new Error("A new password: 8 characters at least.");
    const u = await A.patch("/api/me", { password: d.password || null, new_password: d.new_password });
    A.setMe(u);
    f.reset();
    show(u);
    passOpen(false);
    $("pass-go").focus();
    toast("Your password is changed. Your other sessions ended.");
  });
  $("del-me").addEventListener("click", async () => {
    const ok = await confirm({ title: "Delete your account?", text: "Your account, your shards and your backups are deleted, for good. The shards you shared leave the Community River and others’ favorites; copies others saved before stay theirs, without your name.",
                               ok: "Delete my account", danger: true, check: "I understand: this can’t be undone." });
    if (!ok) return;
    try { await A.del("/api/me"); A.setMe(null); show(null); V.say("Your account is deleted. Thank you for the shards.", "ok"); }
    catch (e) { toast(e.message); }
  });

  A.me().then((u) => {
    if (!u && A.offline) V.say(["The River’s service can’t be reached right now. ", A.offline.message], "err");
    show(u);
  });
})();
