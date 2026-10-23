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
