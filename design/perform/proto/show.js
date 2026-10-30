/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Perform's show, shared by the mockups (index.html) and the live page (live/): the stage (1920 x 1080, fitted to the
 * window), what the FM-1 holds (its scene's values), the HUD (the FM-1's header, quiet) and the dock under the
 * waterline (the river above it stays clean, nothing drawn over it, never dimmed):
 *   LIVE   what the hand does, small, as it comes, then gone: a knob turned (its page, the setting, its value), a page
 *          opened (its four settings), a scene loaded, keys played (the drones' notes; GLINT's walk as a constellation
 *          flowing left, as the River's ARP strip), a button
 *   LFO 1-3  each LFO's shape over a turn, as deep as DEPTH from POLARITY's side, a glint where it is now (the River's
 *          lfo_scope), what it moves, named in full, in its track's colour
 *   CLOUD, TAPE, FILTER  each effect drawn, not its numbers: the room's rings (the iPad app's), the FM-1's cassette
 *          (ui.c tape_scope), the filter's curve over the lowest drone's harmonics; dimmed while off, lit a moment
 *          while the hand moves one of its settings
 * The markup is the pages' own (#stage, #dock, #hud-*); the dock is built here.
 *   PerformShow.factory                    the factory scenes [{name, slot, bytes}]
 *   PerformShow.S                          {scene, slot, v (every setting's value), lit, notes, changed, oct, lfo, auto}
 *   PerformShow.values(scene)              a factory scene's values held (nothing shown)
 *   PerformShow.knob(page, cell, v) · hand(p, v) · turn(page, cell, by, ms) · openPage(page) · scene({name, slot}) ·
 *               keys(track, notes, words) · button(key, word, value, cls) · touch(p) (a box's row lit)
 *   PerformShow.strikes(frame)             GLINT's notes as the engine strikes them (web_live's 16..19, 48..51)
 *   PerformShow.tick(now, frame)           each frame: the tweens, the dock drawn, a gesture let go once the hand left
 *   PerformShow.look()                     what the river needs (PerformRiver's look) · .fit() · .clear() · .hud()
 *   PerformShow.transport(bits)            each track playing (1 the drones, 2 GLINT, 4 the bass)
 *   PerformShow.hold (true: nothing goes) · .onParam(p, v) (each value a knob reaches) · .showing */
"use strict";
window.PerformShow = (() => {
  const SC = window.PERFORM_SCHEMA, P = SC.params, R = PerformRiver, LF = SC.lfo;
  const C = { ice: "#6ed0ff", fog: "#dce3ea", mist: "#8b95a4", slate: "#4a5563", steel: "#26303c", lamp: "#f2a65a", white: "#ffffff" };
  const TRACK = { drone: C.fog, glint: C.ice, bass: C.lamp, off: C.mist };
  const $ = (s) => document.querySelector(s);
  const h = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
  const page = (name) => SC.pages.find((g) => g.name === name);
  const factory = SC.factory.map((f, i) => ({ name: f.name, slot: i + 1, bytes: Uint8Array.from(f.hex.match(/../g), (x) => parseInt(x, 16)) }));

  // ------------------------------------------------------------------ the stage, fitted (the bar above it, if any)
  const stage = $("#stage"), wrap = $("#wrap");
  function fit() {
    const bar = $("#bar"), top = bar && !document.body.classList.contains("bare") ? bar.offsetHeight : 0;
    const s = Math.min(innerWidth / 1920, (innerHeight - top) / 1080);
    stage.style.transform = `scale(${s})`;
    wrap.style.width = `${1920 * s}px`;
    wrap.style.height = `${1080 * s}px`;
  }
  addEventListener("resize", fit);

  // ------------------------------------------------------------------ what the FM-1 holds
  const S = { scene: "", slot: 0, v: new Array(P.length).fill(0), lit: new Set(), notes: false, changed: false, oct: -1,
              lfo: [0, 0, 0], auto: false, artist: "", play: 0, hand: true };   /* hand: a key held (DEMO: always) */
  /* the artist's name, as the connect screen keeps it in this browser (none: no BY line) */
  try { S.artist = (localStorage.getItem("perform.artist") || "").trim(); } catch (_) { /* no storage here */ }
  function values(f) {
    S.scene = f.name;
    S.slot = f.slot;
    S.v = Array.from(f.bytes.subarray(SC.v0, SC.v0 + SC.nsound));
    while (S.v.length < P.length) S.v.push(0);
    S.changed = false;
    S.lit = new Set();
    S.notes = false;
    hud();
  }
  /* what the LFOs add now to each setting they move, as app.c's lfo_run (mod[]): DEPTH times SHAPE's value (BIPOLAR,
   * UP: from 0, DOWN: below it) times the setting's range; a setting as it sounds, the LFOs on it */
  let mod = {};
  function modulate(f) {
    mod = {};
    for (let i = 0; f && i < 3; i++) {
      const b = LF.base[i], depth = S.v[b + 2] / 127, pol = S.v[b + 3], w = f[32 + i];
      if (!(f[26 + i] > 0) || depth <= 0) continue;
      const off = depth * (pol === 1 ? 0.5 * (w + 1) : pol === 2 ? -0.5 * (w + 1) : w);
      for (let k = 4; k < 8; k++) {
        const tg = S.v[b + k] && LF.targets[S.v[b + k] % LF.targets.length];
        if (tg && tg.p >= 0) mod[tg.p] = (mod[tg.p] || 0) + off * P[tg.p].max;
      }
    }
  }
  const eff = (p) => Math.max(0, Math.min(P[p].max, S.v[p] + (mod[p] || 0)));
  const text = (p, v = S.v[p]) => (P[p].texts ? P[p].texts[Math.max(0, Math.min(P[p].max, Math.round(v)))] : String(Math.round(v)));
  /* the MIXER's four levels behind the river while the hand is on them (or on its page): in at once, two seconds after
   * the last touch, then out slowly */
  const MIX = [73, 74, 75, 76];
  let mixAt = -1e9;
  const look = () => {
    const sp = eff(32), bl = eff(125) / 127, since = performance.now() - mixAt;
    return {
      choir: eff(0) / 127, partials: eff(1) / 127, breath: eff(2) / 127, motion: eff(16) / 127,
      space: Math.min(1, (sp + (0.9 * 127 - sp) * Math.max(0, bl) * (sp < 0.9 * 127 ? 1 : 0)) / 127),
      freeze: S.v[156] === 1, sink: eff(126) / 127, dim: 1, notes: S.notes, lit: S.lit,
      mix: { a: since < 2000 ? 1 : Math.max(0, 1 - (since - 2000) / 1600), lv: MIX.map((p) => eff(p) / P[p].max) },
    };
  };

  // ------------------------------------------------------------------ the HUD: the FM-1's header, quiet
  const KB = `<svg viewBox="0 0 22 14" class="kb"><rect x="1" y="1" width="20" height="12" rx="1.5"/><path d="M5 1v12M9 1v12M13 1v12M17 1v12"/><path d="M4 1h2v7H4zM8 1h2v7H8zM15 1h2v7h-2z" class="fill"/></svg>`;
  function hud() {
    const v = S.v;
    $("#hud-scene").textContent = S.scene;
    $("#hud-artist").textContent = S.artist ? `BY ${S.artist}` : "";
    $("#hud-scene").classList.toggle("changed", S.changed);
    $("#hud-status").innerHTML = S.auto ? `<span class="auto">AUTO</span>` : "";
    transport(S.play);
    $("#hud-keys").innerHTML = `${KB}<b>${S.notes ? "GLINT" : "DRONE"}</b><span>${S.oct > 0 ? "+" : ""}${S.oct}</span>`;
    $("#hud-keys").className = "keys" + (S.notes ? " ice" : "");
    bare();
  }
  /* H's header: how far the logo goes to the middle and the transport to the right edge (its words gone) */
  function bare() {
    const hud = $(".hud"), brand = $(".brand"), tp = $("#transport");
    if (!tp) return;
    const r = tp.parentElement;
    stage.style.setProperty("--to-mid", `${hud.offsetWidth / 2 - brand.offsetLeft - brand.offsetWidth / 2}px`);
    stage.style.setProperty("--to-right", `${r.offsetLeft + r.offsetWidth - tp.offsetLeft - tp.offsetWidth}px`);
  }
  /* the transport: each track playing (bits: 1 the drones, 2 GLINT, 4 the bass), as the FM-1 says it; under each,
   * where its notes come from: GLINT's SEQ (ARP, SEQ), the bass's SEQUENCE (SEQ), else the keys' KEY MODE */
  let tpWas = "";
  function transport(bits) {
    const tp = $("#transport"), v = S.v, key = text(153, v[153]);
    if (!tp) return;
    const words = [key, v[123] === 1 ? "ARP" : v[123] === 2 ? "SEQ" : key, v[176] ? "SEQ" : key];
    if (`${bits}${words}` === tpWas) return;
    tpWas = `${bits}${words}`;
    S.play = bits;
    const t = tp.querySelectorAll("span");
    t[0].classList.toggle("stop", !bits);
    for (let k = 0; k < 3; k++) {
      t[1 + k].classList.toggle("stop", !(bits >> k & 1));
      t[1 + k].querySelector("small").textContent = words[k];
    }
  }

  // ------------------------------------------------------------------ the FM-1, small (the guide's panel, in its units)
  const WHITE_PC = [5, 7, 9, 11, 0, 2, 4, 5, 7, 9, 11, 0, 2, 4, 5, 7];
  const BLACK_X = [93, 160.3, 227.5, 362, 429.3, 563.8, 631, 698.3, 832.8, 900, 1034.5], BLACK_PC = [6, 8, 10, 1, 3, 6, 8, 10, 1, 3, 6];
  const BTN = [["FX", 680, 263], ["SEL", 761, 263], ["ENV", 842, 263], ["LFO", 923, 263], ["EDIT", 1004, 263], ["GLO", 1085, 263],
               ["HOME", 680, 375], ["SAVE", 761, 375], ["ARP", 842, 375], ["SEQ", 923, 375], ["PLAY", 1004, 375], ["REC", 1085, 375]];
  function mini(lit) {
    const on = (id) => (lit[id] ? ` class="${lit[id]}"` : "");
    let s = `<svg viewBox="0 0 1180 790" class="mini"><rect x="8" y="8" width="1164" height="774" rx="40" class="body"/>`;
    [["MASTER", 118, 112], ["SELECT", 250, 112], ["PRESETS", 118, 246], ["ALGORITHM", 250, 246]].forEach(([id, x, y]) =>
      (s += `<circle cx="${x}" cy="${y}" r="30"${on(id)} data-k/>`));
    [1, 2, 3, 4].forEach((n) => (s += `<circle cx="${572 + n * 120}" cy="112" r="28"${on("K" + n)} data-k/>`));
    s += `<rect x="62" y="346" width="110" height="38" rx="9"${on("OCT-")} data-b/><rect x="190" y="346" width="110" height="38" rx="9"${on("OCT+")} data-b/>`;
    s += `<rect x="350" y="46" width="262" height="262" rx="22" class="screen"/>`;
    BTN.forEach(([id, x, y]) => (s += `<rect x="${x - 32}" y="${y - 25}" width="64" height="40" rx="9"${on(id)} data-b/>`));
    WHITE_PC.forEach((pc, i) => (s += `<rect x="${59.4 + i * 67.25}" y="607" width="52.5" height="147" rx="24"${on("w" + i)} data-w/>`));
    BLACK_X.forEach((x, i) => (s += `<rect x="${x}" y="504" width="52.5" height="93" rx="24"${on("b" + i)} data-w/>`));
    return s + "</svg>";
  }
  const keysFor = (notes) => {
    const lit = {}, used = new Set();
    notes.forEach((n) => {
      const pc = n % 12;
      let i = WHITE_PC.findIndex((p, j) => p === pc && !used.has("w" + j));
      if (i >= 0) { lit["w" + i] = "on"; used.add("w" + i); return; }
      i = BLACK_PC.findIndex((p, j) => p === pc && !used.has("b" + j));
      if (i >= 0) { lit["b" + i] = "on"; used.add("b" + i); }
    });
    return lit;
  };

  // ------------------------------------------------------------------ a value as the FM-1 writes it, its gauge
  function valueHTML(p, v = S.v[p]) {
    const t = text(p, v), m = /^([-+]?[\d.:]+)\s?([A-Z%]+)$/.exec(t);
    return m && !/^\d+\/\d+$/.test(t) ? `${m[1]}<small>${m[2]}</small>` : t;
  }
  function gaugeHTML(p, v = S.v[p]) {
    const pr = P[p];
    if (pr.kind === "steps" || pr.kind === "target") {
      let s = `<div class="steps">`;
      for (let i = 0; i <= pr.max; i++) s += `<i class="${i === Math.round(v) ? "sel" : ""}"></i>`;
      return s + "</div>";
    }
    return `<div class="gauge"><i style="width:${(v / pr.max) * 100}%"></i><b style="left:${(v / pr.max) * 100}%"></b></div>`;
  }

  // ------------------------------------------------------------------ the dock: LIVE, the LFOs, the effects
  const dock = $("#dock");
  const FX = [["CLOUD", 156, [20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 156]], ["TAPE", 142, [142, 143, 144, 145, 146, 147]],
              ["FILTER", 138, [138, 139, 140, 141]]];
  /* an LFO TARGET named in full: its section's word before it where the FM-1 shortens it (CH RESO: CHOIR RESONANCE,
   * GL BRIGHT: GLINT BRIGHT, CUTOFF: FILTER CUTOFF, DRONE LVL: MIXER DRONE); the sound's own and the hands' as they are */
  const PLAIN = ["SOUND", "DRONE TONE", "DRONE MOTION", "GENERATIVE FX"];
  function targetName(t) {
    if (!t || t.p < 0) return t ? t.name : "?";
    const pages = SC.pages.filter((g) => g.p.includes(t.p)).map((g) => g.name);
    if (!pages.length || pages.some((n) => PLAIN.includes(n))) return P[t.p].name;
    const w = pages[0] === "MASTER FILTER" ? "FILTER" : pages[0].split(" ")[0];
    return P[t.p].name.startsWith(w) ? P[t.p].name : `${w} ${P[t.p].name}`;
  }
  dock.innerHTML = `<section class="box now" id="now"></section>` +
    [0, 1, 2].map((i) => `<section class="box lfo" data-i="${i}"><header><b>LFO ${i + 1}</b><span></span></header>
      <canvas width="352" height="120"></canvas><ul class="tg"></ul></section>`).join("") +
    FX.map(([n], i) => `<section class="box fx" data-i="${i}"><header><b>${n}</b><span></span></header><canvas width="300" height="270"></canvas></section>`).join("");
  const now = $("#now");
  const lfoBox = [...dock.querySelectorAll(".lfo")], fxBox = [...dock.querySelectorAll(".fx")];
  const touchedAt = {};                  /* a setting the hand moved: when (its row lit a moment) */

  function lfoRnd(i, c) {               /* app.c's lfo_rnd: RANDOM's value for turn c of LFO i, -1..1 */
    let x = (Math.imul((c + 1) >>> 0, 2654435761) ^ Math.imul(i + 1, 0x9E3779B9)) >>> 0;
    x ^= x >>> 15;
    x = Math.imul(x, 0x85EBCA77) >>> 0;
    x ^= x >>> 13;
    return (x >>> 8) * (2 / 16777216) - 1;
  }
  function lfoShape(sh, x, c, i) {      /* app.c's lfo_shape: SHAPE at x (0..1) of turn c of LFO i, -1..1 */
    if (sh === 0) return Math.sin(2 * Math.PI * x);
    if (sh === 1) return x < 0.25 ? 4 * x : x < 0.75 ? 2 - 4 * x : 4 * x - 4;
    if (sh === 2) return 2 * x - 1;
    if (sh === 3) return 1 - 2 * x;
    if (sh === 4) return x < 0.5 ? 1 : -1;
    if (sh === 5) return lfoRnd(i, c);
    const a = lfoRnd(i, c), b = lfoRnd(i, c + 1);
    return a + (b - a) * x * x * (3 - 2 * x);
  }
  const lfoOn = (b) => S.v[b + 2] > 0 && [4, 5, 6, 7].some((t) => S.v[b + t] > 0);
  const star = (ctx, x, y, a) => {
    const q = a * 0.2, v = a * 0.82;
    ctx.beginPath();
    ctx.moveTo(x, y - v); ctx.lineTo(x + q, y - q); ctx.lineTo(x + a, y); ctx.lineTo(x + q, y + q);
    ctx.lineTo(x, y + v); ctx.lineTo(x - q, y + q); ctx.lineTo(x - a, y); ctx.lineTo(x - q, y - q);
    ctx.closePath();
    ctx.fill();
  };
  const glint = (ctx, x, y, r, a) => {
    const g = ctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, `rgba(110,208,255,${0.5 * a})`);
    g.addColorStop(1, "rgba(110,208,255,0)");
    ctx.fillStyle = g;
    ctx.fillRect(x - r, y - r, 2 * r, 2 * r);
  };

  /* an LFO's box: its shape over a turn (four for RANDOM and DRIFT: the three before and this one), a glint where it is */
  function drawLfo(i, f) {
    const box = lfoBox[i], cv = box.querySelector("canvas"), ctx = cv.getContext("2d"), W = cv.width, H = cv.height;
    const b = LF.base[i], sh = S.v[b] % 7, depth = S.v[b + 2], pol = S.v[b + 3] % 3, on = lfoOn(b), rnd = sh >= 5, turns = rnd ? 4 : 1;
    const top = H * 0.14, bot = H * 0.86, d = 0.2 + 0.8 * depth / 127, base = pol === 1 ? bot : pol === 2 ? top : (top + bot) / 2;
    const Y = (v) => (pol === 1 ? bot - 0.5 * (v + 1) * (bot - top) * d : pol === 2 ? top + 0.5 * (v + 1) * (bot - top) * d : base - v * 0.5 * (bot - top) * d);
    const live = on && f && f[26 + i] > 0, cyc = live ? Math.round(f[35 + i]) : 3, col = on ? C.ice : C.slate;
    const sig = `${on},${S.v.slice(b, b + 8).join(",")}`;
    if (box._sig !== sig) {              /* its words: SHAPE; what it moves, in full, in its tracks' colours */
      box._sig = sig;
      box.classList.toggle("off", !on);
      box.querySelector("header span").textContent = on ? LF.shapes[sh] : "OFF";
      const tg = [4, 5, 6, 7].map((t) => S.v[b + t]).filter((t) => t > 0).map((t) => LF.targets[t] || { name: "?", track: "off", p: -1 });
      box.querySelector(".tg").innerHTML = tg.length
        ? tg.map((t) => `<li style="color:${on ? TRACK[t.track] || C.fog : C.slate}">${targetName(t)}</li>`).join("")
        : `<li>NO TARGET</li>`;
    }
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = C.steel;
    ctx.fillRect(0, Math.round(base) - 1, W, 2);
    ctx.beginPath();
    for (let x = 0; x <= W; x += 2) {
      const u = Math.min(x / W * turns, turns - 1e-4), c = rnd ? cyc + Math.floor(u) - 3 : 0;
      const y = Y(lfoShape(sh, u - Math.floor(u), c, i));
      x ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
    }
    ctx.lineJoin = "round";
    ctx.strokeStyle = col;
    ctx.globalAlpha = on ? 0.14 : 0;
    ctx.lineWidth = 9;
    ctx.stroke();
    ctx.globalAlpha = on ? 0.9 : 0.8;
    ctx.lineWidth = 2.6;
    ctx.stroke();
    ctx.globalAlpha = 1;
    if (!live) return;
    const x = (rnd ? (3 + f[29 + i]) / 4 : f[29 + i]) * W, y = Y(f[32 + i]);
    ctx.fillStyle = "rgba(110,208,255,0.22)";
    ctx.fillRect(x - 1, H * 0.05, 2, H * 0.9);
    glint(ctx, x, y, 22, 1);
    ctx.fillStyle = C.ice;
    star(ctx, x, y, 13);
    ctx.fillStyle = C.white;
    ctx.beginPath();
    ctx.arc(x, y, 3, 0, Math.PI * 2);
    ctx.fill();
  }
  /* an effect's box: drawn (cloud, cassette, filter), on or off, lit while the hand moves one of its settings */
  function drawFx(k, t, f) {
    const [, onP, ps] = FX[k], box = fxBox[k], cv = box.querySelector("canvas"), ctx = cv.getContext("2d");
    const on = k === 0 || S.v[onP] > 0, hot = ps.some((p) => t - (touchedAt[p] || -1e9) < 1600);
    const sig = `${S.v[onP]},${on},${hot}`;
    if (box._sig !== sig) {
      box._sig = sig;
      box.classList.toggle("off", !on);
      box.classList.toggle("hot", hot);
      const tag = box.querySelector("header span");
      tag.textContent = k === 0 ? (S.v[156] ? "FREEZE" : "") : k === 2 ? ["OFF", "LOW", "HIGH"][S.v[onP]] || "" : text(onP);
      tag.className = (k === 0 ? S.v[156] : on) ? "ice" : "";
    }
    const cw = Math.round(cv.clientWidth * 2), chh = Math.round(cv.clientHeight * 2);
    if (cw && chh && (cv.width !== cw || cv.height !== chh)) { cv.width = cw; cv.height = chh; }
    ctx.clearRect(0, 0, cv.width, cv.height);
    (k === 0 ? drawRoom : k === 1 ? drawTape : drawFilter)(ctx, cv.width, cv.height, t / 1000, f, on);
  }
  const rgba = (hex, a) => `rgba(${parseInt(hex.slice(1, 3), 16)},${parseInt(hex.slice(3, 5), 16)},${parseInt(hex.slice(5, 7), 16)},${a})`;
  /* CLOUD: the room (the iPad app's), rings leaving the source, as wide as SIZE, as long as DECAY, rounder with
   * DIFFUSION, swaying with MODULATION, wider with WIDTH; set (FREEZE): still, a lattice over it */
  function drawRoom(ctx, W, H, now, f) {
    const size = eff(20) / 127, decay = eff(21) / 127, diff = eff(24) / 127, mod = eff(25) / 127, pre = eff(26) / 127;
    const width = eff(29) / 127, frozen = S.v[156] !== 0, cx = W / 2, cy = H * 0.57, lvl = Math.min(1, Math.max(0, (f && f[45]) || 0));
    const R = 0.16 * W + size * Math.min(W, H * 2) * 0.5, per = 1 + decay * 7, n = 14;
    for (let k = 0; k < n; k++) {
      const age = frozen ? k / n : ((now / per + k / n) % 1), r = pre * 26 + 10 + age * R;
      const a = Math.pow(1 - age, 1.4) * (0.14 + 0.55 * lvl + 0.22 * decay);
      ctx.beginPath();
      for (let s = 0; s <= 64; s++) {
        const th = s / 64 * 2 * Math.PI, wob = 1 + mod * 0.05 * Math.sin(th * 5 + now * 1.5 + k);
        const x = cx + Math.cos(th) * r * (1 + width * 0.9) * wob, y = cy + Math.sin(th) * r * 0.55 * wob;
        s ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      }
      ctx.strokeStyle = rgba(C.fog, a);
      ctx.lineWidth = 2 + diff * 6 * age;
      ctx.stroke();
    }
    if (frozen) {
      ctx.fillStyle = rgba(C.ice, 0.16);
      for (let x = cx % 36; x < W; x += 36) ctx.fillRect(x, 0, 1.5, H);
      for (let y = cy % 36; y < H; y += 36) ctx.fillRect(0, y, W, 1.5);
    }
    const r = 30 + 34 * lvl, g = ctx.createRadialGradient(cx, cy, 0, cx, cy, r);
    g.addColorStop(0, rgba(frozen ? C.ice : C.fog, 0.8));
    g.addColorStop(1, rgba(C.fog, 0));
    ctx.fillStyle = g;
    ctx.fillRect(cx - r, cy - r, 2 * r, 2 * r);
  }
  /* TAPE: the FM-1's cassette (ui.c tape_scope): two reels, the tape off the left one round a roller, over the head,
   * round the other onto the right one, which fills as the left empties (then a quick rewind); the free spans looser
   * and swaying as WOBBLE grows; SATURATION warms the tape and lights the head. Still and slate while TAPE is off */
  const tp = { rot: [0, 0], u: 0.35, rew: false, vs: 1, last: 0 };
  function drawTape(ctx, W, H, t, f, on) {
    const k = W / 182, ox = W / 2 - 120 * k, oy = H * 0.58 - 63 * k, X = (x) => ox + x * k, Y = (y) => oy + y * k;
    const sat = eff(143) / 127, wob = eff(144) / 127, gr = 4, hy = 83 + gr, a0 = 100, a1 = 529;
    const dt = Math.min(0.1, tp.last ? t - tp.last : 0);
    tp.last = t;
    const spd = on ? 1 + wob * (0.08 * Math.sin(2 * Math.PI * 0.55 * t) + 0.03 * Math.sin(2 * Math.PI * 1.7 * t)) : 1;
    tp.vs += (spd - tp.vs) * Math.min(1, dt * 6.7);
    const pl = Math.sqrt(a0 + (1 - tp.u) * (a1 - a0)), pr = Math.sqrt(a0 + tp.u * (a1 - a0));
    if (on) {
      let v = tp.rew ? -400 : 60 * (1 + 5 * (tp.vs - 1));
      v = (!tp.rew && v < 0 ? 0 : v) * dt;
      tp.u += tp.rew ? -dt / 1.5 : v / 5400;
      if (tp.u >= 1) tp.rew = true;
      if (tp.u <= 0) { tp.u = 0; tp.rew = false; }
      tp.u = Math.min(1, tp.u);
      tp.rot[0] -= v / (2 * Math.PI * pl);
      tp.rot[1] -= v / (2 * Math.PI * pr);
    }
    const mixc = (a, b, u) => `rgb(${a.map((x, i) => Math.round(x + (b[i] - x) * u)).join(",")})`;
    const tape = on ? mixc([120, 72, 40], [242, 166, 90], sat) : C.steel, hub = on ? C.fog : C.slate, rim = on ? C.slate : C.steel;
    if (on && sat > 0) {                 /* the head's glow: wider and whiter as SATURATION grows, flickering a little */
      const rad = (10 + 22 * sat) * k, gl = (0.35 + 0.65 * sat) * (0.85 + 0.15 * Math.sin(t * 2.3 * 2 * Math.PI) * Math.sin(t * 0.37 * 2 * Math.PI));
      ctx.save();
      ctx.translate(X(120), Y(hy));
      ctx.scale(1.6, 1);
      const g = ctx.createRadialGradient(0, 0, 0, 0, 0, rad);
      g.addColorStop(0, sat > 0.5 ? `rgba(240,236,230,${0.9 * gl})` : `rgba(242,166,90,${0.9 * gl})`);
      g.addColorStop(0.3, `rgba(242,166,90,${0.5 * gl})`);
      g.addColorStop(1, "rgba(242,166,90,0)");
      ctx.fillStyle = g;
      ctx.fillRect(-rad, -rad, 2 * rad, 2 * rad);
      ctx.restore();
    }
    const reel = (cx, pack, rot) => {
      const x = X(cx), y = Y(60);
      ctx.lineWidth = 1.6 * k;
      ctx.strokeStyle = rim;
      ctx.beginPath(); ctx.arc(x, y, 25.5 * k, 0, 2 * Math.PI); ctx.stroke();
      ctx.fillStyle = tape;
      ctx.beginPath(); ctx.arc(x, y, pack * k, 0, 2 * Math.PI); ctx.arc(x, y, 9.5 * k, 0, 2 * Math.PI, true); ctx.fill();
      ctx.strokeStyle = "rgba(10,13,18,0.35)";
      ctx.lineWidth = 0.6 * k;
      for (let r = 11; r < pack; r += 2) { ctx.beginPath(); ctx.arc(x, y, r * k, 0, 2 * Math.PI); ctx.stroke(); }
      ctx.strokeStyle = hub;
      ctx.lineWidth = 1.4 * k;
      ctx.beginPath(); ctx.arc(x, y, 7 * k, 0, 2 * Math.PI); ctx.stroke();
      const ms = Math.sin(2 * Math.PI * (rot + 0.125)), mc = Math.sin(2 * Math.PI * (rot + 0.375));
      ctx.lineWidth = 1.2 * k;
      ctx.beginPath(); ctx.moveTo(x + (pack - 5) * k * mc, y + (pack - 5) * k * ms); ctx.lineTo(x + (pack - 0.5) * k * mc, y + (pack - 0.5) * k * ms); ctx.stroke();
      ctx.lineWidth = 2.2 * k;
      ctx.lineCap = "round";
      for (let j = 0; j < 3; j++) {
        const a = rot + j / 3, sn = Math.sin(2 * Math.PI * a), cs = Math.cos(2 * Math.PI * a);
        ctx.beginPath(); ctx.moveTo(x + 1.5 * k * cs, y + 1.5 * k * sn); ctx.lineTo(x + 6.5 * k * cs, y + 6.5 * k * sn); ctx.stroke();
      }
    };
    reel(60, pl, tp.rot[0]);
    reel(180, pr, tp.rot[1]);
    const tangent = (ax, ay, ar, bx, by, br) => {   /* where the tape leaves circle a for circle b, under both */
      const dx = bx - ax, dy = by - ay, l = Math.hypot(dx, dy) + 1e-6, ux = dx / l, uy = dy / l, c = (ar - br) / l, sn = Math.sqrt(Math.max(0, 1 - c * c));
      let nx = c * ux - sn * uy, ny = c * uy + sn * ux;
      if (ny < 0) { nx = c * ux + sn * uy; ny = c * uy - sn * ux; }
      return [ax + ar * nx, ay + ar * ny, bx + br * nx, by + br * ny];
    };
    const span = (p, sag) => {           /* a free span hanging sag at its middle */
      const dx = p[2] - p[0], dy = p[3] - p[1], l = Math.hypot(dx, dy) + 1e-6;
      let nx = -dy / l, ny = dx / l;
      if (ny < 0) { nx = -nx; ny = -ny; }
      ctx.beginPath();
      ctx.moveTo(X(p[0]), Y(p[1]));
      ctx.quadraticCurveTo(X(0.5 * (p[0] + p[2]) + 2 * sag * nx), Y(0.5 * (p[1] + p[3]) + 2 * sag * ny), X(p[2]), Y(p[3]));
      ctx.strokeStyle = tape;
      ctx.lineWidth = 1.3 * k;
      ctx.stroke();
    };
    let loose = on ? wob * (4 + 3 * Math.sin(2 * Math.PI * t * 0.7)) + 60 * Math.max(0, 1 - spd) : 0;
    loose = Math.min(10, loose);
    span(tangent(60, 60, pl, 100, 83, gr), loose);
    span(tangent(140, 83, gr, 180, 60, pr), on ? wob * (4 + 3 * Math.sin(2 * Math.PI * (t * 0.7 + 0.4))) : 0);
    ctx.strokeStyle = on && sat > 0.3 ? mixc([242, 166, 90], [220, 227, 234], sat) : tape;
    ctx.lineWidth = (1.2 + 0.8 * sat) * k;
    ctx.beginPath(); ctx.moveTo(X(100), Y(hy)); ctx.lineTo(X(140), Y(hy)); ctx.stroke();
    ctx.strokeStyle = hub;
    ctx.lineWidth = 1.1 * k;
    [100, 140].forEach((x) => { ctx.beginPath(); ctx.arc(X(x), Y(83), (gr - 0.5) * k, 0, 2 * Math.PI); ctx.stroke(); });
    ctx.fillStyle = on ? (sat > 0.5 ? mixc([242, 166, 90], [220, 227, 234], (sat - 0.5) * 1.2) : C.lamp) : C.slate;
    ctx.fillRect(X(113), Y(hy + 1), 15 * k, 4 * k);
    ctx.fillStyle = "#0a0d12";
    ctx.fillRect(X(120), Y(hy + 1), 1 * k, 4 * k);
  }
  /* FILTER: its curve (LOW PASS, HIGH PASS at CUTOFF, its RESONANCE's peak) over the lowest drone's harmonics, each
   * as heard before it (steel) and after it (fog, as loud as the drone) */
  const hzOf = (t) => { const m = /^([\d.]+)\s*(K?)HZ$/.exec(t || ""); return m ? +m[1] * (m[2] ? 1000 : 1) : 1000; };
  function filterMag(fr, fc, q, type) {
    const r = fr / fc, den = Math.sqrt((1 - r * r) ** 2 + (r / q) ** 2);
    return type === 0 ? 1 / den : (r * r) / den;
  }
  function drawFilter(ctx, W, H, t, f, on) {
    const type = S.v[138] === 2 ? 2 : 0, fc = hzOf(text(139, eff(139))), q = 0.6 + eff(140) / 127 * 9;
    const lx = (fr) => Math.log2(fr / 30) / Math.log2(16000 / 30) * W, db = (m) => 20 * Math.log10(Math.max(1e-4, m));
    const yd = (d) => H - (Math.max(-42, Math.min(18, d)) + 42) / 60 * (H - 84);
    const mag = (fr) => (on ? filterMag(fr, fc, q, type) : 1);
    let low = 99, lvl = 0;
    for (let i = 0; f && i < 8; i++) if (f[i] >= 0 && f[8 + i] > 0.01) { low = Math.min(low, f[i]); lvl = Math.max(lvl, f[8 + i]); }
    if (low < 99) {
      const f0 = 440 * Math.pow(2, (low - 69) / 12);
      for (let k = 1; k <= 40; k++) {
        const fr = f0 * k, x = lx(fr);
        if (x > W) break;
        const a = db(1 / k) - 6, after = a + db(mag(fr));
        ctx.fillStyle = C.steel;
        ctx.fillRect(x, yd(a), 3, H - yd(a));
        ctx.fillStyle = rgba(C.fog, 0.25 + 0.5 * lvl);
        ctx.fillRect(x, yd(after), 3, H - yd(after));
      }
    }
    ctx.beginPath();
    for (let x = 0; x <= W; x += 2) { const y = yd(db(mag(30 * Math.pow(16000 / 30, x / W)))); x ? ctx.lineTo(x, y) : ctx.moveTo(x, y); }
    ctx.strokeStyle = on ? C.ice : C.slate;
    ctx.lineWidth = 2.8;
    ctx.stroke();
    ctx.lineTo(W, H); ctx.lineTo(0, H); ctx.closePath();
    ctx.fillStyle = rgba(C.ice, on ? 0.08 : 0);
    ctx.fill();
    if (on) { ctx.fillStyle = rgba(C.ice, 0.35); ctx.fillRect(lx(fc) - 1, 0, 2, H); }
  }

  // ------------------------------------------------------------------ LIVE: the gestures, shown then gone
  let shown = null, goneAt = 0;
  const api = { hold: false, onParam: null };
  function show(key, hold, build) {
    if (!shown || shown.key !== key) {
      if (shown) {                       /* the one before out quickly (80 ms), the next in over it */
        const old = shown.el;
        old.classList.remove("in");
        old.classList.add("swap");
        setTimeout(() => old.remove(), 120);
      }
      const el = h("div", "ev");
      now.appendChild(el);
      shown = { key, el };
      S.lit = new Set();                 /* (the keys' notes lit only while their gesture shows) */
      requestAnimationFrame(() => el.classList.add("in"));
    }
    build(shown.el);
    fitWide(shown.el);
    goneAt = performance.now() + hold;
  }
  /* a value too wide for the box (1/8 BEAT beside PULSE): its type smaller, never under the FM-1 small */
  function fitWide(el) {
    el.querySelectorAll(".val, .bval, .scn, .notes").forEach((t) => {
      const box = t.parentElement, over = Math.max(box.scrollWidth - box.clientWidth, t.scrollWidth - t.clientWidth);
      if (over <= 0) return;
      const f = parseFloat(getComputedStyle(t).fontSize), w = t.scrollWidth;
      t.style.fontSize = `${Math.max(28, Math.floor(f * (w - over) / w * 0.98))}px`;
    });
  }
  function clear() {
    shown = null;
    now.innerHTML = "";
    tweens.length = 0;
    struck.length = 0;
    S.lit = new Set();
  }
  const kicker = (key, word) => `<div class="kick">${key ? `<span class="cap">${key}</span>` : ""}<span>${word}</span></div>`;
  /* a setting turned: its page, its badge and name, its value and gauge; the FM-1 small, the knob lit */
  function setting(key, kickKey, kickWord, badge, p, v, lit) {
    touchedAt[p] = performance.now();
    if (MIX.includes(p)) mixAt = touchedAt[p];
    show(key, 2200, (el) => {
      el.innerHTML = `<div class="col">${kicker(kickKey, kickWord)}
          <div class="row"><div class="name">${badge}${P[p].name}</div><div class="val">${valueHTML(p, v)}</div></div>
          ${gaugeHTML(p, v)}</div>${mini(lit)}`;
    });
  }
  function knob(pageName, cell, v) {
    const g = page(pageName), p = g.p[cell];
    if (p < 0 || !P[p]) return;          /* (a page's empty cell: CLOUD III's fourth) */
    if (S.v[p] !== v && api.onParam) api.onParam(p, v);
    S.v[p] = v;
    S.changed = true;
    hud();
    setting(`knob:${p}`, g.button, pageName, `<i class="bd">${cell + 1}</i>`, p, v, { ["K" + (cell + 1)]: "on", [g.button]: "pg" });
  }
  /* a left knob turned, the hands on every page (A SELECT: KEYS, B PRESETS: the drone's preset, C ALGORITHM: BODY) */
  const HANDS = [[165, "A", "SELECT"], [113, "B", "PRESETS"], [114, "C", "ALGORITHM"]];
  function hand(p, v) {
    const k = HANDS.findIndex(([q]) => q === p);
    if (k < 0) return;
    if (S.v[p] !== v && api.onParam) api.onParam(p, v);
    S.v[p] = v;
    S.changed = true;
    hud();
    setting(`hand:${p}`, "", HANDS[k][2], `<i class="bd abc">${HANDS[k][1]}</i>`, p, v, { [HANDS[k][2]]: "on" });
  }
  /* a page opened: its four settings */
  function openPage(pageName) {
    const g = page(pageName);
    if (pageName === "MIXER") mixAt = performance.now();
    show(`page:${pageName}`, 1800, (el) => {
      el.innerHTML = `<div class="col">${kicker(g.button, pageName)}<div class="cells">${g.p.map((p, i) => (p < 0 || !P[p] ? `<div class="cell"></div>`
        : `<div class="cell"><span><i class="bd">${i + 1}</i>${P[p].name}</span><b>${valueHTML(p)}</b></div>`)).join("")}</div></div>${mini({ [g.button]: "on" })}`;
    });
  }
  /* a scene loaded: its number and name */
  function scene(f) {
    show(`scene:${f.name}`, 2600, (el) => {
      el.innerHTML = `<div class="col">${kicker("", f.slot ? `SCENE ${String(f.slot).padStart(2, "0")}` : "SCENE")}
        <div class="scn">${f.name}</div></div>`;
    });
  }
  /* GLINT's notes struck, as the engine strikes them (web_live 16..19, 48..51: seconds since struck), the last 40 */
  const struck = [];
  function strikes(f) {
    const lfo = [f[26] > 0, f[27] > 0, f[28] > 0];
    if (lfo.some((o, i) => o !== S.lfo[i])) S.lfo = lfo;
    for (let i = 0; i < 4; i++) {
      const n = f[16 + i], t = f[48 + i];
      if (n < 0 || t > 0.08 || f[20 + i] < 0.05) continue;
      const last = struck[struck.length - 1];
      if (last && last.n === n && performance.now() - last.at < 120) continue;
      struck.push({ n, at: performance.now() - t * 1000 });
      if (struck.length > 40) struck.shift();
      if (shown && shown.key === "keys:GLINT" && S.hand) goneAt = Math.max(goneAt, performance.now() + 2200);   /* (a hand on the keys) */
    }
  }
  /* GLINT's walk, drawn: each note struck a star at its height, flowing left at RATE's pace (the River's ARP strip) */
  function drawWalk(cv, t) {
    const ctx = cv.getContext("2d"), W = cv.width, H = cv.height, x1 = W - 70, top = 26, bot = H - 22;
    const m = /^1\/(\d+)(T?)$/.exec(text(46)), bpm = (S.v[SC.tempo] || 90) + 30;
    const step = Math.max(0.06, (m ? 96 / +m[1] * (m[2] ? 2 / 3 : 1) : 6) * 60 / (24 * bpm)), slot = 40;
    const hits = struck.filter((s) => x1 - (t - s.at) / 1000 / step * slot > 8);
    ctx.clearRect(0, 0, W, H);
    if (!hits.length) return;
    let lo = Math.min(...hits.map((s) => s.n)), hi = Math.max(...hits.map((s) => s.n));
    if (hi - lo < 12) { const c = Math.floor((hi + lo) / 2); lo = c - 6; hi = c + 6; }
    const Y = (n) => bot - (n - lo) * (bot - top) / (hi - lo);
    const pts = hits.map((s) => [x1 - (t - s.at) / 1000 / step * slot, Y(s.n), s]);
    ctx.beginPath();
    pts.forEach(([x, y], j) => (j ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
    ctx.strokeStyle = "rgba(110,208,255,0.42)";
    ctx.lineWidth = 2.2;
    ctx.lineJoin = ctx.lineCap = "round";
    ctx.stroke();
    pts.forEach(([x, y]) => {
      const fresh = Math.max(0, 1 - (x1 - x) / slot / 3), fade = Math.min(1, (x - 8) / 160);
      glint(ctx, x, y, 13 + 12 * fresh, fade);
      ctx.globalAlpha = (0.7 + 0.3 * fresh) * fade;
      ctx.fillStyle = C.ice;
      star(ctx, x, y, 7.6 + 6.8 * fresh);
      ctx.globalAlpha = fade;
      ctx.fillStyle = fresh > 0.5 ? C.white : C.fog;
      ctx.beginPath();
      ctx.arc(x, y, 2.2 + 1.8 * fresh, 0, Math.PI * 2);
      ctx.fill();
      ctx.globalAlpha = 1;
    });
    const last = pts[pts.length - 1];
    ctx.font = '600 30px "Barlow Semi Condensed", sans-serif';
    ctx.letterSpacing = "2px";
    ctx.textAlign = "left";
    ctx.textBaseline = "middle";
    ctx.fillStyle = C.ice;
    ctx.fillText(R.noteName(last[2].n), last[0] + 18, last[1]);
  }
  /* keys played: the drones' notes named (lit in the river), or GLINT's walk */
  function keys(track, notes, words) {
    S.notes = track === "GLINT";
    hud();
    show(`keys:${track}`, 2600, (el) => {
      if (track === "GLINT") {
        if (!el.querySelector("canvas")) el.innerHTML = `<div class="col wide">${kicker("KEYS", `GLINT · ${words}`)}<canvas class="walk" width="900" height="210"></canvas></div>`;
        return;
      }
      S.lit = new Set(notes.map((n) => n % 12));
      el.innerHTML = `<div class="col">${kicker("KEYS", "DRONE")}<div class="notes">${notes.map((n) => `<span>${R.noteName(n)}</span>`).join("")}</div>
        <div class="words">${words}</div></div>${mini(keysFor(notes))}`;
    });
  }
  /* a button: its key and what it set */
  function button(key, word, value, cls = "") {
    show(`btn:${key}:${value}`, 1800, (el) => {
      el.innerHTML = `<div class="col">${kicker(key, word)}<div class="bval ${cls}">${value}</div></div>${mini({ [key]: "on" })}`;
    });
  }

  /* a knob turned smoothly, as a hand does: from its value now by `by` over ms */
  const tweens = [];
  function turn(pageName, cell, by, ms = 1800) {
    const p = page(pageName).p[cell], from = S.v[p], to = Math.max(0, Math.min(P[p].max, from + by));
    for (let i = tweens.length - 1; i >= 0; i--) if (tweens[i].p === p) tweens.splice(i, 1);
    tweens.push({ pageName, cell, p, from, to, t0: performance.now(), ms });
  }
  function tick(t, f) {
    modulate(f);
    for (let i = tweens.length - 1; i >= 0; i--) {
      const w = tweens[i], u = Math.min(1, (t - w.t0) / w.ms), e = u < 0.5 ? 2 * u * u : 1 - Math.pow(-2 * u + 2, 2) / 2;
      const v = Math.round(w.from + (w.to - w.from) * e);
      if (v !== S.v[w.p] || u === 1) knob(w.pageName, w.cell, v);
      if (u === 1) tweens.splice(i, 1);
    }
    if (shown && !api.hold && t > goneAt) {
      const el = shown.el;
      el.classList.remove("in");
      el.classList.add("out");
      setTimeout(() => el.remove(), 700);
      shown = null;
      S.lit = new Set();
    }
    if (shown && shown.key === "keys:GLINT") { const cv = shown.el.querySelector("canvas"); if (cv) drawWalk(cv, t); }
    for (let i = 0; i < 3; i++) drawLfo(i, f);
    for (let k = 0; k < 3; k++) drawFx(k, t, f);
  }
  /* a setting moved by something other than a knob shown (MIDI, a scene's morph): its box's row lit all the same */
  const touch = (p) => { touchedAt[p] = performance.now(); if (MIX.includes(p)) mixAt = touchedAt[p]; };

  const logo = $(".brand canvas.logo");   /* FRASIL's logo, alive (logo.js) */
  if (logo && window.PerformLogo) PerformLogo.start(logo);

  Object.assign(api, { factory, S, page, values, text, look, hud, fit, clear, knob, hand, turn, openPage, scene, keys,
                      button, strikes, tick, touch, transport });
  /* (a getter of its own: Object.assign would have copied its value once, null) */
  Object.defineProperty(api, "showing", { get: () => shown && shown.key, enumerable: true });
  return api;
})();
