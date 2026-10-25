/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The landing's factory scenes (assemble.py factory_tunes: the rows, and their bytes in #tunes-data), each played live
 * by FRASIL's own engine (river-player.js: the Play page's app.wasm), one at a time: another one replaces it, the one
 * playing tapped again stops. The one sounding glows, its line a current carrying ice: crystals drifting downstream at
 * its tempo, the current as tall as it is loud. Nothing plays until a row's button is pressed. Escape stops it. With
 * reduced motion, the line is still: a glowing calm wave. */
"use strict";
(() => {
  const list = document.querySelector(".tunes"), data = document.getElementById("tunes-data"), pl = window.RiverPlayer;
  if (!list || !data || !pl) return;
  pl.base = list.dataset.emu;
  const TUNES = JSON.parse(data.textContent).map((t) => ({ ...t, bytes: Uint8Array.from(t.hex.match(/../g), (x) => parseInt(x, 16)) }));
  const rows = [...list.querySelectorAll(".tune")];
  const note = document.querySelector(".tunes-note");
  const reduce = matchMedia("(prefers-reduced-motion: reduce)");
  const ICE = "110, 208, 255", WHITE = "243, 246, 249";

  const say = (t) => { if (note) note.textContent = t; };
  function mark(i, state) {
    const r = rows[i];
    if (!r) return;
    r.dataset.state = state;
    const b = r.querySelector(".tune-b");
    b.setAttribute("aria-pressed", String(state === "playing"));
    b.setAttribute("aria-label", (state === "playing" ? "Stop " : "Play ") + TUNES[i].name);
  }

  // ---------------------------------------------------------------- the sound
  pl.addEventListener("loading", (e) => mark(e.detail.id, "loading"));
  pl.addEventListener("playing", (e) => {
    say("");
    rows.forEach((r, i) => { if (i !== e.detail.id && r.dataset.state) mark(i, ""); });
    mark(e.detail.id, "playing");
    wake();
  });
  pl.addEventListener("stopped", (e) => { mark(e.detail.id, ""); wake(); });
  pl.addEventListener("error", (e) => {
    const { id, message } = e.detail;
    mark(id, "");
    say(message);
  });
  function play(i) {
    if (pl.id === i && pl.state === "playing") { pl.stop(); return; }
    say("");
    pl.unlock();
    pl.play(TUNES[i].bytes, i);
  }
  list.addEventListener("click", (e) => {
    const r = e.target.closest(".tune");
    if (r) play(Number(r.dataset.i));
  });
  addEventListener("keydown", (e) => {
    if (e.key === "Escape" && pl.state !== "idle" && !document.querySelector("dialog[open]")) pl.stop();
  });

  // ---------------------------------------------------------------- the current
  const flows = rows.map((r) => ({ c: r.querySelector(".tune-flow"), amp: 0, drawn: false }));
  let raf = 0, seen = true, last = 0;
  function fit(f) {
    const dpr = Math.min(devicePixelRatio || 1, 2), w = Math.max(1, Math.round(f.c.clientWidth)), h = Math.max(1, Math.round(f.c.clientHeight));
    if (f.c.width !== Math.round(w * dpr) || f.c.height !== Math.round(h * dpr)) { f.c.width = Math.round(w * dpr); f.c.height = Math.round(h * dpr); }
    return { ctx: f.c.getContext("2d"), w, h, dpr };
  }
  function draw(f, i, t) {
    const { ctx, w: W, h: H, dpr } = fit(f), a = f.amp, bps = TUNES[i].bpm / 60;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, W, H);
    f.drawn = a > 0.002;
    if (!f.drawn) return;
    const mid = H / 2, A = Math.min(1, a * 1.25) * (mid - 2), tt = t * (0.6 + 0.25 * bps);
    const y = (x) => {
      const u = x / W, e = Math.sin(Math.PI * u);
      return mid + A * e * (0.62 * Math.sin(u * 9.4 - tt / 900) + 0.38 * Math.sin(u * 23 + tt / 640));
    };
    ctx.lineWidth = 1.3;
    ctx.lineJoin = "round";
    const vis = Math.min(1, a * 5);       // (fading out: the line goes with it, not only its swell)
    ctx.strokeStyle = `rgba(${ICE}, ${(vis * (0.3 + 0.7 * Math.min(1, a * 1.6))).toFixed(3)})`;
    ctx.shadowColor = `rgba(${ICE}, ${(0.9 * vis).toFixed(3)})`;
    ctx.shadowBlur = 4 + 9 * a;
    ctx.beginPath();
    for (let x = 0; x <= W; x += 2) x ? ctx.lineTo(x, y(x)) : ctx.moveTo(x, y(x));
    ctx.stroke();
    // the ice it carries: a crystal every 26 px or so, each at its own pace and depth, downstream at the tempo
    const n = Math.max(6, Math.round(W / 26));
    ctx.shadowBlur = 6;
    for (let k = 0; k < n; k++) {
      const h1 = Math.sin(k * 127.1) * 43758.5453, r1 = h1 - Math.floor(h1);
      const h2 = Math.sin(k * 311.7) * 24634.6345, r2 = h2 - Math.floor(h2);
      const u = (k / n + r1 * 0.5 + (t / 1000) * bps * (0.018 + 0.02 * r2)) % 1, x = u * W;
      const edge = Math.sin(Math.PI * u), tw = 0.55 + 0.45 * Math.sin(t / (380 + 420 * r2) + k * 2.1);
      const al = a * edge * tw;
      if (al < 0.02) continue;
      const yy = y(x) + (r2 - 0.5) * 9 * (0.4 + a), s = 1.1 + 1.6 * r1 * (0.5 + a);
      ctx.fillStyle = `rgba(${r1 > 0.72 ? WHITE : ICE}, ${(vis * Math.min(1, 0.25 + al * 1.6)).toFixed(3)})`;
      ctx.fillRect(x - s / 2, yy - s / 2, s, s);
    }
    ctx.shadowBlur = 0;
  }
  function frame(t) {
    raf = 0;
    const dt = last ? Math.min(64, t - last) : 16;
    last = t;
    const lv = Math.min(1, pl.level() * 9);
    let more = false;
    flows.forEach((f, i) => {
      const on = pl.id === i && pl.state === "playing";
      const goal = on ? (reduce.matches ? 0.55 : Math.max(0.22, lv)) : 0;
      const k = reduce.matches ? 1 : 1 - Math.pow(on ? 0.94 : 0.965, dt / 16);
      f.amp += (goal - f.amp) * k;
      if (!on && f.amp < 0.002) f.amp = 0;
      if (f.amp || f.drawn) draw(f, i, reduce.matches ? 0 : t);
      rows[i].style.setProperty("--lv", f.amp.toFixed(3));
      if (f.amp && !(reduce.matches && !on)) more = true;
    });
    if (more && seen && !document.hidden && !reduce.matches) raf = requestAnimationFrame(frame);
    else last = 0;
  }
  function wake() { if (!raf) raf = requestAnimationFrame(frame); }
  if ("IntersectionObserver" in window) {
    new IntersectionObserver(([e]) => { seen = e.isIntersecting; if (seen) wake(); }).observe(list);
  }
  document.addEventListener("visibilitychange", () => { if (!document.hidden) wake(); });
  if ("ResizeObserver" in window) new ResizeObserver(() => { flows.forEach((f) => { f.drawn = true; }); wake(); }).observe(list);
})();
