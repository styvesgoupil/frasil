/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Perform's mockups: the show (show.js) with the river fed by what the engine played (data/perform-data.js: web_live's
 * frames, 30 a second) and the FM-1's gestures as they would come over MIDI, frame by frame, and a film of a hand on the
 * FM-1 (72 s, three factory scenes).
 *   Keys: 1-7 the frames, F full screen, H the bar hidden. */
"use strict";
(() => {
  const D = window.PERFORM_DATA, R = PerformRiver, Sh = PerformShow, S = Sh.S;
  const $ = (s) => document.querySelector(s);
  const h = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
  const scn = (name) => Sh.factory.find((f) => f.name === name);

  // ------------------------------------------------------------------ the frames the engine played
  let t0 = 0, offset = 0;
  function load(name, at = 0) {
    Sh.values(scn(name));
    t0 = performance.now();
    offset = at;
  }
  function frame(now) {
    const fr = D.live[S.scene], n = fr.length;
    const x = (((now - t0) / 1000 * 30 + offset) % n + n) % n, i = Math.floor(x) % n, u = x - Math.floor(x);
    const a = fr[i], b = fr[(i + 1) % n], f = a.slice();
    const lerp = (j, nj) => { if (a[nj] === b[nj]) f[j] = a[j] + (b[j] - a[j]) * u; };
    for (let v = 0; v < 8; v++) lerp(8 + v, v);
    for (let v = 0; v < 4; v++) { lerp(20 + v, 16 + v); if (a[16 + v] === b[16 + v] && b[48 + v] >= a[48 + v]) f[48 + v] = a[48 + v] + (b[48 + v] - a[48 + v]) * u; }
    lerp(25, 24);
    return f;
  }
  const scene = (name, at) => { load(name, at); Sh.scene(scn(name)); };

  // ------------------------------------------------------------------ the film: three scenes, a hand on the FM-1
  const FILM = [
    [0.2, () => scene("NORTH RIVER", 0)],
    [4.0, () => Sh.openPage("SOUND")],
    [4.9, () => Sh.turn("SOUND", 1, 44, 2400)],
    [9.6, () => Sh.openPage("CLOUD")],
    [10.5, () => Sh.turn("CLOUD", 1, 26, 2000)],
    [14.6, () => Sh.turn("DRONE MOTION", 0, -70, 2600)],
    [19.0, () => { S.v[156] = 1; Sh.button("FX", "FREEZE", "ON", "ice"); }],
    [22.2, () => { S.v[156] = 0; Sh.button("FX", "FREEZE", "OFF"); }],
    [24.2, () => scene("RESILIENCE", 0)],
    [28.0, () => Sh.keys("GLINT", [], "ARP · RANDOM · 1/16")],
    [33.2, () => Sh.turn("GLINT ARP", 1, -1, 900)],
    [37.4, () => Sh.turn("MIXER", 2, -36, 2000)],
    [42.6, () => { S.oct = 0; Sh.hud(); Sh.button("OCT+", "OCTAVE", "+0"); }],
    [48.2, () => scene("LONG NIGHT", 0)],
    [52.0, () => Sh.keys("DRONE", [33, 52], "A PHRYGIAN · FIFTH")],
    [56.4, () => Sh.turn("DRONE TONE", 1, 52, 2600)],
    [61.6, () => Sh.turn("SOUND", 2, 70, 2400)],
    [66.6, () => Sh.turn("LFO 1", 2, 40, 2000)],
  ];
  const FILM_LEN = 72;
  let film = null;
  function filmTick(now) {
    if (!film) return;
    const t = (now - film.t0) / 1000;
    while (film.next < FILM.length && FILM[film.next][0] <= t) FILM[film.next++][1]();
    if (t >= FILM_LEN) { film.t0 = now; film.next = 0; S.oct = -1; }
    $("#film-t").textContent = `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, "0")} / 1:12`;
  }

  // ------------------------------------------------------------------ the frames of the mockup
  const FRAMES = [
    ["CONNECT", () => { load("NORTH RIVER", 120); $("#connect").hidden = false; S.dimTo = 0.32; }],
    ["LIVE", () => load("NORTH RIVER", 190)],
    ["KNOB", () => { load("NORTH RIVER", 200); Sh.hold = true; Sh.knob("CLOUD", 1, 104); }],
    ["SCENE", () => { Sh.hold = true; scene("RESILIENCE", 60); }],
    ["KEYS", () => { load("RESILIENCE", 150); Sh.hold = true; Sh.keys("GLINT", [], "ARP · RANDOM · 1/16"); }],
    ["FREEZE", () => { load("NORTH RIVER", 260); S.v[156] = 1; Sh.hold = true; Sh.button("FX", "FREEZE", "ON", "ice"); }],
    ["FILM", () => { load("NORTH RIVER", 0); film = { t0: performance.now(), next: 0 }; }],
  ];
  let cur = 0;
  function go(i) {
    cur = i;
    film = null;
    Sh.hold = false;
    Sh.clear();
    S.oct = -1;
    $("#connect").hidden = true;
    $("#stage").dataset.frame = FRAMES[i][0].toLowerCase();
    FRAMES[i][1]();
    Sh.hud();
    document.querySelectorAll("#frames button").forEach((b, j) => b.classList.toggle("on", j === i));
    $("#film-t").hidden = FRAMES[i][0] !== "FILM";
    history.replaceState(null, "", `#${FRAMES[i][0].toLowerCase()}`);
  }

  // ------------------------------------------------------------------ the bar, keys, the loop
  const fb = $("#frames");
  FRAMES.forEach(([name], i) => { const b = h("button", "", `<kbd>${i + 1}</kbd>${name}`); b.onclick = () => go(i); fb.appendChild(b); });
  $("#full").onclick = () => (document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen());
  document.addEventListener("fullscreenchange", () => { document.body.classList.toggle("bare", !!document.fullscreenElement); Sh.fit(); });
  addEventListener("keydown", (e) => {
    if (e.metaKey || e.ctrlKey || e.altKey) return;
    const k = e.key.toLowerCase();
    if (k >= "1" && k <= String(FRAMES.length)) go(+k - 1);
    else if (k === "f") $("#full").onclick();
    else if (k === "h") { document.body.classList.toggle("bare"); Sh.fit(); }
  });
  const fromHash = () => {
    const m = /^#(\w+)/.exec(location.hash);
    return m ? FRAMES.findIndex(([n]) => n.toLowerCase() === m[1]) : -1;
  };
  addEventListener("hashchange", () => { const i = fromHash(); if (i >= 0) go(i); });
  const river = R.create($("#river"));
  function loop(now) {
    filmTick(now);
    const f = frame(now);
    Sh.strikes(f);
    Sh.tick(now, f);
    river.draw(f, Sh.look(), now);
    requestAnimationFrame(loop);
  }
  if (new URLSearchParams(location.search).has("bare")) document.body.classList.add("bare");
  const first = fromHash();
  Sh.fit();
  document.fonts.ready.then(() => {
    go(first >= 0 ? first : 1);
    requestAnimationFrame(loop);
  });
})();
