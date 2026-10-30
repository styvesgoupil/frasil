/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Perform's river: the FM-1's play-page river (app/ui.c: river, current, crystal, glints, bass_bed, haze) drawn again
 * for a big screen, more detail rather than bigger pixels. What sounds comes from web_live's 52 floats (app/web.c), the
 * scene's settings from its values (CHOIR, PARTIALS, BREATH, MOTION, SPACE, FREEZE), as the FM-1 would send them.
 *   PerformRiver.create(canvas) -> {draw(frame, look, now)}
 *     frame  web_live's floats (52)
 *     look   {choir, partials, breath, motion, space, sink (0..1), freeze (bool), dim (0..1), notes (bool: glints named),
 *             lit: Set of the drone notes whose label is lit (a key pressed), mix: {a (0..1), lv: the MIXER's four
 *             levels, 0..1} (drawn behind everything while a > 0)} */
"use strict";
window.PerformRiver = (() => {
  const W = 1920, H = 1080;
  const C = {
    bg: [10, 13, 18], steel: [38, 48, 60], slate: [74, 85, 99], mist: [139, 149, 164], fog: [220, 227, 234],
    lamp: [242, 166, 90], ice: [110, 208, 255], haze: [51, 65, 79], white: [255, 255, 255],
  };
  const G = { x0: 176, x1: 1824, top: 150, bottom: 806 };
  G.xc = G.x0 + (130 - 14) / 212 * (G.x1 - G.x0);     /* the device's xc 130 between its margins 14 and 226 */
  G.sx = (G.x1 - G.x0) / 212;                          /* one device pixel, across */
  G.sy = (G.bottom - G.top) / 84;                      /* and down (R2 32 .. HZ_HOME 116) */
  const rgba = (c, a) => `rgba(${c[0] | 0},${c[1] | 0},${c[2] | 0},${a < 0 ? 0 : a > 1 ? 1 : a})`;
  const mix = (a, b, t) => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];
  const sn = (t) => Math.sin(Math.PI * 2 * t);
  const clamp01 = (t) => (t < 0 ? 0 : t > 1 ? 1 : t);
  const smooth01 = (t) => { t = clamp01(t); return t * t * (3 - 2 * t); };
  const hsh = (i, k) => {
    let v = (Math.imul(i, 374761393) + Math.imul(k, 668265263)) >>> 0;
    v = Math.imul(v ^ (v >>> 13), 1274126177) >>> 0;
    return (v ^ (v >>> 16)) & 0xFFFF;
  };
  const NN = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const noteName = (n) => NN[n % 12] + (Math.floor(n / 12) - 1);
  const noteY = (n) => {                               /* C1 on the waterline, D6 at the top */
    const y = G.bottom - (n - 24) * (G.bottom - G.top) / 62;
    return Math.max(G.top + 40, Math.min(G.bottom - 30, y));
  };
  const devX = (x) => 14 + (x - G.x0) / G.sx;          /* a screen x as the device's */

  /* a horizontal fade at both ends, as ui.edge does: alpha a in the middle */
  function band(ctx, xa, xb, fade, col, a) {
    const g = ctx.createLinearGradient(xa, 0, xb, 0), f = Math.min(0.5, fade / Math.max(1, xb - xa));
    [[0, 0], [0.25, 0.16], [0.5, 0.5], [0.75, 0.84], [1, 1]].forEach(([u, k]) => {
      g.addColorStop(u * f, rgba(col, a * k));
      g.addColorStop(1 - u * f, rgba(col, a * k));
    });
    return g;
  }

  /* the halo's profile across a current: exp(-d^2) out to 2.4 of its thickness, as current()'s prof[] (64 px tall) */
  function profile(col) {
    const c = document.createElement("canvas");
    c.width = 1;
    c.height = 64;
    const x = c.getContext("2d");
    for (let y = 0; y < 64; y++) {
      const d = ((y + 0.5) / 32 - 1) * 2.4;
      x.fillStyle = rgba(col, Math.exp(-d * d));
      x.fillRect(0, y, 1, 1);
    }
    return c;
  }

  function create(canvas) {
    const ctx = canvas.getContext("2d");
    const FOG = profile(C.fog), LAMP = profile(C.lamp);
    let labels = [];
    let k = 1, stillT = 0, frzAt = -1e9, frz = false, last = 0;
    function fit() {
      const r = canvas.getBoundingClientRect(), dpr = Math.min(2, window.devicePixelRatio || 1);
      const w = Math.max(320, Math.round(Math.min(r.width * dpr, 2880)));
      if (canvas.width !== w) {
        canvas.width = w;
        canvas.height = Math.round(w * H / W);
      }
      k = canvas.width / W;
    }

    /* SPACE: the haze over the bed, stratus rows; dotted when set */
    function haze(look, dim) {
      const space = look.space, hh = (24 + 46 * space) * (G.bottom - G.top) / 143;
      for (let y = G.bottom - hh, i = 0; y < G.bottom; y += 3, i++) {
        const tr = (y - (G.bottom - hh)) / hh;
        const a = 0.55 * space * dim * tr * tr * (i & 1 ? 1 : 0.6) * (look.freeze ? 1.8 : 1);
        if (look.freeze && i % 2) continue;
        ctx.fillStyle = band(ctx, G.x0, G.x1, 260, C.haze, a);
        if (look.freeze) {
          for (let x = G.x0; x < G.x1; x += 9) ctx.fillRect(x, y, 3, 2);
        } else ctx.fillRect(G.x0, y, G.x1 - G.x0, 2);
      }
    }

    /* the bass: a slow swell in lamp at its note's height, the water under it warm down to the bed */
    function bass(f, look, dim, tt, fz) {
      const note = f[24], lvl = f[25];
      if (note < 0 || lvl < 0.01) return;
      const yc = noteY(note), amp = (1.5 + 2.5 * lvl) * (1 - fz) * G.sy, a0 = (0.40 + 0.50 * lvl) * dim;
      const pts = [];
      for (let x = G.x0; x <= G.x1; x += 8) {
        const d = devX(x);
        pts.push([x, yc + amp * (sn(d * 0.0042 + tt * 0.021) * 0.8 + sn(d * 0.0097 - tt * 0.013) * 0.2)]);
      }
      const path = () => { ctx.beginPath(); pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y))); };
      const g = ctx.createLinearGradient(0, yc - amp, 0, G.bottom);
      g.addColorStop(0, rgba(mix(C.haze, C.lamp, 0.35), 0.20 * a0));
      g.addColorStop(1, rgba(mix(C.haze, C.lamp, 0.25), 0.015 * a0));
      ctx.fillStyle = g;
      pts.forEach(([x, y]) => {           /* the water under it, down to the bed */
        ctx.globalAlpha = smooth01((x - G.x0) / 260) * smooth01((G.x1 - x) / 260);
        ctx.fillRect(x - 4, y + 2, 8.5, G.bottom - y - 2);
        ctx.globalAlpha *= 0.10 * a0 * (0.6 + lvl);
        ctx.drawImage(LAMP, x - 4, y - 18, 8.5, 36);
      });
      ctx.globalAlpha = 1;
      ctx.lineCap = "round";
      path();
      ctx.lineWidth = 3;
      ctx.strokeStyle = band(ctx, G.x0, G.x1, 260, C.lamp, 0.95 * a0);
      ctx.stroke();
      labels.push([noteName(note), G.x1, yc - 30, "right", rgba(C.mist, 0.9 * dim)]);
    }

    function label(t, x, y, align, col, size = 26) {
      ctx.font = `600 ${size}px "Barlow Semi Condensed", sans-serif`;
      ctx.letterSpacing = "2px";
      ctx.textAlign = align;
      ctx.textBaseline = "middle";
      ctx.fillStyle = col;
      ctx.fillText(t, x, y);
    }

    /* one crystal of frasil: a grain, a needle along the flow, a disc catching light; set ice is a small cross */
    function crystal(x, y, col, b, kind, set) {
      if (set) {
        ctx.fillStyle = rgba(col, b);
        ctx.fillRect(x - 1.5, y - 1.5, 3, 3);
        ctx.fillStyle = rgba(col, 0.45 * b);
        ctx.fillRect(x - 5, y - 0.75, 10, 1.5);
        ctx.fillRect(x - 0.75, y - 5, 1.5, 10);
      } else if (kind < 0.08) {
        ctx.fillStyle = rgba(col, Math.min(1, b * 1.2));
        ctx.fillRect(x - 1.5, y - 1.5, 3, 3);
        ctx.fillStyle = rgba(col, 0.4 * b);
        ctx.fillRect(x - 4.5, y - 1, 9, 2);
        ctx.fillRect(x - 1, y - 4.5, 2, 9);
      } else if (kind < 0.40) {
        ctx.fillStyle = rgba(col, b);
        ctx.fillRect(x, y - 0.9, 4, 1.8);
        ctx.fillStyle = rgba(col, 0.6 * b);
        ctx.fillRect(x + 4, y - 0.9, 4, 1.8);
        ctx.fillStyle = rgba(col, 0.25 * b);
        ctx.fillRect(x + 8, y - 0.9, 5, 1.8);
      } else {
        ctx.fillStyle = rgba(col, 0.9 * b);
        ctx.fillRect(x - 1.1, y - 1.1, 2.2, 2.2);
      }
    }

    /* a voice: the water around it, the current, its crystals */
    function current(note, lvl, look, dim, tt, fz) {
      const wob = 1 - fz, yc = noteY(note), half = (30 + lvl * 76) * G.sx, ph = note * 0.37;
      const th = (1.4 + lvl * 2) * G.sy, drift = tt * (0.004 + 0.03 * look.motion), fade = 34 * G.sx;
      const xa = Math.max(G.x0 - 20, G.xc - half), xb = Math.min(G.x1, G.xc + half);
      const yAt = (x) => {
        const d = devX(x);
        return yc + wob * G.sy * (sn(d * 0.0064 + ph + drift) * 1.1 + sn(d * 0.0154 + 2 * ph + drift * 1.7) * 0.45);
      };
      const path = () => {
        ctx.beginPath();
        for (let x = xa; x <= xb; x += 6) (x === xa ? ctx.moveTo(x, yAt(x)) : ctx.lineTo(x, yAt(x)));
      };
      const halo = lvl * dim * 0.10 * (0.15 + 1.1 * look.breath) * 1.25, rr = th * 2.4;
      for (let x = xa; x <= xb; x += 6) {   /* the water around the current */
        const e = 1 - smooth01((Math.abs(x - G.xc) - (half - fade)) / fade);
        if (e <= 0) continue;
        ctx.globalAlpha = Math.min(1, halo * e);
        ctx.drawImage(FOG, x - 3, yAt(x) - rr, 6.6, 2 * rr);
      }
      ctx.globalAlpha = 1;
      ctx.lineCap = "round";
      const ca = (0.22 + 0.25 * lvl) * dim * (1 + 1.2 * fz) * (0.25 + 0.95 * look.choir);
      path();
      ctx.lineWidth = 1.8;
      ctx.strokeStyle = band(ctx, xa, xb, fade, C.fog, Math.min(1, ca * 1.5));
      ctx.stroke();
      /* the frasil, downstream, faster with MOTION; FREEZE eases it into a lattice */
      const ice = mix(fz > 0.5 ? C.ice : mix(C.ice, C.fog, 10 / 32), C.lamp, 0);
      const nc = Math.round((18 + 56 * lvl) * (dim > 0.5 ? 1 : 0.45) * look.partials * 3.2);
      const seed = note * 31 + 7;
      for (let i = 0; i < nc; i++) {
        const u = hsh(seed, i * 3) / 65536, spread = 0.55 + 0.45 * hsh(seed, i * 3 + 7) / 65536;
        const speed = 0.6 + 0.8 * hsh(seed, i * 5 + 3) / 65536;
        let p = u + tt * (0.01 + 0.05 * look.motion) * speed;
        p -= Math.floor(p);
        let xf = G.xc + (p - 0.5) * 2 * (half - 6 * G.sx) * spread;
        const g = (hsh(seed, i * 3 + 1) + hsh(seed, i * 3 + 2)) / 65536 - 1;
        let yf = yc + g * th * 2.2 + wob * G.sy * sn(devX(xf) * 0.0064 + ph + drift) * 1.1;
        if (fz > 0) {                    /* the lattice: every 3 device pixels, on the current and either side */
          const step = 3 * G.sx;
          const lx = G.x0 + Math.floor((xf - G.x0) / step) * step + (i % 3 === 1 ? step / 2 : 0);
          const ly = yc + (i % 3 === 0 ? 0 : i % 3 === 1 ? -1.5 : 1.5) * G.sy;
          xf += (lx - xf) * fz;
          yf += (ly - yf) * fz;
        }
        if (xf < G.x0 || xf > G.x1) continue;
        const b = (1 - smooth01((Math.abs(xf - G.xc) - (half - fade)) / fade)) *
                  (0.45 + 0.55 * hsh(seed, i * 5 + 11) / 65536) * dim;
        if (b > 0.05) crystal(xf, yf, ice, b, hsh(seed, i * 7 + 5) / 65536, fz > 0.5);
      }
    }

    /* a glint in the sky, rising a little as it fades; its name beside it while it rings (look.notes) */
    function glints(f, look, dim) {
      for (let i = 0; i < 4; i++) {
        const note = f[16 + i], lv = f[20 + i] * dim, t = f[48 + i];
        if (note < 0 || lv < 0.01) continue;
        const rise = Math.min(8, t * 5) * G.sy;
        const gx = G.x0 + (44 + ((note * 37) % 23) * 8 - 14) * G.sx;
        const u = clamp01((note - 60) / 48);
        const gy = Math.max(G.top - 60, G.top - 50 + (1 - u) * (G.bottom - G.top) * 0.36 - rise);
        const r = 26 + 70 * lv;
        const gl = ctx.createRadialGradient(gx, gy, 0, gx, gy, r);
        gl.addColorStop(0, rgba(C.ice, 0.30 * lv));
        gl.addColorStop(0.35, rgba(C.ice, 0.08 * lv));
        gl.addColorStop(1, rgba(C.ice, 0));
        ctx.fillStyle = gl;
        ctx.fillRect(gx - r, gy - r, 2 * r, 2 * r);
        const arm = (1 + lv * 8) * G.sx * 0.9;
        [[1, 0, 1], [0, 1, 0.75]].forEach(([dx, dy, s]) => {
          const g = ctx.createLinearGradient(gx - dx * arm, gy - dy * arm, gx + dx * arm, gy + dy * arm);
          g.addColorStop(0, rgba(C.ice, 0));
          g.addColorStop(0.5, rgba(C.ice, lv * s));
          g.addColorStop(1, rgba(C.ice, 0));
          ctx.fillStyle = g;
          if (dx) ctx.fillRect(gx - arm, gy - 1.2, 2 * arm, 2.4);
          else ctx.fillRect(gx - 1.2, gy - arm * s, 2.4, 2 * arm * s);
        });
        ctx.fillStyle = rgba(C.white, lv);
        ctx.beginPath();
        ctx.arc(gx, gy, 3.2, 0, Math.PI * 2);
        ctx.fill();
        for (let j = 0; j < 6; j++) {    /* a few motes around it */
          const h = hsh(note, j);
          ctx.fillStyle = rgba(C.ice, lv * (h >> 12) / 16);
          ctx.fillRect(gx + ((h & 15) - 8) * G.sx * 0.8, gy + (((h >> 4) & 15) - 8) * G.sy * 0.8, 2.2, 2.2);
        }
        if (look.notes && t < 1.8)
          label(noteName(note), gx + 22, gy - 26, "left", rgba(C.ice, Math.min(1, 1.4 * (1 - t / 1.8)) * Math.max(0.4, lv)), 24);
      }
    }

    /* SINK: the river under water as it is turned, a clear blue veil from the top down, densest at the top, fading out
     * where it reaches and at the sides as the river does (ui.c sink, C_SINK) */
    function sink(look, dim) {
      const sv = Math.min(1, look.sink || 0), h = G.bottom - G.top;
      if (sv <= 0.01) return;
      const line = G.top + sv * 1.25 * h, fade = 0.6 * h;
      for (let y = G.top; y < G.bottom && y < line; y += 3) {
        const a = 0.34 * dim * smooth01((line - y) / fade) * smooth01((y - G.top + 1) / (6 * G.sy));
        ctx.fillStyle = band(ctx, G.x0, G.x1, 260, [10, 100, 176], a);
        ctx.fillRect(G.x0, y, G.x1 - G.x0, 3);
      }
    }

    /* the MIXER behind the river: a band each (drone, bass, glint, cloud), in its track's colour, risen from the
     * waterline as high as its level, glowing more as it rises; its heights and its fade glide */
    const MIXC = [C.fog, C.lamp, C.ice, [139, 170, 196]];
    const mixS = { a: 0, h: [0, 0, 0, 0] };
    function mixer(look) {
      const m = look.mix || { a: 0, lv: [0, 0, 0, 0] };
      mixS.a += (m.a - mixS.a) * (m.a > mixS.a ? 0.18 : 0.06);
      for (let i = 0; i < 4; i++) mixS.h[i] += ((m.lv[i] || 0) - mixS.h[i]) * 0.2;
      if (mixS.a < 0.005) return;
      const n = 4, bw = 250, gap = (G.x1 - G.x0 - n * bw) / (n + 1), hmax = (G.bottom - G.top) * 0.96;
      const across = (x, c, al) => {     /* soft at both sides, as the river's ends are */
        const g = ctx.createLinearGradient(x, 0, x + bw, 0);
        g.addColorStop(0, rgba(c, 0));
        g.addColorStop(0.22, rgba(c, al));
        g.addColorStop(0.78, rgba(c, al));
        g.addColorStop(1, rgba(c, 0));
        return g;
      };
      ctx.save();
      ctx.beginPath();
      ctx.rect(G.x0, G.top - 30, G.x1 - G.x0, G.bottom - G.top + 30);
      ctx.clip();
      for (let i = 0; i < n; i++) {
        const lv = mixS.h[i], x = G.x0 + gap + i * (bw + gap), top = G.bottom - lv * hmax, a = mixS.a, c = MIXC[i];
        ctx.fillStyle = across(x, c, a * (0.05 + 0.13 * lv));
        ctx.fillRect(x, top, bw, G.bottom - top);
        const gh = 16 + 46 * lv * lv;    /* the glow at its top, larger and brighter as it rises */
        ctx.save();
        ctx.translate(x + bw / 2, top);
        ctx.scale(bw / 2 / gh, 1);
        const gg = ctx.createRadialGradient(0, 0, 0, 0, 0, gh);
        gg.addColorStop(0, rgba(c, a * (0.08 + 0.32 * lv * lv)));
        gg.addColorStop(1, rgba(c, 0));
        ctx.fillStyle = gg;
        ctx.fillRect(-gh, -gh, 2 * gh, 2 * gh);
        ctx.restore();
        ctx.fillStyle = across(x, c, a * (0.3 + 0.5 * lv));
        ctx.fillRect(x, top - 1, bw, 2);
      }
      ctx.restore();
    }

    function draw(f, look, now) {
      fit();
      const t = now / 1000;
      if (frz !== !!look.freeze) { frz = !!look.freeze; frzAt = now; stillT = last; }
      let fz = smooth01((now - frzAt) / 300);
      if (!look.freeze) fz = 1 - fz;
      const tt = look.freeze ? stillT : t;
      last = tt;
      const dim = look.dim == null ? 1 : look.dim;
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.globalCompositeOperation = "source-over";
      ctx.fillStyle = rgba(C.bg, 1);
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      ctx.setTransform(k, 0, 0, k, 0, 0);
      ctx.globalCompositeOperation = "lighter";
      mixer(look);
      ctx.globalCompositeOperation = "source-over";
      haze(look, dim);
      ctx.globalCompositeOperation = "lighter";
      bass(f, look, dim, tt, fz);
      const order = [];
      for (let i = 0; i < 8; i++) if (f[i] >= 0 && f[8 + i] > 0.01) order.push(i);
      order.sort((a, b) => f[a] - f[b]);
      order.forEach((i) => current(f[i], f[8 + i], look, dim, tt, fz));
      glints(f, look, dim);
      ctx.globalCompositeOperation = "source-over";
      sink(look, dim);
      let lastY = 1e9;                   /* the voices' notes, lowest first, apart enough not to collide */
      order.forEach((i) => {
        const y = noteY(f[i]);
        if (lastY - y < 30) return;
        const lit = look.lit && look.lit.has(f[i] % 12);
        labels.push([noteName(f[i]), 96, y - 4, "left", rgba(lit ? C.lamp : C.slate, (lit ? 1 : 0.95) * Math.max(dim, lit ? 1 : 0))]);
        lastY = y;
      });
      /* the river in the water under the waterline, faint */
      const rh = 230;
      ctx.save();
      ctx.globalAlpha = 0.24 * dim;
      ctx.translate(0, 2 * G.bottom);
      ctx.scale(1, -1);
      ctx.drawImage(canvas, 0, (G.bottom - rh) * k, canvas.width, rh * k, 0, G.bottom - rh, W, rh);
      ctx.restore();
      const fadeG = ctx.createLinearGradient(0, G.bottom, 0, G.bottom + rh);
      fadeG.addColorStop(0, rgba(C.bg, 0.25));
      fadeG.addColorStop(1, rgba(C.bg, 1));
      ctx.fillStyle = fadeG;
      ctx.fillRect(0, G.bottom, W, rh + 2);
      ctx.fillStyle = band(ctx, G.x0, G.x1, 200, look.freeze ? C.ice : C.slate, (look.freeze ? 0.9 : 0.7) * Math.max(dim, 0.5));
      ctx.fillRect(G.x0, G.bottom, G.x1 - G.x0, 1.5);   /* the waterline */
      if (look.labels !== false) labels.forEach((l) => label(...l));
      labels = [];
    }
    return { draw, fit };
  }
  return { create, noteName, W, H, G, C };
})();
