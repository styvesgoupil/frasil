/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Perform's logo, top left, alive as the site's hero draws it (website/assets/js/river.js): FRASIL's letters above
 * the waterline still, their lower half under the water, deep blue, swaying row by row and a little veiled; frasil
 * drifting over the waterline, the sky's light on the water in short strokes that come and go. Drawn from the vector
 * logo (website/assets/img/logo-print.svg: its two paths, the letters with the waterline, and the lower half) on a
 * canvas the logo's size at three times its pixels. Still when motion is reduced.
 *   PerformLogo.start(canvas)   canvas.dataset.src: the logo's SVG */
"use strict";
window.PerformLogo = (() => {
  const reduce = matchMedia("(prefers-reduced-motion: reduce)");
  const VW = 2324, VH = 472, WL = 223, SPLIT = 246;   /* the SVG's units: its width, height, the waterline, the lower half */
  let seed = 47;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);

  async function start(canvas) {
    const svg = await fetch(canvas.dataset.src).then((r) => r.text());
    const doc = new DOMParser().parseFromString(svg, "image/svg+xml");
    const [top, low] = [...doc.querySelectorAll("path")].map((p) => ({ d: new Path2D(p.getAttribute("d")), fill: p.getAttribute("fill") }));
    const ctx = canvas.getContext("2d"), W = canvas.width, H = canvas.height, s = W / VW, wl = WL * s, y0 = SPLIT * s;
    /* the lower half, drawn once, then laid row by row */
    const under = document.createElement("canvas");
    under.width = W;
    under.height = H;
    const u = under.getContext("2d");
    u.scale(s, s);
    u.fillStyle = low.fill;
    u.fill(low.d, "evenodd");
    seed = 47;
    const grains = Array.from({ length: 22 }, () => {
      const k = rnd();
      return { x: rnd() * W, h: (rnd() < 0.15 ? 14 + rnd() * 30 : 2 + Math.abs(rnd() - rnd()) * 16) * s * 4, vx: (6 + rnd() * 14) * s * 4,
               p: rnd() * 6.3, f: 0.2 + rnd() * 0.5, kind: k < 0.1 ? 2 : k < 0.45 ? 1 : 0, len: (2 + rnd() * 3.5) * s * 4,
               a: 0.3 + rnd() * 0.5, ice: rnd() < 0.3 };
    });
    const strokes = Array.from({ length: 10 }, () => ({ x: rnd() * W, y: y0 + rnd() * (H - y0 - 4), len: (6 + rnd() * 26) * s * 4,
                                                      a: 0.12 + rnd() * 0.22, T: 3 + rnd() * 5, p: rnd(), vx: (2 + rnd() * 6) * s * 4 }));
    let t = 6, last = 0;
    const px = s * 4;                    /* a hero's pixel here (the hero's logo some four times as wide) */
    function draw() {
      ctx.clearRect(0, 0, W, H);
      const SD = 2;                      /* the letters under the water: rows that sway, deeper more, veiled */
      for (let Y = Math.round(y0); Y < H; Y += SD) {
        const d = (Y - y0) / (H - y0), yh = Y / px;
        const dx = ((0.6 + 3.2 * d) * Math.sin(t * 0.9 + yh * 0.11) + (0.3 + 1.6 * d) * Math.sin(t * 0.53 - yh * 0.047 + 1.7)) * px;
        ctx.globalAlpha = 0.9 - 0.32 * d;
        ctx.drawImage(under, 0, Y, W, SD, Math.round(dx), Y, W, SD);
      }
      ctx.globalAlpha = 1;
      const veil = ctx.createLinearGradient(0, y0, 0, H);
      veil.addColorStop(0, "rgba(10, 13, 18, 0)");
      veil.addColorStop(1, "rgba(10, 13, 18, 0.3)");
      ctx.globalCompositeOperation = "source-atop";
      ctx.fillStyle = veil;
      ctx.fillRect(0, y0, W, H - y0);
      ctx.globalCompositeOperation = "source-over";
      for (const k of strokes) {         /* the sky's light on the water */
        const q = (t / k.T + k.p) % 1, a = k.a * Math.pow(Math.sin(Math.PI * q), 2);
        if (a < 0.01) continue;
        ctx.fillStyle = `rgba(176, 196, 214, ${a.toFixed(3)})`;
        ctx.fillRect((k.x + t * k.vx) % (W + 40) - 20, k.y, k.len, Math.max(1, px * 0.9));
      }
      for (const g of grains) {          /* frasil over the waterline */
        const x = (g.x + t * g.vx) % (W + 20) - 10, y = wl - g.h + Math.sin(t * g.f + g.p) * 1.6 * px;
        const a = g.a * (0.75 + 0.25 * Math.sin(t * 0.7 + g.p * 2));
        ctx.fillStyle = g.ice ? `rgba(150, 214, 245, ${a.toFixed(3)})` : `rgba(220, 227, 234, ${a.toFixed(3)})`;
        const w = Math.max(1.5, px);
        if (g.kind === 0) ctx.fillRect(x, y, w, w);
        else if (g.kind === 1) ctx.fillRect(x, y, g.len, w * 0.8);
        else { ctx.fillRect(x - 1.5 * w, y, 3 * w, w * 0.8); ctx.fillRect(x - 0.4 * w, y - 1.1 * w, w * 0.8, 3 * w); }
      }
      ctx.save();                        /* the letters above the water, and the waterline */
      ctx.scale(s, s);
      ctx.fillStyle = top.fill;
      ctx.fill(top.d, "evenodd");
      ctx.restore();
    }
    function frame(now) {
      t += Math.min(0.1, (now - (last || now)) / 1000);
      last = now;
      draw();
      if (!reduce.matches) requestAnimationFrame(frame);
    }
    draw();
    if (!reduce.matches) requestAnimationFrame(frame);
  }
  return { start: (c) => start(c).catch(() => {}) };
})();
