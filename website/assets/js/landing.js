/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The landing's small instruments, drawn the way FRASIL's screen draws (app/ui.c):
 *   the rivers: voices as currents carrying ice; pressing warms a voice's crystals to lamp light and swells it;
 *     FREEZE sets the crystals into a lattice, the currents go straight, the waterline turns white;
 *   the sky: GLINT's arpeggio as glints, each always at the same place for its note, with ping-pong echoes (TYPE ARP,
 *     in MODE's order); or its sequence (TYPE SEQ): sixteen steps written once, rests among them, an accent on each beat;
 *   an LFO's page: its shape over a turn (four for RANDOM and DRIFT), a glint where it is now, and two settings it
 *     moves, each keeping its value while an ice-blue mark shows where the LFO has it;
 *   the bass: its slow swell and the eight models' shapes; the mixer: four meters ducking with PUMP.
 * Each sleeps off screen and in a hidden tab; with reduced motion, each is a still picture that still answers. */
"use strict";
(() => {
  const reduce = matchMedia("(prefers-reduced-motion: reduce)").matches;
  const small = matchMedia("(max-width: 700px), (pointer: coarse)").matches;
  const ICE = [110, 208, 255], LAMP = [242, 166, 90], FOG = [220, 227, 234], MIST = [139, 149, 164], SLATE = [74, 85, 99], WHITE = [243, 246, 249];
  const mix = (a, b, k) => [a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k, a[2] + (b[2] - a[2]) * k];
  const rgba = (c, a) => `rgba(${c[0] | 0}, ${c[1] | 0}, ${c[2] | 0}, ${Math.max(0, Math.min(1, a)).toFixed(3)})`;
  const ease = (k) => k < .5 ? 4 * k * k * k : 1 - Math.pow(-2 * k + 2, 3) / 2;
  const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
  let seed = 11;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  const gauss = () => { let u = 0, v = 0; while (!u) u = rnd(); while (!v) v = rnd(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v); };
  const UI_FONT = '600 13px "Barlow Semi Condensed", "Arial Narrow", sans-serif';

  /* a canvas that fits its box, and a loop that runs only while it is seen */
  function stage(el, draw) {
    const canvas = el.querySelector("canvas"), ctx = canvas.getContext("2d");
    const s = { el, canvas, ctx, W: 0, H: 0, dpr: 1, t: 0, seen: false, raf: 0, last: 0, wake: null, onsize: null };
    const fit = () => {
      const r = el.getBoundingClientRect();
      s.W = Math.max(1, Math.round(r.width)); s.H = Math.max(1, Math.round(r.height));
      s.dpr = Math.min(window.devicePixelRatio || 1, small ? 1.5 : 2);
      canvas.width = Math.round(s.W * s.dpr); canvas.height = Math.round(s.H * s.dpr);
      if (s.onsize) s.onsize();
      paint(0);
    };
    const paint = (dt) => { ctx.setTransform(s.dpr, 0, 0, s.dpr, 0, 0); ctx.clearRect(0, 0, s.W, s.H); draw(s, dt); };
    const frame = (now) => {
      s.raf = 0;
      if (!s.seen || document.hidden) return;
      if (small && now - s.last < 31) { s.raf = requestAnimationFrame(frame); return; }
      const dt = s.last ? Math.min(.08, (now - s.last) / 1000) : 1 / 60;
      s.last = now;
      s.t += dt;
      paint(dt);
      s.raf = requestAnimationFrame(frame);
    };
    s.wake = () => {
      if (reduce) { paint(0); return; }                     // still: one picture, the state as it is
      if (!s.raf && s.seen) { s.last = 0; s.raf = requestAnimationFrame(frame); }
    };
    new ResizeObserver(fit).observe(el);
    new IntersectionObserver((es) => { s.seen = es[es.length - 1].isIntersecting; s.wake(); }, { rootMargin: "80px" }).observe(el);
    document.addEventListener("visibilitychange", () => { if (!document.hidden) s.wake(); });
    if (document.fonts && document.fonts.ready) document.fonts.ready.then(() => paint(0));
    return s;
  }

  // ============================================================ the rivers
  function river(el, mode) {
    const freezeMode = mode === "freeze";
    const voices = (freezeMode ? [["A3", .3], ["D3", .45], ["G2", .6]] : [["A3", .3], ["D3", .47], ["G2", .64]])
      .map(([n, y], i) => ({ n, y, p: 0, ph: i * 2.1 + .4, lvl: [.82, 1, .9][i], xs: [] }));
    const WATER = freezeMode ? .8 : .84;
    let crystals = [], pressing = false, sel = 1, px = -1, py = -1, used = false;
    let frz = 0, frzTo = 0, nodes = null, riverT = 0;
    seed = freezeMode ? 23 : 11;

    const s = stage(el, draw);
    s.onsize = () => {
      seed = freezeMode ? 23 : 11;
      crystals = [];
      const per = Math.round(clamp(s.W / 1000 * (freezeMode ? 230 : 200), 70, 240));
      voices.forEach((v, vi) => {
        for (let i = 0; i < per; i++) {
          const k = rnd();
          crystals.push({ v: vi, u: rnd(), off: gauss() * .9, sp: .6 + rnd() * .8, ph: rnd() * 6.3, a: .5 + rnd() * .5,
                          kind: k < .08 ? 2 : k < .45 ? 1 : 0, len: 2 + rnd() * 3, extra: rnd() < .32, warm: .7 + rnd() * .3 });
        }
      });
      nodes = null;
      if (frzTo) lattice();
    };

    const G = () => clamp(s.W / 70, 9, 15);                // the lattice's step
    const baseY = (v) => v.y * s.H;
    const curY = (v, x, tt, still) => baseY(v) + still * (s.H * .014 * Math.sin(x * .0062 + tt * .45 + v.ph) + s.H * .007 * Math.sin(x * .017 - tt * .3 + v.ph * 2));

    function lattice() {                                    // each crystal's place in the ice, and the ice's links
      const g = G();
      nodes = new Map();
      for (const c of crystals) {
        if (c.extra) continue;
        const v = voices[c.v];
        const x = (c.u * (s.W + 40)) % (s.W + 40) - 20;
        const row = clamp(Math.round(c.off * 1.1), -1, 1);
        const col = Math.round((x - (row & 1 ? g / 2 : 0)) / g);
        c.lx = col * g + (row & 1 ? g / 2 : 0);
        c.ly = baseY(v) + row * g * .5;
        nodes.set(`${c.v}:${row}:${col}`, [c.lx, c.ly]);
      }
    }

    function draw(s, dt) {
      const { ctx, W, H } = s;
      const still = 1 - ease(frz);
      if (!reduce) {
        riverT += dt * still;
        for (const v of voices) {
          const to = pressing && voices[sel] === v ? 1 : 0;
          v.p += (to - v.p) * Math.min(1, dt * (to > v.p ? 2.6 : 1.0));
        }
        if (frz !== frzTo) frz = frzTo ? Math.min(1, frz + dt / 1.5) : Math.max(0, frz - dt / 1.2);
      } else {
        voices.forEach((v) => (v.p = pressing && voices[sel] === v ? 1 : 0));
        frz = frzTo;
      }
      const tt = riverT + 30, f = ease(frz);
      const wy = H * WATER;

      // the water under the line
      let g = ctx.createLinearGradient(0, wy, 0, H);
      g.addColorStop(0, rgba([16, 30, 44], .7)); g.addColorStop(1, rgba([10, 13, 18], 0));
      ctx.fillStyle = g; ctx.fillRect(0, wy, W, H - wy);
      g = ctx.createLinearGradient(0, 0, W, 0);
      const wc = mix(SLATE, WHITE, f);
      g.addColorStop(0, rgba(wc, 0)); g.addColorStop(.15, rgba(wc, .55 + .3 * f)); g.addColorStop(.85, rgba(wc, .55 + .3 * f)); g.addColorStop(1, rgba(wc, 0));
      ctx.fillStyle = g; ctx.fillRect(0, Math.round(wy), W, 1);

      // the hand: a warm light where it presses
      if (!freezeMode && px >= 0) {
        const pp = voices[sel].p;
        if (pp > .01) {
          g = ctx.createRadialGradient(px, py, 0, px, py, Math.min(W, H) * .5);
          g.addColorStop(0, rgba(LAMP, .12 * pp)); g.addColorStop(1, rgba(LAMP, 0));
          ctx.fillStyle = g; ctx.fillRect(0, 0, W, H);
        }
      }

      for (const v of voices) {
        const p = v.p, by = baseY(v);
        // BREATH: the mist around the current
        g = ctx.createLinearGradient(0, by - H * .09, 0, by + H * .09);
        const mc = mix(MIST, LAMP, p * .8);
        g.addColorStop(0, rgba(mc, 0)); g.addColorStop(.5, rgba(mc, .05 + .06 * p)); g.addColorStop(1, rgba(mc, 0));
        ctx.fillStyle = g; ctx.fillRect(0, by - H * .09, W, H * .18);
        // CHOIR: the current
        const lc = mix(mix(FOG, LAMP, p * .85), WHITE, f * .5);
        ctx.beginPath();
        for (let x = -10; x <= W + 10; x += 12) {
          const y = curY(v, x, tt, still);
          x < 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
        }
        if (p > .02) { ctx.strokeStyle = rgba(LAMP, .16 * p); ctx.lineWidth = 7; ctx.stroke(); }
        const fade = ctx.createLinearGradient(0, 0, W, 0);
        const la = (.22 + .18 * v.lvl + .4 * p + .25 * f);
        fade.addColorStop(0, rgba(lc, 0)); fade.addColorStop(.12, rgba(lc, la)); fade.addColorStop(.88, rgba(lc, la)); fade.addColorStop(1, rgba(lc, 0));
        ctx.strokeStyle = fade; ctx.lineWidth = 1 + .5 * p + .3 * f; ctx.stroke();
        // its note, on the left
        ctx.font = UI_FONT;
        ctx.fillStyle = rgba(mix(MIST, LAMP, Math.min(1, p * 1.6)), .95);
        ctx.fillText(v.n, W * .075, by - 9);
      }

      // the lattice's links, when frozen
      if (nodes && f > .02) {
        const gg = G();
        ctx.strokeStyle = rgba(ICE, .32 * f); ctx.lineWidth = .8;
        ctx.beginPath();
        for (const [key, [x, y]] of nodes) {
          const [vi, row, col] = key.split(":").map(Number);
          const r = nodes.get(`${vi}:${row}:${col + 1}`);
          if (r) { ctx.moveTo(x, y); ctx.lineTo(r[0], r[1]); }
          const down = row < 1 && (nodes.get(`${vi}:${row + 1}:${col + (row & 1 ? 1 : 0)}`) || nodes.get(`${vi}:${row + 1}:${col}`));
          if (down && Math.abs(down[0] - x) < gg) { ctx.moveTo(x, y); ctx.lineTo(down[0], down[1]); }
        }
        ctx.stroke();
      }

      // PARTIALS: the crystals in the current
      for (const c of crystals) {
        const v = voices[c.v], p = v.p;
        if (c.extra && p < .03 && f < .5) continue;
        c.u += dt * still * c.sp * (14 / Math.max(400, W));
        const x0 = (c.u * (W + 40)) % (W + 40) - 20;
        const spread = H * .034 * (1 + 1.3 * p);
        const y0 = curY(v, x0, tt, still) + (c.off + .25 * Math.sin(tt * .8 + c.ph)) * spread;
        let x = x0, y = y0;
        if (f > 0 && c.lx !== undefined) { x = x0 + (c.lx - x0) * f; y = y0 + (c.ly - y0) * f; }
        const edge = clamp(Math.min(x, W - x) / (W * .1), 0, 1);
        let a = c.a * edge * (c.extra ? p * (1 - f) : 1) * (.7 + .3 * Math.sin(tt * 1.3 + c.ph));
        if (f > 0 && c.extra) a *= 1 - f;
        if (a < .02) continue;
        const col = mix(mix(ICE, LAMP, Math.min(1, p * c.warm * 1.15)), mix(ICE, WHITE, .35), f * .6);
        ctx.fillStyle = rgba(col, a + .2 * f);
        if (f > .6 || c.kind === 2) {                      // a disc, or the ice set: a small cross
          const r = f > .6 ? 2.2 + .6 * f : 2;
          ctx.fillRect(x - r, y - .5, r * 2, 1); ctx.fillRect(x - .5, y - r, 1, r * 2);
        } else if (c.kind === 1) ctx.fillRect(x - c.len / 2, y - .55, c.len * (1 + p * .5), 1.1);
        else ctx.fillRect(x - .8, y - .8, 1.6 + p * .7, 1.6 + p * .7);
      }

    }

    // ---------------------------------------------------------- the hand
    if (!freezeMode) {
      const nearest = (y) => {
        let best = 0, d = 1e9;
        voices.forEach((v, i) => { const dd = Math.abs(baseY(v) - y); if (dd < d) { d = dd; best = i; } });
        return best;
      };
      const at = (e) => { const r = el.getBoundingClientRect(); px = e.clientX - r.left; py = e.clientY - r.top; };
      const down = (e) => {
        if (e.button > 0) return;
        at(e);
        sel = nearest(py);
        pressing = true; used = true;
        el.classList.add("pressing", "used");
        el.setAttribute("aria-pressed", "true");
        try { el.setPointerCapture(e.pointerId); } catch (_) { /* fine */ }
        s.wake();
      };
      const up = () => {
        if (!pressing) return;
        pressing = false;
        el.classList.remove("pressing");
        el.setAttribute("aria-pressed", "false");
        s.wake();
      };
      el.addEventListener("pointerdown", down);
      el.addEventListener("pointermove", (e) => { if (pressing) { at(e); sel = nearest(py); } });
      el.addEventListener("pointerup", up);
      el.addEventListener("pointercancel", up);
      el.addEventListener("lostpointercapture", up);
      el.addEventListener("contextmenu", (e) => e.preventDefault());
      el.addEventListener("keydown", (e) => {
        if (e.key === " " || e.key === "Enter") {
          e.preventDefault();
          if (e.repeat) return;
          px = s.W * .55; py = baseY(voices[sel]);
          pressing = true; used = true; el.classList.add("pressing", "used"); el.setAttribute("aria-pressed", "true"); s.wake();
        } else if (e.key === "ArrowUp" || e.key === "ArrowDown") {
          e.preventDefault();
          sel = clamp(sel + (e.key === "ArrowUp" ? -1 : 1), 0, voices.length - 1);
          py = baseY(voices[sel]);
          s.wake();
        }
      });
      el.addEventListener("keyup", (e) => { if (e.key === " " || e.key === "Enter") up(); });
      el.addEventListener("click", (e) => {
        if (e.detail !== 0 || pressing) return;
        px = s.W * .55; py = baseY(voices[sel]);
        pressing = true; used = true; el.classList.add("pressing", "used"); el.setAttribute("aria-pressed", "true"); s.wake();
        setTimeout(up, 1600);
      });
      el.addEventListener("blur", up);
    } else {
      const btn = el.querySelector(".freeze-btn"), val = el.querySelector(".freeze-val");
      btn.addEventListener("click", () => {
        frzTo = frzTo ? 0 : 1;
        if (frzTo) lattice();
        btn.setAttribute("aria-pressed", String(!!frzTo));
        val.textContent = frzTo ? "ON" : "OFF";
        s.wake();
      });
    }
    return s;
  }

  document.querySelectorAll("[data-river]").forEach((el) => river(el, el.dataset.river));

  // ============================================================ GLINT's sky
  const sky = document.querySelector("[data-sky]");
  if (sky) {
    // a D minor seventh, over two octaves: eight steps; each note's glint has its place in the sky
    const NOTES = [62, 65, 69, 72];
    const RANGE = 2, STEP = .42, DELAY = STEP * 1.5, FB = .5;
    const glints = [], echoes = [];
    let type = "ARP", mode = "UP", idx = -1, dir = 1, next = .4, held = null;
    const count = NOTES.length * RANGE;
    const SEQ = [0, 4, 2, -1, 1, 5, 3, -1, 2, 6, 4, 3, 1, -1, 5, 7];   // SEQ: each step a place, or a rest (-1)
    seed = 5;
    const place = Array.from({ length: count }, (_, i) => ({ x: .1 + .8 * (i + .5) / count + (rnd() - .5) * .05, y: .62 - .42 * (i / (count - 1)) + (rnd() - .5) * .1 }));
    const bgStars = Array.from({ length: 70 }, () => ({ x: rnd(), y: rnd() * .85, a: .1 + rnd() * .3 }));
    function step() {                                      // the firmware's arpeggiator (app/app.c)
      if (type === "SEQ") { idx = (idx + 1) % SEQ.length; return SEQ[idx]; }   // the step's note, or a rest
      if (mode === "UP") idx = (idx + 1) % count;
      else if (mode === "DOWN") idx = idx <= 0 ? count - 1 : idx - 1;
      else if (mode === "UPDOWN") {
        idx += dir;
        if (idx >= count) { idx = count > 1 ? count - 2 : 0; dir = -1; } else if (idx < 0) { idx = count > 1 ? 1 : 0; dir = 1; }
      } else if (mode === "RANDOM") idx = Math.floor(Math.random() * count);
      else {                                               // DRIFT: a step either way, or stay
        idx = idx < 0 ? 0 : idx + Math.floor(Math.random() * 3) - 1;
        if (idx < 0) idx = count > 1 ? 1 : 0;
        if (idx >= count) idx = count > 1 ? count - 2 : 0;
      }
      return idx;
    }
    function strike(x, y, size, at) {
      if (reduce) { at -= .06; if (glints.length > 12) glints.shift(); }
      const gl = { x, y, size, t0: at, life: 1.9 + size * .9, press: 0 };
      glints.push(gl);
      if (reduce) return gl;                               // still: no echoes to come (time does not pass)
      for (let k = 1; k <= 3; k++) echoes.push({ x: k % 2 ? 1 - x : x, y: y + k * .015, size: size * Math.pow(FB, k) * 1.3, t0: at + DELAY * k, life: 1.4, a: Math.pow(FB, k) });
      return gl;
    }
    function star(ctx, x, y, r, a, c) {
      const g = ctx.createRadialGradient(x, y, 0, x, y, r * 2.4);
      g.addColorStop(0, rgba(c, .5 * a)); g.addColorStop(1, rgba(c, 0));
      ctx.fillStyle = g; ctx.fillRect(x - r * 2.4, y - r * 2.4, r * 4.8, r * 4.8);
      ctx.fillStyle = rgba(WHITE, a);
      ctx.fillRect(x - 1, y - 1, 2, 2);
      let lg = ctx.createLinearGradient(x - r * 2, 0, x + r * 2, 0);
      lg.addColorStop(0, rgba(c, 0)); lg.addColorStop(.5, rgba(c, a)); lg.addColorStop(1, rgba(c, 0));
      ctx.fillStyle = lg; ctx.fillRect(x - r * 2, y - .5, r * 4, 1);
      lg = ctx.createLinearGradient(0, y - r * 1.5, 0, y + r * 1.5);
      lg.addColorStop(0, rgba(c, 0)); lg.addColorStop(.5, rgba(c, a)); lg.addColorStop(1, rgba(c, 0));
      ctx.fillStyle = lg; ctx.fillRect(x - .5, y - r * 1.5, 1, r * 3);
    }
    [[1, 1.2], [4, .7], [6, .25]].forEach(([i, age]) => strike(place[i].x, place[i].y, 1, -age));
    const s = stage(sky, (s, dt) => {
      const { ctx, W, H, t } = s;
      for (const b of bgStars) { ctx.fillStyle = rgba(FOG, b.a); ctx.fillRect(b.x * W, b.y * H, 1, 1); }
      const wy = H * .9;
      const g = ctx.createLinearGradient(0, 0, W, 0);
      g.addColorStop(0, rgba(SLATE, 0)); g.addColorStop(.2, rgba(SLATE, .5)); g.addColorStop(.8, rgba(SLATE, .5)); g.addColorStop(1, rgba(SLATE, 0));
      ctx.fillStyle = g; ctx.fillRect(0, Math.round(wy), W, 1);
      if (reduce && !glints.length) place.forEach((p, i) => i % 3 === 0 && strike(p.x, p.y, 1, t - .2 - i * .05));
      if (!reduce && t >= next) {
        const i = step();
        if (i >= 0) strike(place[i].x, place[i].y, type === "SEQ" ? (idx % 4 === 0 ? 1.3 : .75) : .9 + .2 * (i % NOTES.length === 0), t);
        next = t + STEP;
      }
      if (held) { held.press = Math.min(1, held.press + dt * 1.2); held.size = 1 + held.press * .8; held.life = 2.4 + held.press * 2.4; held.t0 = Math.max(held.t0, t - .06); }
      for (const list of [echoes, glints]) {
        for (let i = list.length - 1; i >= 0; i--) {
          const e = list[i], age = t - e.t0;
          if (age < 0) continue;
          if (age > e.life) { list.splice(i, 1); continue; }
          const env = (age < .04 ? age / .04 : Math.exp(-(age - .04) * 3.2 / e.life)) * (e.a || 1);
          const r = (5 + 9 * e.size) * (1 + .15 * (1 - env));
          star(ctx, e.x * W, e.y * H, r, env, e.a ? mix(ICE, FOG, .25) : ICE);
          // its light on the water
          if (e.y * H < wy) { ctx.fillStyle = rgba(ICE, env * .25); ctx.fillRect(e.x * W - r * .6, wy + 6 + (1 - e.y) * 8, r * 1.2, 1); }
        }
      }
    });
    const strikeBtn = sky.querySelector(".sky-strike");
    strikeBtn.addEventListener("pointerdown", (e) => {
      const r = sky.getBoundingClientRect();
      held = strike((e.clientX - r.left) / r.width, clamp((e.clientY - r.top) / r.height, .08, .8), 1, s.t);
      try { strikeBtn.setPointerCapture(e.pointerId); } catch (_) { /* fine */ }
      s.wake();
    });
    const letGo = () => { held = null; };
    strikeBtn.addEventListener("pointerup", letGo);
    strikeBtn.addEventListener("pointercancel", letGo);
    strikeBtn.addEventListener("keydown", (e) => {
      if ((e.key === " " || e.key === "Enter") && !e.repeat) { e.preventDefault(); strike(.15 + Math.random() * .7, .15 + Math.random() * .5, 1.3, s.t); s.wake(); }
    });
    strikeBtn.addEventListener("click", (e) => e.preventDefault());
    const modes = sky.querySelector(".sky-modes");
    const setType = (t) => {                             // TYPE: the arpeggiator, or the sequencer in its place
      type = t; idx = -1; dir = 1;
      sky.querySelectorAll(".chip[data-type]").forEach((o) => o.setAttribute("aria-pressed", String(o.dataset.type === t)));
      if (modes) modes.classList.toggle("off", t !== "ARP");
    };
    sky.querySelectorAll(".chip[data-type]").forEach((b) => b.addEventListener("click", () => { setType(b.dataset.type); s.wake(); }));
    sky.querySelectorAll(".chip[data-mode]").forEach((b) => b.addEventListener("click", () => {
      mode = b.dataset.mode;                             // a MODE: the arpeggiator's, so TYPE on ARP
      sky.querySelectorAll(".chip[data-mode]").forEach((o) => o.setAttribute("aria-pressed", String(o === b)));
      setType("ARP");
      s.wake();
    }));
  }

  // ============================================================ an LFO's page, as the screen draws it (app/ui.c, lfo_scope)
  const lfoBox = document.querySelector("[data-lfo]");
  if (lfoBox) {
    const fig = lfoBox.closest(".lfo-fig") || document;
    const marks = Array.from(fig.querySelectorAll(".gauge")).map((g) => ({ el: g.querySelector(".gauge-lfo"), v: parseFloat(g.style.getPropertyValue("--v")) || .5 }));
    const D = .2 + .8 * .6, AMOUNT = .24;              // DEPTH a little past the middle; how far the two settings move
    let shape = 0;
    const rndv = (c) => {                                // a turn's own value, -1..1: the same turn, the same value
      let h = Math.imul((c | 0) ^ 0x2545f491, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16;
      return (h >>> 8) / 8388608 - 1;
    };
    const wave = (x, c) => {                             // SHAPE at x (0..1) of turn c, -1..1 (app/app.c, lfo_shape)
      switch (shape) {
        case 0: return Math.sin(2 * Math.PI * x);
        case 1: return x < .25 ? 4 * x : x < .75 ? 2 - 4 * x : 4 * x - 4;
        case 2: return 2 * x - 1;
        case 3: return 1 - 2 * x;
        case 4: return x < .5 ? 1 : -1;
        case 5: return rndv(c);
        default: { const a = rndv(c), b = rndv(c + 1); return a + (b - a) * x * x * (3 - 2 * x); }
      }
    };
    const s = stage(lfoBox, (s) => {
      const { ctx, W, H } = s, rnd = shape >= 5, turns = rnd ? 4 : 1, T = rnd ? 2.4 : 4.2;   // seconds a turn
      const tt = reduce ? T * 1.12 : s.t + T * .12;
      const cyc = Math.floor(tt / T), ph = tt / T - cyc;
      const top = H * .1, bot = H * .9, base = (top + bot) / 2, half = (bot - top) / 2 * D;
      const yOf = (v) => base - v * half;
      let g = ctx.createLinearGradient(0, 0, W, 0);       // the strip's middle line
      g.addColorStop(0, rgba(SLATE, .15)); g.addColorStop(.5, rgba(SLATE, .6)); g.addColorStop(1, rgba(SLATE, .15));
      ctx.fillStyle = g; ctx.fillRect(0, Math.round(base), W, 1);
      // its shape over a turn; RANDOM and DRIFT: the three turns before and this one
      ctx.beginPath();
      for (let x = 0; x <= W; x += 1.5) {
        const u = Math.min(x / W, .99999) * turns, k = Math.floor(u);
        const y = yOf(wave(rnd ? u - k : u, cyc + k - 3));
        x ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      }
      ctx.lineJoin = "round";
      ctx.strokeStyle = rgba(ICE, .08); ctx.lineWidth = 7; ctx.stroke();
      ctx.strokeStyle = rgba(ICE, .85); ctx.lineWidth = 1.5; ctx.stroke();
      // where it is now: a hairline across the strip and a glint
      const now = wave(ph, cyc), gx = (rnd ? (3 + ph) / 4 : ph) * W, gy = yOf(now);
      ctx.fillStyle = rgba(ICE, .2); ctx.fillRect(Math.round(gx), top - 10, 1, bot - top + 20);
      g = ctx.createRadialGradient(gx, gy, 0, gx, gy, 14);
      g.addColorStop(0, rgba(ICE, .55)); g.addColorStop(1, rgba(ICE, 0));
      ctx.fillStyle = g; ctx.fillRect(gx - 14, gy - 14, 28, 28);
      for (const [dx, dy, len] of [[1, 0, 18], [0, 1, 13]]) {
        g = ctx.createLinearGradient(gx - dx * len, gy - dy * len, gx + dx * len, gy + dy * len);
        g.addColorStop(0, rgba(ICE, 0)); g.addColorStop(.5, rgba(ICE, .9)); g.addColorStop(1, rgba(ICE, 0));
        ctx.fillStyle = g;
        ctx.fillRect(dx ? gx - len : gx - .5, dy ? gy - len : gy - .5, dx ? len * 2 : 1, dy ? len * 2 : 1);
      }
      ctx.fillStyle = rgba(WHITE, 1); ctx.fillRect(gx - 1.5, gy - 1.5, 3, 3);
      // the two settings keep their values; the LFO moves them both by the same amount, around them (BIPOLAR)
      for (const m of marks) if (m.el) m.el.style.setProperty("--l", clamp(m.v + now * AMOUNT, 0, 1).toFixed(4));
    });
    fig.querySelectorAll(".chip[data-shape]").forEach((b) => b.addEventListener("click", () => {
      shape = parseInt(b.dataset.shape, 10) || 0;
      fig.querySelectorAll(".chip[data-shape]").forEach((o) => o.setAttribute("aria-pressed", String(o === b)));
      s.wake();
    }));
  }

  // ============================================================ MASTER FILTER, LOFI TAPE, MASTER EQ: their pages' pictures
  // (app/ui.c filter_scope, tape_scope, eq_scope), each over the spectrum's grid or as the cassette; under each its page:
  // K1 a choice (chips), K2..K4 the screen's gauges, following what the picture does
  const FS = 44100, NIGHT = [10, 13, 18];
  const fmtDb = (db) => { const r = Math.round(db * 2) / 2; return (r > 0 ? "+" : r < 0 ? "−" : "") + String(Math.abs(r)); };
  const hzText = (hz) => hz < 1000 ? [String(Math.round(hz)), "HZ"] : [(hz < 9950 ? (Math.round(hz / 100) / 10).toFixed(1) : String(Math.round(hz / 1000))), "KHZ"];
  /* a gauge of the page: its value (and unit) and where its line is lit; written only when it changes */
  function gauge(card, key) {
    const b = card.querySelector(`[data-g="${key}"]`), g = b && b.closest(".gauge");
    let was = "";
    return (txt, unit, v) => {
      if (!b) return;
      const k = txt + "|" + unit + "|" + v.toFixed(3);
      if (k === was) return;
      was = k;
      b.textContent = txt;
      if (unit) { const sm = document.createElement("small"); sm.textContent = unit; b.appendChild(sm); }
      g.style.setProperty("--v", clamp(v, 0, 1).toFixed(3));
    };
  }
  /* K1's chips: the one pressed, the card grey when it is OFF */
  function chips(card, val, pick) {
    const bs = Array.from(card.querySelectorAll(".fx-k1 .chip"));
    const show = (v) => {
      bs.forEach((b) => b.setAttribute("aria-pressed", String(+b.dataset.v === v)));
      card.classList.toggle("off", v === 0);
    };
    bs.forEach((b) => b.addEventListener("click", () => { pick(+b.dataset.v); show(+b.dataset.v); }));
    show(val);
    return show;
  }
  /* the screen's fills are dithered, a pixel in two: a mask of it, a CSS pixel a dot */
  function ditherer(s) {
    const n = Math.max(1, Math.round(s.dpr)), tile = document.createElement("canvas");
    tile.width = tile.height = 2 * n;
    const tg = tile.getContext("2d");
    tg.fillStyle = "#fff"; tg.fillRect(n, 0, n, n); tg.fillRect(0, n, n, n);
    const layer = document.createElement("canvas"), lg = layer.getContext("2d");
    layer.width = s.canvas.width; layer.height = s.canvas.height;
    const pat = lg.createPattern(tile, "repeat");
    /* fill(lg) draws on the layer (CSS pixels); then the layer, dithered, onto the picture */
    return (fill) => {
      lg.setTransform(1, 0, 0, 1, 0, 0); lg.globalCompositeOperation = "source-over"; lg.clearRect(0, 0, layer.width, layer.height);
      lg.setTransform(s.dpr, 0, 0, s.dpr, 0, 0);
      fill(lg);
      lg.setTransform(1, 0, 0, 1, 0, 0); lg.globalCompositeOperation = "destination-in";
      lg.fillStyle = pat; lg.fillRect(0, 0, layer.width, layer.height);
      s.ctx.save(); s.ctx.setTransform(1, 0, 0, 1, 0, 0); s.ctx.drawImage(layer, 0, 0); s.ctx.restore();
    };
  }
  /* the spectrum's grid (ui.c rs_grid): 20 Hz .. 20 kHz across, 100, 1K, 10K dotted, the 0 dB line */
  function rsGrid(ctx, W, top, bed, zero, xOf) {
    ctx.fillStyle = rgba(SLATE, .55); ctx.fillRect(0, Math.round(zero), W, 1);
    ctx.font = '600 10.5px "Barlow Semi Condensed", "Arial Narrow", sans-serif';
    if ("letterSpacing" in ctx) ctx.letterSpacing = "1px";
    ctx.textAlign = "center"; ctx.textBaseline = "top";
    [[100, "100"], [1000, "1K"], [10000, "10K"]].forEach(([hz, nm]) => {
      const x = Math.round(xOf(hz));
      ctx.fillStyle = rgba(SLATE, .7);
      for (let y = top; y <= bed; y += 3) ctx.fillRect(x, y, 1, 1);
      ctx.fillStyle = rgba(MIST, .8); ctx.fillText(nm, x, bed + 7);
    });
    if ("letterSpacing" in ctx) ctx.letterSpacing = "0px";
    ctx.textAlign = "start"; ctx.textBaseline = "alphabetic";
  }
  /* a knob's badge on the curve: the blue disc and its figure, haloed while it turns */
  function badgeAt(ctx, x, y, ch, lit, off) {
    const r = lit ? 8.5 : 7.5, c = off ? SLATE : ICE;
    if (lit) {
      const g = ctx.createRadialGradient(x, y, 0, x, y, r * 2.6);
      g.addColorStop(0, rgba(ICE, .45)); g.addColorStop(1, rgba(ICE, 0));
      ctx.fillStyle = g; ctx.fillRect(x - r * 2.6, y - r * 2.6, r * 5.2, r * 5.2);
    }
    ctx.beginPath(); ctx.arc(x, y, r, 0, 2 * Math.PI); ctx.fillStyle = rgba(c, 1); ctx.fill();
    ctx.fillStyle = rgba(NIGHT, 1); ctx.font = '700 11px "Barlow Semi Condensed", "Arial Narrow", sans-serif';
    ctx.textAlign = "center"; ctx.textBaseline = "middle"; ctx.fillText(ch, x, y + .5);
    ctx.textAlign = "start"; ctx.textBaseline = "alphabetic";
  }
  const curveGlow = (ctx, col, a) => { ctx.lineJoin = "round"; ctx.strokeStyle = rgba(col, .09 * a); ctx.lineWidth = 7; ctx.stroke(); ctx.strokeStyle = rgba(col, .95 * a); ctx.lineWidth = 1.6; ctx.stroke(); };
  const spectrum = (W) => { const pad = 4; return { xOf: (hz) => pad + Math.log(hz / 20) / Math.log(1000) * (W - 2 * pad), hzOf: (x) => 20 * Math.pow(1000, (x - pad) / (W - 2 * pad)) }; };

  // ------------------------------------------------------------ MASTER FILTER: its response, CUTOFF swept as an LFO would
  const flCard = document.querySelector('.fx-card[data-fx="filter"]');
  if (flCard) {
    let type = 1, dith = null;
    const gCut = gauge(flCard, "cut"), gRes = gauge(flCard, "reso");
    const s = stage(flCard.querySelector(".fx-pic"), (s) => {
      const { ctx, W, H } = s, t = reduce ? 3.2 : s.t + 3.2;
      const top = 8, bed = H - 24, zero = top + (bed - top) / 3, pxDb = (bed - top) / 60;
      const { xOf, hzOf } = spectrum(W);
      const uc = (type === 2 ? .4 : .66) + .15 * Math.sin(2 * Math.PI * t / 13);   /* CUTOFF, its knob 0..1 */
      const ur = .5 + .14 * Math.sin(2 * Math.PI * t / 23 + 1);                    /* RESONANCE */
      const fc = 20 * Math.pow(1000, uc), q = .7071 * Math.pow(17, ur);
      const yOf = (hz) => {
        if (!type) return zero;
        const r = hz / fc, re = 1 - r * r, im = r / q;
        const db = clamp(-10 * Math.log10(re * re + im * im + 1e-12) + (type === 2 ? 40 * Math.log10(r) : 0), -40, 20);
        return zero - db * pxDb;
      };
      if (!dith) dith = ditherer(s);
      const pts = [];
      for (let x = 0; x <= W + 1; x += 1.5) pts.push([x, yOf(hzOf(x))]);
      if (type) dith((g) => {                          /* what passes: water under the curve, fading to the bed */
        g.beginPath(); pts.forEach(([x, y], i) => (i ? g.lineTo(x, y + 2) : g.moveTo(x, y + 2)));
        g.lineTo(W, bed); g.lineTo(0, bed); g.closePath();
        const gr = g.createLinearGradient(0, top, 0, bed);
        gr.addColorStop(0, rgba(ICE, .55)); gr.addColorStop(.45, rgba(ICE, .3)); gr.addColorStop(1, rgba(ICE, 0));
        g.fillStyle = gr; g.fill();
      });
      rsGrid(ctx, W, top, bed, zero, xOf);
      ctx.beginPath(); pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
      if (type) { curveGlow(ctx, ICE, 1); badgeAt(ctx, xOf(fc), clamp(yOf(fc), top + 2, bed - 2), "2", !reduce, false); }
      else { ctx.strokeStyle = rgba(SLATE, .9); ctx.lineWidth = 1.4; ctx.stroke(); }
      const [ht, hu] = hzText(fc);
      gCut(ht, hu, uc);
      gRes(String(Math.round(ur * 100)), "", ur);
    });
    s.onsize = () => { dith = null; };
    chips(flCard, type, (v) => { type = v; s.wake(); });
  }

  // ------------------------------------------------------------ LOFI TAPE: the cassette (ui.c tape_scope)
  const tpCard = document.querySelector('.fx-card[data-fx="tape"]');
  if (tpCard) {
    let on = true;
    const gSat = gauge(tpCard, "sat"), gWob = gauge(tpCard, "wob");
    /* the reels' angles, how much tape is on the right one, rewinding; the speed shown; the wow's phases and its rate
     * wandering; a drag under way (time left, its length, its depth) and the lag it left, caught up after */
    const tp = { rot: [0, 0], u: .3, rew: false, vs: 1, ph: [0, 0, 0], rate: .55, rateTo: .55, drag: 0, dragN: 0, dragS: 0, lag: 0, next: 2.5 };
    const TAPE0 = [120, 72, 40];
    const s = stage(tpCard.querySelector(".fx-pic"), (s, dt) => {
      const { ctx, W, H } = s, t = reduce ? 6 : s.t + 6;
      const sat = reduce ? .62 : .5 + .32 * Math.sin(2 * Math.PI * (t - 6) / 17 - .4);     /* SATURATION, breathing */
      const wob = reduce ? .7 : .66 + .18 * Math.sin(2 * Math.PI * (t - 6) / 29 + 1.2);    /* WOBBLE */
      const k = Math.min(W / 182, H / 76), ox = W / 2 - 120 * k, oy = H * .5 - 62 * k;
      const X = (x) => ox + x * k, Y = (y) => oy + y * k;
      const gr = 4, hy = 83 + gr, a0 = 100, a1 = 529;
      let v = 0;
      if (on && !reduce && dt > 0) {
        if (Math.random() < dt / .7 * .1) tp.rateTo = .3 + .5 * Math.random();   /* the wow's rate wanders, .3 .. .8 Hz */
        tp.rate += (tp.rateTo - tp.rate) * Math.min(1, dt * .2);
        tp.ph[0] += tp.rate * dt; tp.ph[1] += .23 * dt; tp.ph[2] += 1.7 * dt;
        if (tp.drag > 0) {                                 /* a drag: slower, most at its middle */
          const x = Math.sin(Math.PI * tp.drag / tp.dragN);
          v = tp.dragS * x * x;
          tp.lag += v * dt;
          tp.drag -= dt;
        } else {                                           /* on at speed, catching up; now and then a drag */
          tp.lag -= tp.lag * Math.min(1, dt / 1.4);
          if (s.t > tp.next && Math.random() < (.06 + .32 * wob * wob) * dt) {
            const r1 = Math.random();
            tp.dragN = tp.drag = .25 * Math.pow(2, 2.4 * r1 * r1);              /* a quarter of a second .. 1.3 s */
            tp.dragS = Math.min(.96, wob * (.95 + .4 * Math.random()));
            tp.next = s.t + tp.dragN + 2.5;
          }
        }
      }
      const wow = on ? wob * (.08 * Math.sin(2 * Math.PI * tp.ph[0]) + .025 * Math.sin(2 * Math.PI * tp.ph[1]) + .02 * Math.sin(2 * Math.PI * tp.ph[2])) : 0;
      const spd = on ? Math.max(.03, 1 + wow - v + (tp.drag > 0 ? 0 : tp.lag / 1.4)) : 1;
      tp.vs += (spd - tp.vs) * Math.min(1, dt * 6.7);
      const pl = Math.sqrt(a0 + (1 - tp.u) * (a1 - a0)), pr = Math.sqrt(a0 + tp.u * (a1 - a0));
      if (on && !reduce) {                                 /* 60 px a second past the head; at the end, rewound quickly */
        const d = (tp.rew ? -400 : 60 * tp.vs) * dt;
        tp.u += tp.rew ? -dt / 1.5 : d / 3000;
        if (tp.u >= 1) tp.rew = true;
        if (tp.u <= 0) { tp.u = 0; tp.rew = false; }
        tp.u = Math.min(1, tp.u);
        tp.rot[0] -= d / (2 * Math.PI * pl);               /* anticlockwise, each at its radius */
        tp.rot[1] -= d / (2 * Math.PI * pr);
      }
      const tape = on ? mix(TAPE0, LAMP, sat) : [38, 48, 60], hub = on ? FOG : SLATE, rim = on ? SLATE : [38, 48, 60];
      if (on && sat > 0) {                                 /* the head's glow: wider and whiter as SATURATION grows */
        const rad = (10 + 22 * sat) * k, gl = (.35 + .65 * sat) * (reduce ? 1 : .85 + .15 * Math.sin(t * 2.3 * 2 * Math.PI) * Math.sin(t * .37 * 2 * Math.PI));
        ctx.save(); ctx.translate(X(120), Y(hy)); ctx.scale(1.6, 1);
        const g = ctx.createRadialGradient(0, 0, 0, 0, 0, rad);
        g.addColorStop(0, rgba(mix(LAMP, [240, 236, 230], clamp((sat - .35) * 2, 0, 1)), .9 * gl));
        g.addColorStop(.3, rgba(LAMP, .5 * gl)); g.addColorStop(1, rgba(LAMP, 0));
        ctx.fillStyle = g; ctx.fillRect(-rad, -rad, 2 * rad, 2 * rad);
        ctx.restore();
      }
      const reel = (cx, pack, rot) => {                    /* its flange, the tape wound on it, its hub and spokes */
        const x = X(cx), y = Y(60);
        ctx.lineWidth = 1.6 * k / 1.6; ctx.strokeStyle = rgba(rim, 1);
        ctx.beginPath(); ctx.arc(x, y, 25.5 * k, 0, 2 * Math.PI); ctx.stroke();
        ctx.fillStyle = rgba(tape, 1);
        ctx.beginPath(); ctx.arc(x, y, pack * k, 0, 2 * Math.PI); ctx.arc(x, y, 9.5 * k, 0, 2 * Math.PI, true); ctx.fill();
        ctx.strokeStyle = rgba(NIGHT, .35); ctx.lineWidth = .6 * k;
        for (let r = 11; r < pack; r += 2) { ctx.beginPath(); ctx.arc(x, y, r * k, 0, 2 * Math.PI); ctx.stroke(); }
        ctx.strokeStyle = rgba(hub, 1); ctx.lineWidth = 1.4 * k / 1.4;
        ctx.beginPath(); ctx.arc(x, y, 7 * k, 0, 2 * Math.PI); ctx.stroke();
        const ms = Math.sin(2 * Math.PI * (rot + .125)), mc = Math.sin(2 * Math.PI * (rot + .375));
        ctx.lineWidth = 1.2 * k / 1.4;
        ctx.beginPath(); ctx.moveTo(x + (pack - 5) * k * mc, y + (pack - 5) * k * ms); ctx.lineTo(x + (pack - .5) * k * mc, y + (pack - .5) * k * ms); ctx.stroke();
        ctx.lineWidth = 2.2 * k / 1.4; ctx.lineCap = "round";
        for (let j = 0; j < 3; j++) {
          const a = rot + j / 3, sn = Math.sin(2 * Math.PI * a), cs = Math.cos(2 * Math.PI * a);
          ctx.beginPath(); ctx.moveTo(x + 1.5 * k * cs, y + 1.5 * k * sn); ctx.lineTo(x + 6.5 * k * cs, y + 6.5 * k * sn); ctx.stroke();
        }
        ctx.lineCap = "butt";
      };
      reel(60, pl, tp.rot[0]);
      reel(180, pr, tp.rot[1]);
      const tangent = (ax, ay, ar, bx, by, br) => {      /* where the tape leaves circle a for circle b, under both */
        const dx = bx - ax, dy = by - ay, l = Math.hypot(dx, dy) + 1e-6, ux = dx / l, uy = dy / l, c = (ar - br) / l, sn = Math.sqrt(Math.max(0, 1 - c * c));
        let nx = c * ux - sn * uy, ny = c * uy + sn * ux;
        if (ny < 0) { nx = c * ux + sn * uy; ny = c * uy - sn * ux; }
        return [ax + ar * nx, ay + ar * ny, bx + br * nx, by + br * ny];
      };
      const span = (p, sag) => {                           /* a free span, hanging sag at its middle */
        const dx = p[2] - p[0], dy = p[3] - p[1], l = Math.hypot(dx, dy) + 1e-6;
        let nx = -dy / l, ny = dx / l;
        if (ny < 0) { nx = -nx; ny = -ny; }
        ctx.beginPath(); ctx.moveTo(X(p[0]), Y(p[1]));
        ctx.quadraticCurveTo(X(.5 * (p[0] + p[2]) + 2 * sag * nx), Y(.5 * (p[1] + p[3]) + 2 * sag * ny), X(p[2]), Y(p[3]));
        ctx.strokeStyle = rgba(tape, 1); ctx.lineWidth = 1.3 * k / 1.3; ctx.stroke();
      };
      const loose = on ? Math.min(10, wob * (4 + 3 * Math.sin(2 * Math.PI * t * .7)) + 14 * Math.max(0, 1 - tp.vs)) : 0;
      span(tangent(60, 60, pl, 100, 83, gr), loose);
      span(tangent(140, 83, gr, 180, 60, pr), on ? wob * (4 + 3 * Math.sin(2 * Math.PI * (t * .7 + .4))) : 0);
      ctx.strokeStyle = rgba(on && sat > .3 ? mix(LAMP, FOG, sat) : tape, 1);   /* over the head: taut, hot as it saturates */
      ctx.lineWidth = (1.2 + .8 * sat) * k / 1.3;
      ctx.beginPath(); ctx.moveTo(X(100), Y(hy)); ctx.lineTo(X(140), Y(hy)); ctx.stroke();
      ctx.strokeStyle = rgba(hub, 1); ctx.lineWidth = 1.1 * k / 1.2;              /* the rollers: rings, a pin */
      [100, 140].forEach((x) => {
        ctx.beginPath(); ctx.arc(X(x), Y(83), (gr - .5) * k, 0, 2 * Math.PI); ctx.stroke();
        ctx.fillStyle = rgba(hub, 1); ctx.fillRect(X(x) - .9, Y(83) - .9, 1.8, 1.8);
      });
      ctx.fillStyle = rgba(on ? (sat > .5 ? mix(LAMP, FOG, (sat - .5) * 1.2) : LAMP) : SLATE, 1);   /* the head, its gap */
      ctx.fillRect(X(113), Y(hy + 1), 15 * k, 4 * k);
      ctx.fillStyle = rgba(NIGHT, 1); ctx.fillRect(X(120), Y(hy + 1), Math.max(1, k), 4 * k);
      gSat(String(Math.round(sat * 100)), "", sat);
      gWob(String(Math.round(wob * 100)), "", wob);
    });
    chips(tpCard, 1, (v) => { on = v === 1; s.wake(); });
  }

  // ------------------------------------------------------------ MASTER EQ: its curve, stepping through the presets
  const eqCard = document.querySelector('.fx-card[data-fx="eq"]');
  if (eqCard) {
    /* each preset's LOW, MID, HIGH (half decibels, 24: 0 dB), LOW FREQ, MID FREQ, Q, HIGH FREQ (knobs, 0..127): app/params.c */
    const PRESETS = { 2: [24, 24, 24, 59, 64, 39, 90], 3: [29, 24, 20, 68, 64, 39, 83], 4: [24, 24, 30, 59, 64, 39, 102], 5: [24, 17, 24, 59, 34, 39, 90],
                      6: [24, 18, 24, 59, 51, 53, 90], 7: [30, 22, 29, 51, 64, 24, 96], 8: [12, 28, 24, 59, 87, 29, 90] };
    const CYCLE = [3, 4, 5, 6, 7, 8];
    const lowF = (u) => 30 * Math.pow(20, u), midF = (u) => 80 * Math.pow(150, u), highF = (u) => 1500 * Math.pow(10.6667, u), qOf = (u) => .4 * Math.pow(20, u);
    function coefs(kind, hz, db, q) {                    /* app/dsp/frasil.c frasil_eq_coefs: a shelf, a bell, a shelf */
      const A = Math.pow(10, db / 40), w = 2 * Math.PI * hz / FS, cw = Math.cos(w), sw = Math.sin(w);
      let b0, b1, b2, a0, a1, a2;
      if (kind === 1) {
        const al = sw / (2 * q);
        b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A;
      } else {
        const sq = 2 * Math.sqrt(A) * sw * .70710678, ap = A + 1, am = A - 1, sg = kind ? -1 : 1;
        b0 = A * (ap - sg * am * cw + sq); b1 = sg * 2 * A * (am - sg * ap * cw); b2 = A * (ap - sg * am * cw - sq);
        a0 = ap + sg * am * cw + sq; a1 = -sg * 2 * (am + sg * ap * cw); a2 = ap + sg * am * cw - sq;
      }
      return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0];
    }
    function bqDb(c, hz) {
      const w = 2 * Math.PI * hz / FS, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
      const nr = c[0] + c[1] * c1 + c[2] * c2, ni = c[1] * s1 + c[2] * s2, dr = 1 + c[3] * c1 + c[4] * c2, di = c[3] * s1 + c[4] * s2;
      return 10 * Math.log10((nr * nr + ni * ni + 1e-12) / (dr * dr + di * di + 1e-12));
    }
    let preset = 3, auto = true, ci = 0, nextAt = 3.6, morph = 1, dith = null;
    let from = PRESETS[3].slice(), to = PRESETS[3].slice(), cur = PRESETS[3].slice();
    const gLow = gauge(eqCard, "low"), gMid = gauge(eqCard, "mid"), gHigh = gauge(eqCard, "high");
    let showChips = null;
    const go = (p) => {                                  /* a preset: its bands glide there (OFF: they stay, unheard) */
      preset = p;
      if (p) { from = cur.slice(); to = PRESETS[p].slice(); morph = reduce ? 1 : 0; if (reduce) cur = to.slice(); }
    };
    const s = stage(eqCard.querySelector(".fx-pic"), (s, dt) => {
      const { ctx, W, H } = s;
      if (auto && !reduce && s.t >= nextAt) {            /* the presets one after another, until a hand picks one */
        ci = (ci + 1) % CYCLE.length; go(CYCLE[ci]); if (showChips) showChips(preset);
        nextAt = s.t + 3.6;
      }
      if (morph < 1) {
        morph = Math.min(1, morph + dt / .9);
        const e = ease(morph);
        cur = from.map((a, i) => a + (to[i] - a) * e);
      }
      const off = preset === 0;
      const top = 10, bed = H - 24, zero = (top + bed) / 2, pxDb = (zero - top) / 12;
      const { xOf, hzOf } = spectrum(W);
      const fl = lowF(cur[3] / 127), fm = midF(cur[4] / 127), fh = highF(cur[6] / 127);
      const bands = [coefs(0, fl, .5 * (cur[0] - 24), .707), coefs(1, fm, .5 * (cur[1] - 24), qOf(cur[5] / 127)), coefs(2, fh, .5 * (cur[2] - 24), .707)];
      const yOf = (hz) => zero - clamp(bqDb(bands[0], hz) + bqDb(bands[1], hz) + bqDb(bands[2], hz), -13, 13) * pxDb;
      const pts = [];
      for (let x = 0; x <= W + 1; x += 2) pts.push([x, yOf(hzOf(x))]);
      if (!dith) dith = ditherer(s);
      if (!off) dith((g) => {                            /* what it raises, warm; what it cuts, ice */
        g.beginPath(); g.moveTo(0, zero); pts.forEach(([x, y]) => g.lineTo(x, y)); g.lineTo(W, zero); g.closePath();
        g.save(); g.beginPath(); g.rect(0, 0, W, zero); g.clip();
        g.beginPath(); g.moveTo(0, zero); pts.forEach(([x, y]) => g.lineTo(x, y)); g.lineTo(W, zero); g.closePath();
        g.fillStyle = rgba(LAMP, .62); g.fill(); g.restore();
        g.save(); g.beginPath(); g.rect(0, zero + 1, W, bed); g.clip();
        g.beginPath(); g.moveTo(0, zero); pts.forEach(([x, y]) => g.lineTo(x, y)); g.lineTo(W, zero); g.closePath();
        g.fillStyle = rgba(ICE, .5); g.fill(); g.restore();
      });
      rsGrid(ctx, W, top, bed, zero, xOf);
      ctx.beginPath(); pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
      if (off) { ctx.strokeStyle = rgba(SLATE, .95); ctx.lineWidth = 1.4; ctx.stroke(); } else curveGlow(ctx, FOG, .95);
      const moving = morph < 1;
      [[fl, "2", 0], [fm, "3", 1], [fh, "4", 2]].forEach(([hz, ch, i]) => badgeAt(ctx, xOf(hz), clamp(yOf(hz), top + 2, bed - 2), ch, moving && Math.abs(to[i] - from[i]) > .5, off));
      gLow(fmtDb(.5 * (cur[0] - 24)), "DB", cur[0] / 48);
      gMid(fmtDb(.5 * (cur[1] - 24)), "DB", cur[1] / 48);
      gHigh(fmtDb(.5 * (cur[2] - 24)), "DB", cur[2] / 48);
    });
    s.onsize = () => { dith = null; };
    showChips = chips(eqCard, preset, (v) => { auto = false; go(v); s.wake(); });
  }

  // ============================================================ the bass: its swell and its models' shapes
  const swell = document.querySelector(".swell");
  if (swell) {
    const y = (x) => 112 + 30 * Math.sin(2 * Math.PI * x / 800) + 12 * Math.sin(2 * Math.PI * x / 400 + 1.1) + 5 * Math.sin(2 * Math.PI * x / 200 + 2.3);
    let d = "";
    for (let x = 0; x <= 1600; x += 8) d += (x ? "L" : "M") + x + " " + y(x).toFixed(1);
    swell.querySelector(".swell-path").setAttribute("d", d);
    swell.querySelector(".swell-halo").setAttribute("d", d);
    swell.querySelector(".swell-fill").setAttribute("d", d + " L1600 220 L0 220 Z");
  }
  const WAVES = {
    sine: (p) => Math.sin(p),
    triangle: (p) => 2 / Math.PI * Math.asin(Math.sin(p)),
    square: (p) => Math.tanh(6 * Math.sin(p)),
    saw: (p) => 1 - 2 * (((p / (2 * Math.PI)) % 1 + 1) % 1),
    reese: (p) => .5 * (1 - 2 * ((p / (2 * Math.PI)) % 1)) + .5 * (1 - 2 * ((p * 1.12 / (2 * Math.PI) + .3) % 1)),
    sub: (p) => .6 * Math.sin(p) + .5 * Math.sin(p / 2),
    organ: (p) => (Math.sin(p) + .55 * Math.sin(2 * p) + .4 * Math.sin(3 * p)) / 1.5,
    fm: (p) => Math.sin(p + 1.3 * Math.sin(p)),
  };
  document.querySelectorAll(".wave[data-wave]").forEach((svg) => {
    const fn = WAVES[svg.dataset.wave];
    if (!fn) return;
    const cycles = svg.dataset.wave === "sub" || svg.dataset.wave === "reese" ? 4 : 2;
    let d = "";
    for (let i = 0; i <= 128; i++) {
      const x = i / 128 * 64, v = fn(i / 128 * cycles * 2 * Math.PI);
      d += (i ? "L" : "M") + x.toFixed(2) + " " + (12 - v * 9.5).toFixed(2);
    }
    svg.querySelector("path").setAttribute("d", d);
  });

  // ============================================================ the mixer, breathing with PUMP
  const mixer = document.querySelector("[data-mixer]");
  if (mixer) {
    const chans = Array.from(mixer.querySelectorAll(".ch"));
    const meters = chans.map((c) => c.querySelector(".meter"));
    const DEPTH = [.62, .5, .32, .55], BASE = [.62, .66, .44, .74];
    const noise = chans.map(() => ({ v: .5, to: .5, at: 0 }));
    let rate = 1, t = 0, last = 0, raf = 0, seen = false;
    const BEAT = 60 / 76;                                // TEMPO 76: a slow pulse
    const env = (ph) => {                                // the duck: down fast, back over the period
      const att = .04;
      return ph < att ? ph / att : Math.exp(-(ph - att) * 5.5);
    };
    const paint = () => {
      chans.forEach((c, i) => {
        const n = noise[i];
        if (t > n.at) { n.to = .82 + Math.random() * .18; n.at = t + .25 + Math.random() * .6; }
        n.v += (n.to - n.v) * .08;
        let duck = 1;
        if (rate) { const ph = ((t / (BEAT * rate)) % 1); duck = 1 - DEPTH[i] * env(ph); }
        meters[i].style.setProperty("--m", (BASE[i] * n.v * duck).toFixed(3));
      });
    };
    const frame = (now) => {
      raf = 0;
      if (!seen || document.hidden) return;
      const dt = last ? Math.min(.08, (now - last) / 1000) : 1 / 60;
      if (now - last < 31) { raf = requestAnimationFrame(frame); return; }
      last = now; t += dt;
      paint();
      raf = requestAnimationFrame(frame);
    };
    const wake = () => { if (reduce) { t = .9; paint(); return; } if (!raf && seen) { last = 0; raf = requestAnimationFrame(frame); } };
    new IntersectionObserver((es) => { seen = es[es.length - 1].isIntersecting; wake(); }).observe(mixer);
    document.addEventListener("visibilitychange", wake);
    mixer.querySelectorAll(".chip[data-rate]").forEach((b) => b.addEventListener("click", () => {
      rate = parseFloat(b.dataset.rate);
      mixer.querySelectorAll(".chip[data-rate]").forEach((o) => o.setAttribute("aria-pressed", String(o === b)));
      wake();
    }));
    paint();
  }
})();
