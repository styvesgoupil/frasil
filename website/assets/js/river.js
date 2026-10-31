/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The landing's river at night, as FRASIL's logo screen draws it: stars over it, and frasil drifting in the water
 * (grains, needles, discs catching the light, set crosses, small plates: Perform's crystals), most near the surface,
 * fewer, slower and bluer down to the submerged letters' foot; the logo on it, its lower half under the water, deep blue, swaying; slow swells of light passing
 * through the water and the sky's light on it in short strokes that come and go.
 * The layout is CSS's: the logo's box says where the waterline is. Still when motion is reduced; asleep off screen. */
"use strict";
(() => {
  const hero = document.querySelector(".hero");
  const canvas = hero && hero.querySelector(".hero-river");
  if (!canvas || !canvas.getContext) return;
  const ctx = canvas.getContext("2d");
  const logoBox = hero.querySelector(".hero-logo");
  const imgTop = hero.querySelector(".logo-top"), imgLow = hero.querySelector(".logo-low");
  const WL = parseFloat(getComputedStyle(hero).getPropertyValue("--wl")) || 0.467;   // the blade: the waterline
  const SPLIT = parseFloat(hero.dataset.split) || 0.502;                             // where the lower half starts
  const reduce = matchMedia("(prefers-reduced-motion: reduce)");
  const LIFT = 1.35;                     // its lights a little brighter than the device's: seen on a dimmed screen too
  const small = matchMedia("(max-width: 700px), (pointer: coarse)");

  // a small seeded random, so the sky is the same sky each time
  let seed = 47;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);

  let W = 0, H = 0, dpr = 1, wl = 0, L = null, bg = null;
  let stars = [], grains = [], strokes = [], swells = [];
  let raf = 0, last = 0, t = 0, seen = true, ready = false;
  let meteor = null, nextMeteor = 6 + Math.random() * 8;   // a shooting star now and then (the first after a few seconds)

  function build() {
    const r = hero.getBoundingClientRect(), lr = logoBox.getBoundingClientRect();
    W = Math.round(r.width); H = Math.round(r.height);
    dpr = Math.min(window.devicePixelRatio || 1, small.matches ? 1.5 : 2);
    canvas.width = Math.round(W * dpr); canvas.height = Math.round(H * dpr);
    L = { x: lr.left - r.left, y: lr.top - r.top, w: lr.width, h: lr.height };
    wl = L.y + L.h * WL;
    seed = 47;
    const area = W / 1440;

    // the still part: sky, the haze off the water, the water, drawn once
    bg = document.createElement("canvas");
    bg.width = canvas.width; bg.height = canvas.height;
    const b = bg.getContext("2d");
    b.scale(dpr, dpr);
    let g = b.createLinearGradient(0, 0, 0, wl);
    g.addColorStop(0, "#05070b"); g.addColorStop(.62, "#080c12"); g.addColorStop(1, "#0d1520");
    b.fillStyle = g; b.fillRect(0, 0, W, wl);
    g = b.createLinearGradient(0, wl, 0, H);             // the water: a deep, dark blue under the surface, back to the
    g.addColorStop(0, "#0c1c33"); g.addColorStop(.22, "#0b182b");   // page's own night as it goes down
    g.addColorStop(.55, "#0a1322"); g.addColorStop(.82, "#0a0f19"); g.addColorStop(1, "#0a0d12");
    b.fillStyle = g; b.fillRect(0, wl, W, H - wl);
    b.save();                                            // the haze: the reverb rising off the water
    b.translate(W / 2, wl); b.scale(W * .62, 90);
    g = b.createRadialGradient(0, 0, 0, 0, 0, 1);
    g.addColorStop(0, "rgba(64, 84, 104, .38)"); g.addColorStop(1, "rgba(64, 84, 104, 0)");
    b.fillStyle = g; b.fillRect(-1, -1, 2, 1);
    b.restore();
    for (let i = 0; i < 260 * area * (wl / 520); i++) {   // faint, far stars: still
      const x = rnd() * W, y = Math.pow(rnd(), 1.15) * (wl - 30);
      b.fillStyle = `rgba(220, 227, 234, ${(LIFT * (.08 + rnd() * .22)).toFixed(3)})`;
      b.fillRect(x, y, rnd() < .5 ? .7 : 1, .7);
    }
    g = b.createLinearGradient(0, 0, W, 0);              // the waterline, across
    g.addColorStop(0, "rgba(74, 85, 99, 0)"); g.addColorStop(.2, "rgba(74, 85, 99, .55)");
    g.addColorStop(.5, "rgba(170, 186, 204, .55)"); g.addColorStop(.8, "rgba(74, 85, 99, .55)"); g.addColorStop(1, "rgba(74, 85, 99, 0)");
    b.fillStyle = g; b.fillRect(0, Math.round(wl) - .5, W, 1);

    // stars that breathe
    stars = [];
    for (let i = 0; i < 95 * area + 26; i++) {
      stars.push({ x: rnd() * W, y: 18 + Math.pow(rnd(), 1.1) * (wl - 60), r: .7 + rnd() * 1.0, a: .4 + rnd() * .6,
                   s: .25 + rnd() * .9, p: rnd() * 6.3, ice: rnd() < .22, cross: rnd() < .07 });
    }
    // frasil: crystals drifting in the water, from just under the surface down to the submerged letters' foot
    grains = [];
    const deep = Math.max(40, (L.y + L.h - wl) * 1.15);
    const n = Math.round((small.matches ? 110 : 190) * Math.min(1.4, area + .2));
    for (let i = 0; i < n; i++) {
      const k = rnd(), d = 2 + Math.pow(rnd(), 1.7) * deep, z = d / deep;   // (most near the surface)
      grains.push({ x: rnd() * W, d, vx: (5 + rnd() * 13) * (1 - .45 * z), p: rnd() * 6.3, f: .2 + rnd() * .5,
                    kind: k < .34 ? 0 : k < .62 ? 1 : k < .78 ? 2 : k < .9 ? 3 : 4,   // grain, needle, disc, cross, plate
                    size: .7 + rnd() * .8, len: 2.5 + rnd() * 4, a: (.3 + rnd() * .6) * (1 - .6 * z), ice: rnd() < .3 + .5 * z });
    }
    // the sky's light on the water: short strokes that come and go
    strokes = [];
    const depth = H - wl;
    for (let i = 0; i < (small.matches ? 46 : 84) * Math.max(.6, area); i++) {
      const d = Math.pow(rnd(), 1.7);
      strokes.push({ x: rnd() * W, y: wl + 7 + d * (depth - 20), len: (5 + rnd() * 36) * (1 - d * .55), a: .05 + rnd() * .2 * (1 - d * .6),
                     T: 3 + rnd() * 6, p: rnd(), vx: 2 + rnd() * 6 });
    }
    // under the brightest stars, their light broken on the water
    for (const s of stars.filter((s) => s.a > .8).slice(0, 7)) {
      for (let j = 0; j < 4; j++) strokes.push({ x: s.x - 4 + rnd() * 8, y: wl + 10 + j * 9 + rnd() * 5, len: 3 + rnd() * 7, a: .06 + rnd() * .1, T: 2 + rnd() * 3, p: rnd(), vx: 0 });
    }
    swells = [0, .37, .71].map((o, i) => ({ o, T: 17 + i * 5 }));
  }

  function draw(now) {
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.drawImage(bg, 0, 0);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    // stars
    for (const s of stars) {
      const a = Math.min(1, LIFT * s.a * (.62 + .38 * Math.sin(t * s.s + s.p)));
      ctx.fillStyle = s.ice ? `rgba(110, 208, 255, ${a.toFixed(3)})` : `rgba(220, 227, 234, ${a.toFixed(3)})`;
      ctx.fillRect(s.x - s.r / 2, s.y - s.r / 2, s.r, s.r);
      if (s.cross) {
        ctx.globalAlpha = a * .45;
        ctx.fillRect(s.x - 4.5, s.y - .3, 9, .6);
        ctx.fillRect(s.x - .3, s.y - 4.5, .6, 9);
        ctx.globalAlpha = 1;
      }
    }
    shootingStar();

    // the water: slow swells of light passing through it, going down
    const depth = H - wl;
    for (const w of swells) {
      const k = ((t / w.T + w.o) % 1);
      const y = wl + 6 + k * depth * .9, hh = 26 + k * 120;
      const a = LIFT * .16 * Math.sin(Math.PI * k) * (1 - k * .5);
      ctx.save();
      ctx.translate(W / 2, y); ctx.scale(W * .5, hh);
      const g = ctx.createRadialGradient(0, 0, 0, 0, 0, 1);
      g.addColorStop(0, `rgba(54, 92, 128, ${a.toFixed(3)})`); g.addColorStop(1, "rgba(54, 92, 128, 0)");
      ctx.fillStyle = g; ctx.fillRect(-1, -1, 2, 2);
      ctx.restore();
    }

    // the letters under the water: deep blue, swaying, row by row
    // (in device pixels: whole rows that touch and never overlap, so no row is drawn twice)
    const nw = imgLow.naturalWidth, nh = imgLow.naturalHeight;
    const Y0 = Math.round((L.y + L.h * SPLIT) * dpr), Y1 = Math.round((L.y + L.h) * dpr), SD = small.matches ? 3 : 2;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    for (let Y = Y0; Y < Y1; Y += SD) {
      const y = Y / dpr, d = (Y - Y0) / (Y1 - Y0);
      const dx = (.35 + 2.3 * d) * Math.sin(t * .9 + y * .11) + (.2 + 1.2 * d) * Math.sin(t * .53 - y * .047 + 1.7);
      const sy = (y - L.y) / L.h * nh;
      ctx.globalAlpha = .84 - .3 * d;
      ctx.drawImage(imgLow, 0, sy, nw, SD / dpr / L.h * nh, Math.round((L.x + dx) * dpr), Y, Math.round(L.w * dpr), SD);
    }
    ctx.globalAlpha = 1;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    // the water over the letters: they are under it, a little veiled
    let veil = ctx.createLinearGradient(0, L.y + L.h * SPLIT, 0, L.y + L.h);
    veil.addColorStop(0, "rgba(13, 24, 34, 0)"); veil.addColorStop(1, "rgba(13, 24, 34, .35)");
    ctx.fillStyle = veil; ctx.fillRect(L.x - 10, L.y + L.h * SPLIT, L.w + 20, L.h * (1 - SPLIT) + 1);

    // the strokes of light, over the letters too
    for (const s of strokes) {
      const k = ((t / s.T + s.p) % 1);
      const a = Math.min(1, LIFT * s.a * Math.pow(Math.sin(Math.PI * k), 2));
      if (a < .01) continue;
      let x = (s.x + t * s.vx) % (W + 40) - 20;
      ctx.fillStyle = `rgba(176, 196, 214, ${a.toFixed(3)})`;
      ctx.fillRect(x, s.y, s.len, 1);
    }

    // frasil in the water: each crystal swaying a little as the water does (Perform's shapes, smaller)
    for (const g of grains) {
      const x = (g.x + t * g.vx) % (W + 30) - 15 + Math.sin(t * .9 + g.p) * .8;
      const y = wl + g.d + Math.sin(t * g.f + g.p) * 1.3;
      const a = Math.min(1, LIFT * g.a * (.75 + .25 * Math.sin(t * .7 + g.p * 2)));
      const col = g.ice ? "150, 214, 245" : "220, 227, 234", s = g.size;
      const fill = (k) => (ctx.fillStyle = `rgba(${col}, ${(a * k).toFixed(3)})`);
      if (g.kind === 0) {                                 // a grain
        fill(1); ctx.fillRect(x, y, s, s);
      } else if (g.kind === 1) {                          // a needle along the flow, its tail fading behind it
        fill(1); ctx.fillRect(x, y, g.len * .45, .8);
        fill(.55); ctx.fillRect(x - g.len * .35, y, g.len * .35, .8);
        fill(.25); ctx.fillRect(x - g.len * .7, y, g.len * .35, .8);
      } else if (g.kind === 2) {                          // a disc catching the light: a bright core, a faint glint
        fill(1); ctx.fillRect(x - s, y - s, 2 * s, 2 * s);
        fill(.35); ctx.fillRect(x - 3 * s, y - .4, 6 * s, .8); ctx.fillRect(x - .4, y - 3 * s, .8, 6 * s);
      } else if (g.kind === 3) {                          // set ice: a small cross
        fill(.9); ctx.fillRect(x - 2 * s, y - .4, 4 * s, .8); ctx.fillRect(x - .4, y - 2 * s, .8, 4 * s);
      } else {                                            // a small plate: a diamond's outline
        fill(.8); ctx.beginPath(); ctx.moveTo(x, y - 2 * s); ctx.lineTo(x + 2 * s, y); ctx.lineTo(x, y + 2 * s);
        ctx.lineTo(x - 2 * s, y); ctx.closePath(); ctx.lineWidth = .7; ctx.strokeStyle = ctx.fillStyle; ctx.stroke();
      }
    }

    // the letters above the water
    ctx.drawImage(imgTop, L.x, L.y, L.w, L.h);
  }

  /* now and then (every 14 to 30 s) a shooting star: across the sky and down, under a second, its tail growing then
   * fading; never down to the water, behind the letters. None with motion reduced (draw once, t still) */
  function shootingStar() {
    if (!meteor && t >= nextMeteor && !reduce.matches) {
      const dir = Math.random() < .5 ? 1 : -1;
      meteor = { t0: t, dur: .6 + Math.random() * .4, x: W * (.12 + Math.random() * .76), y: 24 + Math.random() * wl * .4,
                 dx: dir * (240 + Math.random() * 220), dy: 70 + Math.random() * 90, len: 80 + Math.random() * 110 };
      nextMeteor = t + 14 + Math.random() * 16;
    }
    if (!meteor) return;
    const u = (t - meteor.t0) / meteor.dur;
    if (u >= 1) { meteor = null; return; }
    const e = 1 - (1 - u) * (1 - u), hx = meteor.x + meteor.dx * e, hy = meteor.y + meteor.dy * e;
    if (hy > wl - 24) { meteor = null; return; }
    const n = Math.hypot(meteor.dx, meteor.dy), tx = hx - meteor.dx / n * meteor.len * Math.min(1, u * 2.5),
          ty = hy - meteor.dy / n * meteor.len * Math.min(1, u * 2.5);
    const a = LIFT * .75 * (u < .12 ? u / .12 : 1 - (u - .12) / .88);
    const g = ctx.createLinearGradient(hx, hy, tx, ty);
    g.addColorStop(0, `rgba(240, 246, 252, ${a.toFixed(3)})`);
    g.addColorStop(.3, `rgba(150, 214, 245, ${(a * .4).toFixed(3)})`);
    g.addColorStop(1, "rgba(150, 214, 245, 0)");
    ctx.lineCap = "round"; ctx.lineWidth = 1.2; ctx.strokeStyle = g;
    ctx.beginPath(); ctx.moveTo(hx, hy); ctx.lineTo(tx, ty); ctx.stroke();
    ctx.fillStyle = `rgba(245, 250, 255, ${Math.min(1, a * 1.2).toFixed(3)})`;   // its head, a point of light
    ctx.fillRect(hx - 1, hy - 1, 2, 2);
  }

  function frame(now) {
    raf = 0;
    if (!seen || document.hidden || reduce.matches) return;
    const dt = Math.min(.1, (now - (last || now)) / 1000);
    if (small.matches && now - last < 31) { raf = requestAnimationFrame(frame); return; }    // phones: 30 frames
    last = now;
    t += dt;
    draw(now);
    raf = requestAnimationFrame(frame);
  }
  function start() {
    if (!ready) return;
    if (reduce.matches) { t = 14; draw(0); return; }
    if (!raf) { last = 0; raf = requestAnimationFrame(frame); }
  }

  let pending = 0;
  function relayout() {
    if (pending) return;
    pending = requestAnimationFrame(() => { pending = 0; build(); draw(0); start(); });
  }

  Promise.all([imgTop, imgLow].map((im) => (im.decode ? im.decode() : Promise.resolve()).catch(() => {})))
    .then(() => {
      if (!imgTop.naturalWidth || !imgLow.naturalWidth) return;       // the still logo stays
      ready = true;
      build();
      t = 6;
      draw(0);
      hero.classList.add("river-on");
      start();
      new ResizeObserver(relayout).observe(hero);
      if ("IntersectionObserver" in window) {
        new IntersectionObserver((es) => { seen = es[es.length - 1].isIntersecting; if (seen) start(); }).observe(hero);
      }
      document.addEventListener("visibilitychange", () => { if (!document.hidden) start(); });
      reduce.addEventListener && reduce.addEventListener("change", start);
    });
})();
