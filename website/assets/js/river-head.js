/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * FRASIL River's header band, as the FM-1's play page draws its river (app/ui.c river()): a few currents, thin and
 * wavering, frasil drifting downstream along them (grains, needles along the flow, a disc catching light now and
 * then), the haze off the water, the waterline at the bottom brighter in the middle. Quiet, behind the logo and the
 * menu. Still when motion is reduced; asleep off screen or in a hidden tab. */
"use strict";
(() => {
  const band = document.querySelector(".rvh-band"), canvas = band && band.querySelector(".rvh-water");
  if (!canvas || !canvas.getContext) return;
  const ctx = canvas.getContext("2d");
  const reduce = matchMedia("(prefers-reduced-motion: reduce)");
  let seed = 23;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  let W = 0, H = 0, dpr = 1, currents = [], grains = [], raf = 0, last = 0, t = 0, seen = true;

  function build() {
    const r = band.getBoundingClientRect();
    W = Math.round(r.width); H = Math.round(r.height);
    dpr = Math.min(window.devicePixelRatio || 1, 2);
    canvas.width = Math.round(W * dpr); canvas.height = Math.round(H * dpr);
    seed = 23;
    // the currents: as the voices of a held chord, low ones lower; each its level, its drift
    currents = [0.42, 0.58, 0.71, 0.83].map((y, i) => ({ y: y * H, lvl: 0.55 + 0.45 * rnd(), ph: rnd() * 6.28, sp: 0.6 + 0.6 * rnd(), ice: i === 1 }));
    grains = Array.from({ length: Math.round(90 * W / 1440) }, () => {
      const c = currents[Math.floor(rnd() * currents.length)];
      return { c, u: rnd(), off: (rnd() + rnd() - 1) * 7, sp: 0.5 + rnd(), kind: rnd(), b: 0.35 + 0.65 * rnd() };
    });
  }

  const wob = (c, x, tt) => c.y + Math.sin(x * 0.0064 + c.ph + tt * 0.05 * c.sp) * 2.2 + Math.sin(x * 0.0154 + 2 * c.ph + tt * 0.08 * c.sp) * 0.9;

  function draw(now) {
    const tt = reduce.matches ? 40 : now / 1000;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, W, H);
    for (const c of currents) {                       // a current: its halo, then the line, fading out at the sides
      const g = ctx.createLinearGradient(0, 0, W, 0);
      const col = c.ice ? "110,208,255" : "220,227,234";
      g.addColorStop(0, `rgba(${col},0)`); g.addColorStop(0.18, `rgba(${col},${0.16 * c.lvl})`);
      g.addColorStop(0.82, `rgba(${col},${0.16 * c.lvl})`); g.addColorStop(1, `rgba(${col},0)`);
      ctx.strokeStyle = g;
      ctx.lineWidth = 1;
      ctx.beginPath();
      for (let x = 0; x <= W; x += 6) { const y = wob(c, x, tt); if (x) ctx.lineTo(x, y); else ctx.moveTo(x, y); }
      ctx.stroke();
      ctx.lineWidth = 7; ctx.globalAlpha = 0.24; ctx.stroke(); ctx.globalAlpha = 1;
    }
    for (const p of grains) {                         // the frasil, downstream, each at its own pace
      let u = (p.u + tt * 0.006 * p.sp) % 1;
      const x = u * (W + 40) - 20, y = wob(p.c, x, tt) + p.off;
      const edge = Math.min(1, Math.min(x, W - x) / (W * 0.18));
      if (edge <= 0) continue;
      const a = 0.6 * p.b * edge;
      ctx.fillStyle = p.c.ice || p.kind > 0.7 ? `rgba(110,208,255,${a})` : `rgba(220,227,234,${a})`;
      if (p.kind < 0.08) {                             // a disc catching light: a small cross
        ctx.fillRect(x - 1, y, 3, 1); ctx.fillRect(x, y - 1, 1, 3);
      } else if (p.kind < 0.45) {                      // a needle along the flow
        ctx.fillRect(x, y, 3, 1);
      } else {
        ctx.fillRect(x, y, 1, 1);
      }
    }
  }

  function frame(now) {
    raf = 0;
    if (!seen || document.hidden) return;
    if (now - last >= 40) { last = now; draw(now); }  // (25 frames a second: it moves slowly)
    if (!reduce.matches) raf = requestAnimationFrame(frame);
  }
  const start = () => { if (!raf) raf = requestAnimationFrame(frame); };

  build();
  draw(performance.now());
  start();
  if ("IntersectionObserver" in window) new IntersectionObserver(([e]) => { seen = e.isIntersecting; if (seen) start(); }).observe(band);
  document.addEventListener("visibilitychange", () => { if (!document.hidden) start(); });
  let rz = 0;
  addEventListener("resize", () => { clearTimeout(rz); rz = setTimeout(() => { build(); draw(performance.now()); }, 150); });
  reduce.addEventListener("change", () => { draw(performance.now()); start(); });
})();
