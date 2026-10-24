/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's service, from the page (server/README.md): same-origin JSON, the session's cookie (credentials
 * "same-origin"), every change as JSON (the service checks its Origin and its type); an error is the service's own
 * sentence (detail). What it returns that people typed is shown as text only (river-cards.js).
 *   RiverAPI.get(path) · post(path, body) · patch(path, body) · del(path)    -> the JSON; throws Error(detail) {status}
 *   RiverAPI.me(fresh) -> the account or null (GET /api/auth/session); onMe(fn) · setMe(me) · providers() -> {google}
 *   RiverAPI.scene(shard) -> the shard as a card's scene (its bytes read with the model; null if another layout's)
 *   RiverAPI.b64(bytes) · unb64(text) · day(iso)
 * And the River's shell on every page: who is signed in (the header's [data-who]: Sign in, to Your Private River's
 * form; signed in, the name, to the profile). */
"use strict";
window.RiverAPI = (() => {
  async function call(method, path, body) {
    const init = { method, credentials: "same-origin", headers: { Accept: "application/json" } };
    if (method !== "GET") {
      init.headers["Content-Type"] = "application/json";
      if (method !== "DELETE") init.body = JSON.stringify(body || {});
    }
    let r;
    try { r = await fetch(path, init); }
    catch (_) { throw Object.assign(new Error("The River can’t be reached right now. Check the connection, then try again."), { status: 0 }); }
    let j = null;
    try { j = await r.json(); } catch (_) { /* not JSON: said below */ }
    if (!r.ok || j === null) {
      const msg = j && typeof j.detail === "string" ? j.detail
                : r.status === 429 ? "Too many tries for now: wait a little, then try again."
                : r.ok || r.status === 404 && !path.includes("/shards/") ? "The River’s service isn’t answering here."
                : `The River answered ${r.status}.`;
      throw Object.assign(new Error(msg), { status: r.ok ? 503 : r.status });
    }
    return j;
  }

  // ---------------------------------------------------------------- who is here
  let mine = null, asked = null;
  const meFns = [];
  function setMe(u) {
    mine = u || null;
    asked = Promise.resolve(mine);
    document.querySelectorAll("[data-who]").forEach((a) => {
      a.textContent = mine ? mine.display_name : "Sign in";   // (and kept here, for the next page's header at once)
      a.href = mine ? "/profile/" : "/yours/#signin";
      a.classList.toggle("is-in", !!mine);
      a.setAttribute("aria-label", mine ? `Your profile: signed in as ${mine.display_name}` : "Sign in to FRASIL River");
    });
    try { if (mine) localStorage.setItem("river-me", mine.display_name); else localStorage.removeItem("river-me"); } catch (_) { /* (none: Sign in first) */ }
    meFns.forEach((f) => f(mine));
    return mine;
  }
  /* who this browser is (GET /api/auth/session: never a 401), and the ways to sign in */
  let prov = null;
  function me(fresh = false) {
    if (asked && !fresh) return asked;
    const q = call("GET", "/api/auth/session");
    prov = q.then((j) => ({ password: j.password, google: !!j.google }), () => ({ password: true, google: false }));
    asked = q.then((j) => setMe(j.me), (e) => { setMe(null); RiverAPI.offline = e; return null; });
    return asked;
  }
  const providers = () => prov || (me(), prov);

  // ---------------------------------------------------------------- shards and their bytes
  const b64 = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode(b); return btoa(s); };
  const unb64 = (t) => Uint8Array.from(atob(t), (c) => c.charCodeAt(0));
  /* a shard from the service -> a card's scene (its bytes read and checked by the model, as a scene package's) */
  function scene(x) {
    try {
      const pack = FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: x.layout, firmware: x.firmware,
                                                scenes: [{ name: x.name, from: (x.from_slot ?? 0) + 1, data: x.data }] }));
      return { ...pack.scenes[0], slot: x.from_slot ?? null, shard: x };
    } catch (_) { return null; }
  }
  const day = (iso) => (iso ? new Date(iso).toLocaleDateString("en-GB", { day: "numeric", month: "short", year: "numeric" }) : "");

  const RiverAPI = {
    get: (p) => call("GET", p), post: (p, b) => call("POST", p, b), patch: (p, b) => call("PATCH", p, b), del: (p) => call("DELETE", p),
    me, setMe, onMe: (f) => meFns.push(f), get signedIn() { return mine; }, providers, scene, b64, unb64, day, offline: null,
  };
  me();

  return RiverAPI;
})();
