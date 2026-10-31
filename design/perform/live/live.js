/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Perform, live. CONNECT: the FM-1 itself over Web MIDI (FRASIL with PERFORM, app.c app_sysex: F0 7D 46 50): it says
 * what sounds 20 times a second and each gesture as it comes (a page, a knob, a scene, a button); the page says HELLO
 * every 2 s (not heard for 5 s, the FM-1 goes quiet). DEMO: FRASIL's factory scenes in the browser, with sound, in the
 * River's player (website/assets/js/river-player.js: app.wasm in an AudioWorklet, a fresh engine per scene), the
 * gestures from a hand: AUTO's (a knob turned every few seconds, a scene every seven) or the computer's keys.
 *   Keys (DEMO): ← → the scene · TAB / SHIFT TAB the page · 1-4 its knob · ↑ ↓ (or the wheel) turn it (SHIFT: faster) ·
 *         K the keys' notes · SPACE AUTO · (both) A / B the direction · V the view · H the header · F full screen · ESC back
 * Two views: LUCID (the river, the dock, the header) and DREAM (visuals.js: light only, the whole window, from what
 * sounds, what is heard and the hand), kept in this browser. */
"use strict";
(() => {
  const VERSION = "1.1.11";               /* Perform's own, shown on the first screen (FRASIL's comes with the FM-1) */
  const SC = window.PERFORM_SCHEMA, P = SC.params, ID = SC.ids, R = PerformRiver, Sh = PerformShow, S = Sh.S, pl = RiverPlayer;
  const $ = (s) => document.querySelector(s);
  pl.base = "../../../app/build/emu/";
  pl.follow = true;

  /* nothing sounding yet: every voice free */
  const BLANK = new Float32Array(53);
  [0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 24, 38, 39, 40].forEach((i) => (BLANK[i] = -1));
  /* what the engine lets the page set (app/web.c web_param): the scene's settings, TEMPO */
  const settable = (p) => p < SC.nsound || p === SC.tempo;
  let mode = "";                         /* "fm1" | "demo" | "" (the connect screen) */
  Sh.onParam = (p, v) => { if (mode === "demo" && settable(p)) pl.param(p, v); };
  Sh.onGesture = (kind, d) => V && V.gesture(kind, d);

  // ------------------------------------------------------------------ the scenes
  let at = -1, started = false;
  function start(i) {
    at = (i + Sh.factory.length) % Sh.factory.length;
    const f = Sh.factory[at];
    Sh.clear();
    Sh.values(f);
    Sh.scene(f);
    pl.play(f.bytes, at);
    hand.sceneAt = performance.now();
    hand.moves = 0;
    hand.keys = false;
  }
  pl.addEventListener("error", (e) => say(e.detail && e.detail.message ? e.detail.message.toUpperCase() : "NO SOUND"));

  // ------------------------------------------------------------------ the computer's keys, standing in for the FM-1's
  const PAGES = ["SOUND", "DRONE TONE", "DRONE MOTION", "CLOUD", "GENERATIVE FX", "GLINT I", "GLINT ARP", "BASS",
                 "MIXER", "LFO 1", "CHOIR", "PARTIALS", "BREATH", "MASTER FILTER", "LOFI TAPE"].filter((n) => Sh.page(n));
  let pg = 0, cell = 0;
  const cells = () => Sh.page(PAGES[pg]).p.map((p, i) => (settable(p) ? i : -1)).filter((i) => i >= 0);
  function toPage(d) {
    pg = (pg + d + PAGES.length) % PAGES.length;
    cell = cells()[0] || 0;
    Sh.openPage(PAGES[pg]);
  }
  function pick(n) {
    if (!cells().includes(n)) return;
    cell = n;
    const p = Sh.page(PAGES[pg]).p[n];
    Sh.knob(PAGES[pg], n, S.v[p]);
  }
  function nudge(dir, fast) {
    const p = Sh.page(PAGES[pg]).p[cell];
    if (!settable(p)) return;
    const step = P[p].kind === "gauge" && P[p].max > 40 ? (fast ? 10 : 3) : 1;
    Sh.knob(PAGES[pg], cell, Math.max(0, Math.min(P[p].max, S.v[p] + dir * step)));
  }
  const yours = () => { if (S.auto) { S.auto = false; Sh.hud(); } };

  /* the keys' notes, now: GLINT's as it strikes them if it rings, else the drones' */
  function keysNow() {
    const f = pl.live() || BLANK, v = S.v;
    let ringing = false;
    for (let i = 0; i < 4; i++) if (f[16 + i] >= 0 && f[20 + i] > 0.1 && f[48 + i] < 1.5) ringing = true;
    if (ringing) {
      const words = v[123] === 1 ? `ARP · ${Sh.text(45, v[45])} · ${Sh.text(46, v[46])}` : v[123] === 2 ? "SEQ" : Sh.text(37, v[37]);
      Sh.keys("GLINT", [], words);
      return;
    }
    const notes = [...new Set(Array.from(f.slice(0, 8)).filter((n, i) => n >= 0 && f[8 + i] > 0.01))].sort((a, b) => a - b);
    if (notes.length) Sh.keys("DRONE", notes.slice(0, 6), `${Sh.text(33, v[33])} ${Sh.text(34, v[34])} · ${Sh.text(35, v[35])}`);
  }

  // ------------------------------------------------------------------ AUTO: a hand on the FM-1
  const MOVES = [["SOUND", 1], ["SOUND", 2], ["SOUND", 0], ["DRONE TONE", 1], ["DRONE TONE", 0], ["DRONE TONE", 2],
                 ["CLOUD", 0], ["CLOUD", 1], ["GENERATIVE FX", 2], ["MIXER", 2], ["MIXER", 1], ["GLINT ARP", 1],
                 ["LFO 1", 2], ["CLOUD", 2], ["SOUND", 3]];
  const hand = { next: 0, moves: 0, sceneAt: 0, keys: false, last: "" };
  function handTick(now) {
    if (!S.auto || !started) return;
    if (!hand.keys && now - hand.sceneAt > 3600) {
      keysNow();
      hand.keys = true;
      hand.next = now + 4600;
      return;
    }
    if (now < hand.next) return;
    if (hand.moves >= 7) { start(at + 1); return; }
    const ok = MOVES.filter(([g, c]) => {
      const p = Sh.page(g).p[c];
      return settable(p) && `${g}:${c}` !== hand.last && (g !== "GLINT ARP" || S.v[123] === 1);
    });
    const [g, c] = ok[Math.floor(Math.random() * ok.length)], p = Sh.page(g).p[c], max = P[p].max, v = S.v[p];
    const sign = v > max * 0.62 ? -1 : v < max * 0.38 ? 1 : Math.random() < 0.5 ? -1 : 1;
    const by = P[p].kind === "gauge" && max > 40 ? sign * Math.round(max * (0.16 + Math.random() * 0.22)) : sign;
    Sh.turn(g, c, by, 1700 + Math.random() * 1000);
    hand.last = `${g}:${c}`;
    hand.moves++;
    hand.next = now + 5000 + Math.random() * 2000;
  }

  // ------------------------------------------------------------------ connect, demo
  const say = (t) => ($("#say").textContent = t || "");
  let helpT = 0;
  const HELP = {
    demo: "<kbd>← →</kbd> SCENE · <kbd>TAB</kbd> PAGE · <kbd>1–4</kbd> KNOB · <kbd>↑ ↓</kbd> TURN · <kbd>K</kbd> KEYS · <kbd>SPACE</kbd> AUTO · <kbd>V</kbd> · <kbd>H</kbd> · <kbd>F</kbd>",
    fm1: "<kbd>V</kbd> · <kbd>H</kbd> · <kbd>F</kbd> · <kbd>ESC</kbd>",
  };
  function help() {
    $("#help").innerHTML = HELP[mode] || HELP.demo;
    $("#help").classList.remove("gone");
    clearTimeout(helpT);
    helpT = setTimeout(() => $("#help").classList.add("gone"), 7000);
  }
  function demo() {
    if (mode === "fm1") return;
    mode = "demo";
    $("#connect").hidden = true;
    $("#stage").dataset.frame = "live";
    started = true;
    S.auto = true;
    pl.unlock();
    start(at < 0 ? 0 : at);
    showView();
    poke(3000);
    help();
  }
  function back() {
    pl.stop();
    unplug();
    mode = "";
    started = false;
    S.hand = true;
    Sh.clear();
    document.body.classList.remove("vsh");
    $("#connect").hidden = false;
    $("#stage").dataset.frame = "connect";
    showView();
  }
  // ------------------------------------------------------------------ the views: LUCID ("device"), DREAM ("vis")
  const V = PerformVisuals.create($("#vis"));
  const viewBtns = document.querySelectorAll("#vsw button");
  let view = "device", both = 0;          /* both: until then, the two drawn (the one going fades) */
  try { if (localStorage.getItem("perform.view") === "vis") view = "vis"; } catch (_) { /* no storage here */ }
  function viewPick(k) {
    if (k === "vis" && !V.ok) return say("NO WEBGL HERE · LUCID ONLY");
    view = k;
    try { localStorage.setItem("perform.view", k); } catch (_) { /* no storage here */ }
    viewBtns.forEach((b) => { b.classList.toggle("on", b.dataset.v === k); b.setAttribute("aria-checked", b.dataset.v === k); });
    showView();
    poke(3000);
  }
  /* the view's switch shows for ms, then goes (the pointer's rest hides it with the cursor) */
  let pokeT = 0;
  function poke(ms) {
    clearTimeout(pokeT);
    document.body.classList.toggle("vsh", started);
    if (started) pokeT = setTimeout(() => document.body.classList.remove("vsh"), ms);
  }
  function showView() {
    const vis = started && view === "vis";
    if (vis !== document.body.classList.contains("vis")) both = performance.now() + 1000;
    document.body.classList.toggle("vis", vis);
    if (vis && mode === "fm1") listen();
  }
  viewBtns.forEach((b) => (b.onclick = () => { b.blur(); viewPick(b.dataset.v); }));
  viewPick(V.ok ? view : "device");

  /* H: the header bare (the logo in the middle, the transport at the right), or whole again */
  const bare = () => { Sh.hud(); $("#stage").classList.toggle("nohud"); };
  $("#demo-btn").onclick = demo;
  $("#connect-btn").onclick = connect;
  const full = () => (document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen());
  document.addEventListener("fullscreenchange", Sh.fit);

  /* the artist's name: kept in this browser, shown under the scene's (none: no BY line) */
  const artist = $("#artist");
  artist.value = S.artist;
  artist.focus();                        /* (the caret blinking in it as the page opens, at the name's end) */
  artist.setSelectionRange(artist.value.length, artist.value.length);
  artist.addEventListener("input", () => {
    S.artist = artist.value.trim().toUpperCase();
    try { localStorage.setItem("perform.artist", S.artist); } catch (_) { /* no storage here */ }
    Sh.hud();
  });
  addEventListener("keydown", (e) => {
    if (e.metaKey || e.ctrlKey || e.altKey) return;
    if (e.target === artist) { if (e.key === "Enter") artist.blur(); return; }   /* (typing a name: no shortcut) */
    const k = e.key;
    if (k === "f" || k === "F") return full();
    if (!started) { if ((k === "Enter" || k === " ") && !(e.target.closest && e.target.closest("button"))) { e.preventDefault(); demo(); } return; }
    if (k === "h" || k === "H") return bare();
    if (k === "v" || k === "V") return viewPick(view === "vis" ? "device" : "vis");
    if (k === "Escape" && !document.fullscreenElement) return back();
    if (mode !== "demo") return;
    if (k === "ArrowRight") start(at + 1);
    else if (k === "ArrowLeft") start(at - 1);
    else if (k === "Tab") { e.preventDefault(); yours(); toPage(e.shiftKey ? -1 : 1); }
    else if (k >= "1" && k <= "4") { yours(); pick(+k - 1); }
    else if (k === "ArrowUp" || k === "ArrowDown") { e.preventDefault(); yours(); nudge(k === "ArrowUp" ? 1 : -1, e.shiftKey); }
    else if (k === "k" || k === "K") keysNow();
    else if (k === " ") { e.preventDefault(); S.auto = !S.auto; hand.next = performance.now() + 1500; Sh.hud(); }
    else if (k === "h" || k === "H") bare();
    else if (k === "Escape" && !document.fullscreenElement) back();
    else if (k === "?") help();
  });
  addEventListener("wheel", (e) => {
    if (mode !== "demo") return;
    e.preventDefault();
    yours();
    nudge(e.deltaY < 0 ? 1 : -1, Math.abs(e.deltaY) > 40);
  }, { passive: false });
  /* the pointer hides when it rests (a projector, a film) */
  let idleT = 0;
  addEventListener("pointermove", () => {
    document.body.classList.remove("idle");
    poke(2500);
    clearTimeout(idleT);
    idleT = setTimeout(() => started && document.body.classList.add("idle"), 2500);
  });

  // ------------------------------------------------------------------ the FM-1 (PERFORM: app.c app_sysex, F0 7D 46 50)
  /* HELLO: the version, then flags (1: COMPUTER AUDIO, the FM-1's own output quiet while the page plays its sound) */
  const FP = [0xF0, 0x7D, 0x46, 0x50], HELLO = () => [...FP, 1, 1, A.stream ? 1 : 0, 0xF7], BYE = [...FP, 2, 0xF7];
  const HAND_ENC = { 0: true, 1: true, 2: true }, K1 = 3;   /* plat.h: EN_SELECT, EN_ALGO, EN_PRESET, EN_K1.. */
  const fm = { access: null, inp: null, out: null, timer: 0, wait: 0, page: -1, oct: null, keys: 0, slot: -1, name: "",
               prev: null, cur: null, at: 0, drones: "", droneT: 0, dumped: false, kd: 0 };
  const sendFM = (m) => { try { fm.out && fm.out.send(m); } catch (_) { /* unplugged */ } };
  /* the sound: FM-1 AUDIO, its own output; COMPUTER AUDIO, the FM-1's USB audio (an input on this computer, "FRASIL
   * FM-1") played here while connected, the processing a voice call would get off. Kept in this browser. Played by an
   * audio element, not Web Audio: Chrome renders a stream there following the two clocks as they drift apart (the
   * FM-1's 44.1 kHz, this computer's output); Web Audio's source does not, and clicked */
  const A = { pick: "fm1", el: null, stream: null };
  try { if (localStorage.getItem("perform.audio") === "pc") A.pick = "pc"; } catch (_) { /* no storage here */ }
  const picks = document.querySelectorAll(".cn-audio button[data-a]");
  function audioPick(k) {
    A.pick = k;
    try { localStorage.setItem("perform.audio", k); } catch (_) { /* no storage here */ }
    picks.forEach((b) => { b.classList.toggle("on", b.dataset.a === k); b.setAttribute("aria-checked", b.dataset.a === k); });
  }
  picks.forEach((b) => (b.onclick = () => audioPick(b.dataset.a)));
  audioPick(A.pick);
  /* the FM-1's USB audio, as this computer hears it (a stream), or why not (a string) */
  async function fm1Stream() {
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) return "NO AUDIO INPUT HERE";
    const fm1 = async () => (await navigator.mediaDevices.enumerateDevices()).find((d) => d.kind === "audioinput" &&
      /FM-1/i.test(d.label) && d.deviceId !== "default" && d.deviceId !== "communications");   /* (itself, not "the default") */
    try {
      let dev = await fm1();
      if (!dev) {                        /* (the devices' names only once audio is allowed) */
        (await navigator.mediaDevices.getUserMedia({ audio: true })).getTracks().forEach((t) => t.stop());
        dev = await fm1();
      }
      if (!dev) return "NO FM-1 AUDIO · PLUG IT IN, OR FM-1 AUDIO";
      return await navigator.mediaDevices.getUserMedia({ audio: { deviceId: { exact: dev.deviceId }, channelCount: { ideal: 2 },
        echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
    } catch (_) {
      return "AUDIO NOT ALLOWED · ALLOW IT IN THE ADDRESS BAR";
    }
  }
  async function audioOn() {
    if (A.stream) return "";
    const got = await fm1Stream();
    if (typeof got === "string") return got;
    A.stream = got;
    try {
      A.el = new Audio();
      A.el.srcObject = A.stream;
      /* never back into the FM-1 (an output too: a loop): this computer's own output, its speakers if that is the FM-1 */
      const outs = (await navigator.mediaDevices.enumerateDevices()).filter((d) => d.kind === "audiooutput");
      const sys = outs.find((d) => d.deviceId === "default") || outs[0];
      if (sys && /FM-1/i.test(sys.label)) {
        const other = outs.filter((d) => !/FM-1/i.test(d.label) && d.deviceId !== "default" && d.deviceId !== "communications");
        const spk = other.find((d) => /speaker|haut-parleur|built-in|interne/i.test(d.label)) || other[0];
        if (!spk || !A.el.setSinkId) { audioOff(); return "THIS COMPUTER PLAYS THROUGH THE FM-1 · PICK ITS SPEAKERS"; }
        await A.el.setSinkId(spk.deviceId);
      }
      await A.el.play();
      return "";
    } catch (_) {
      audioOff();
      return "AUDIO NOT ALLOWED · ALLOW IT IN THE ADDRESS BAR";
    }
  }
  function audioOff() {
    if (A.stream) A.stream.getTracks().forEach((t) => t.stop());
    if (A.el) { A.el.pause(); A.el.srcObject = null; }
    A.stream = A.el = null;
  }
  /* VISUALS hear the FM-1: COMPUTER AUDIO's stream, else one of their own, only listened to (the FM-1 not quieted:
   * HELLO's flag is COMPUTER AUDIO's). An analyser in an AudioContext that plays nothing */
  const L = { ac: null, an: null, own: null, busy: false };
  async function listen() {
    if (L.an || L.busy || mode !== "fm1") return;
    L.busy = true;
    let stream = A.stream;
    if (!stream) {
      const got = await fm1Stream();
      if (typeof got !== "string" && mode === "fm1") stream = L.own = got;
      else if (typeof got !== "string") got.getTracks().forEach((t) => t.stop());
    }
    if (stream) {
      try {
        L.ac = new AudioContext({ latencyHint: "playback" });
        L.an = L.ac.createAnalyser();
        L.an.fftSize = 2048;
        L.an.smoothingTimeConstant = 0.5;
        L.ac.createMediaStreamSource(stream).connect(L.an);
        if (L.ac.state === "suspended") L.ac.resume();
      } catch (_) { unlisten(); }
    }
    L.busy = false;
  }
  function unlisten() {
    if (L.own) L.own.getTracks().forEach((t) => t.stop());
    if (L.ac) L.ac.close().catch(() => {});
    L.ac = L.an = L.own = null;
  }
  async function connect() {
    if (!navigator.requestMIDIAccess) return say("NO WEB MIDI HERE · CHROME OR EDGE");
    try { fm.access = fm.access || await navigator.requestMIDIAccess({ sysex: true }); }
    catch (_) { return say("MIDI NOT ALLOWED · ALLOW IT IN THE ADDRESS BAR"); }
    const find = (ports) => [...ports.values()].find((p) => /FM-1/i.test(p.name) && p.state !== "disconnected");
    fm.inp = find(fm.access.inputs);
    fm.out = find(fm.access.outputs);
    if (!fm.inp || !fm.out) return say("NO FM-1 · PLUG IT IN");
    if (A.pick === "pc") {
      say("LISTENING…");
      const err = await audioOn();
      if (err) return say(err);
    }
    fm.inp.onmidimessage = (e) => message(e.data);
    fm.access.onstatechange = (e) => {
      if (mode === "fm1" && e.port.state === "disconnected" && /FM-1/i.test(e.port.name)) { back(); say("FM-1 UNPLUGGED"); }
    };
    say("LISTENING…");
    sendFM(HELLO());
    clearInterval(fm.timer);
    fm.timer = setInterval(() => sendFM(HELLO()), 2000);
    clearTimeout(fm.wait);
    fm.wait = setTimeout(() => { if (mode !== "fm1") { unplug(); say("NO ANSWER · THIS FM-1 NEEDS FRASIL WITH PERFORM"); } }, 3000);
  }
  function unplug() {
    clearInterval(fm.timer);
    clearTimeout(fm.wait);
    unlisten();
    audioOff();
    if (fm.inp) fm.inp.onmidimessage = null;
    sendFM(BYE);
  }
  addEventListener("pagehide", () => sendFM(BYE));
  function live() {
    pl.stop();
    mode = "fm1";
    started = true;
    S.auto = false;
    fm.page = -1, fm.oct = null, fm.slot = -1, fm.prev = fm.cur = null, fm.dumped = false, fm.kd = 0;
    S.hand = false;
    $("#connect").hidden = true;
    $("#stage").dataset.frame = "live";
    say("");
    showView();
    poke(3000);
    help();
  }
  const ascii = (b) => String.fromCharCode(...b).replace(/\s+$/, "");
  /* an earlier FRASIL's numbers in this page's (the schema's), told by HELLO's NPARAM: 2.5 and 2.5.1 (182) before
   * CHOIR III's SOFT, their settings from SOFT's on and their pages from CHOIR III's on one lower */
  const EARLIER = { 182: { p: ID.P_CH_SOFT, g: ID.PG_CHOEUR3 } };
  const PF_VER = 1;                     /* PERFORM's own version, the FM-1's HELLO's first byte (app.c PF_VER) */
  let pOf = (p) => p, gOf = (g) => g;
  function numbering(n) {
    const e = n !== P.length && EARLIER[n];
    pOf = e ? (p) => (p >= e.p ? p + 1 : p) : (p) => p;
    gOf = e ? (g) => (g >= e.g ? g + 1 : g) : (g) => g;
    fm.nparam = n;
  }
  function message(d) {
    if (d.length < 6 || d[0] !== 0xF0 || d[1] !== 0x7D || d[2] !== 0x46 || d[3] !== 0x50) return;
    const b = d.subarray(5, d.length - 1), two = (i) => (b[i] << 7) | b[i + 1];
    switch (d[4]) {
      case 1: {                          /* HELLO ver, NPARAM, FRASIL's version: only a FRASIL this page knows */
        const n = b.length >= 3 ? two(1) : 0, fw = b.length > 3 ? ascii(b.subarray(3)) : "";
        if (b[0] !== PF_VER || (n !== P.length && !EARLIER[n])) {
          if (mode === "fm1") back();
          unplug();
          return say(n > P.length || b[0] > PF_VER ? `FRASIL ${fw} IS NEWER THAN THIS PAGE · RELOAD IT`
                                  : `FRASIL ${fw || "ON THIS FM-1"} · NOT SUPPORTED · UPDATE IT`);
        }
        numbering(n);
        if (mode !== "fm1") live();
        break;
      }
      case 2: {                          /* SCENE slot, dirty, name */
        const slot = b[0], name = ascii(b.subarray(2)) || "NEW SHARD";
        const again = slot !== fm.slot || name !== fm.name;
        fm.slot = slot, fm.name = name;
        S.scene = name;
        S.slot = slot < 16 ? slot + 1 : 0;
        S.changed = !!b[1];
        Sh.hud();
        if (again) Sh.scene({ name, slot: S.slot });
        break;
      }
      case 3: {                          /* VALUES first, n, values */
        const first = two(0), n = b[2];
        for (let i = 0; i < n; i++) S.v[pOf(first + i)] = two(3 + 2 * i);
        if (first + n >= (fm.nparam || P.length)) fm.dumped = true;
        Sh.hud();
        break;
      }
      case 4: value(pOf(two(0)), two(2), b[4]); break;
      case 5: page(gOf(b[0]), b[1] - 3, b[2]); break;
      case 6: frame(b); break;
      case 8: keysHeld(b[0] | b[1] << 7 | b[2] << 14 | b[3] << 21); break;
      default: break;
    }
  }
  /* a setting that moved: a knob's on its page, a hand's, FREEZE; else held quietly */
  function value(p, v, enc) {
    const g = fm.page >= 0 && SC.pages[fm.page];
    if (enc >= K1 && enc < K1 + 4 && g && g.p[enc - K1] === p) return Sh.knob(g.name, enc - K1, v);
    if (HAND_ENC[enc] && (p === ID.P_KB || p === ID.P_DR_PRESET || p === ID.P_DR_FULL)) return Sh.hand(p, v);
    S.v[p] = v;
    Sh.touch(p);
    if (p === ID.P_FREEZE) Sh.button("FX", "FREEZE", v ? "ON" : "OFF", v ? "ice" : "");
    else if (enc !== 127 && g) {         /* a knob on a page that shows something else (SCENES' slots...) */
      const c = g.p.indexOf(p);
      if (c >= 0) Sh.knob(g.name, c, v);
    }
  }
  function page(pg, oct, keys) {
    const first = fm.page < 0;
    if (pg !== fm.page && SC.pages[pg] && !first) Sh.openPage(SC.pages[pg].name);
    fm.page = pg;
    if (fm.oct !== null && oct !== fm.oct) Sh.button(oct > fm.oct ? "OCT+" : "OCT-", "OCTAVE", `${oct > 0 ? "+" : ""}${oct}`);
    fm.oct = oct;
    S.oct = oct;
    fm.keys = keys;
    S.notes = keys === 1;
    Sh.hud();
  }
  /* what sounds, as web_live lays it out (the river's frame) */
  function frame(b) {
    const f = BLANK.slice();
    let i = 0;
    for (let v = 0; v < 8; v++) { const n = b[i++], l = b[i++]; f[v] = n === 127 ? -1 : n; f[8 + v] = l / 127; }
    for (let g = 0; g < 4; g++) { const n = b[i++], l = b[i++], t = b[i++]; f[16 + g] = n === 127 ? -1 : n; f[20 + g] = l / 127; f[48 + g] = t / 20; }
    const bn = b[i++], bl = b[i++];
    f[24] = bn === 127 ? -1 : bn;
    f[25] = bl / 127;
    for (let k = 0; k < 3; k++) f[26 + k] = (b[i] >> k) & 1;
    i++;
    for (let k = 0; k < 3 && i + 5 <= b.length; k++, i += 5) {   /* each LFO: where in its turn, its value, its turns */
      f[29 + k] = b[i] / 127;
      f[32 + k] = b[i + 1] / 127 * 2 - 1;
      f[35 + k] = (b[i + 2] << 14) | (b[i + 3] << 7) | b[i + 4];
    }
    for (let k = 0; k < 4 && i < b.length; k++) f[42 + k] = b[i++] / 127;   /* the tracks' meters (pf4 on) */
    Sh.transport(i < b.length ? b[i] : playing(f));                           /* the transport (pf7 on) */
    f[52] = i + 1 < b.length ? b[i + 1] / 127 : 0;                            /* LOFI TAPE's drag (2.5.2 on) */
    fm.prev = fm.cur || f;
    fm.cur = f;
    fm.at = performance.now();
    Sh.strikes(f);
    keysSeen(f);
  }
  /* the hand on THE KEYS (KEYS, pf8 on): a key down with the keys on GLINT shows its walk; it stays while a key is
   * held, then goes (an arpeggio, a sequence or SPARKLE striking on is not the hand) */
  function keysHeld(mask) {
    const down = mask & ~fm.kd;
    fm.kd = mask;
    S.hand = mask !== 0;
    if (down && fm.keys === 1) {
      const v = S.v;
      Sh.keys("GLINT", [], v[123] === 1 ? `ARP · ${Sh.text(45, v[45])} · ${Sh.text(46, v[46])}` : v[123] === 2 ? "SEQ" : Sh.text(37, v[37]));
    }
  }
  /* the keys, as what sounds shows them: new drones (the keys on DRONE) */
  function keysSeen(f) {
    const notes = [...new Set(Array.from(f.slice(0, 8)).filter((n, i) => n >= 0 && f[8 + i] > 0.05))].sort((a, b) => a - b);
    const key = notes.join(",");
    if (fm.keys === 0 && key !== fm.drones) {
      const added = notes.some((n) => !fm.drones.split(",").includes(String(n)));
      fm.drones = key;
      clearTimeout(fm.droneT);
      if (added && notes.length) fm.droneT = setTimeout(() => {
        const v = S.v;
        Sh.keys("DRONE", notes.slice(0, 6), `${Sh.text(33, v[33])} ${Sh.text(34, v[34])} · ${Sh.text(35, v[35])}`);
      }, 220);
    }
  }
  /* the transport from what sounds (DEMO; an FM-1 that does not say it): the drones sounding; GLINT's arpeggio or
   * sequence on a note within 1.5 s (web_live's 38, 39: never SPARKLE's glints, an effect, not GLINT playing; an FM-1
   * that does not say it: GLINT stopped); the bass sounding or its sequence on a step */
  let glAt = -1e9;
  function playing(f) {
    const now = performance.now();
    let dr = false;
    if (f[38] >= 0 || f[39] >= 0) glAt = now;
    for (let v = 0; v < 8; v++) if (f[v] >= 0 && f[8 + v] > 0.01) dr = true;
    const gl = now - glAt < 1500, bs = f[40] >= 0 || (f[24] >= 0 && f[25] > 0.01);
    return (dr ? 1 : 0) | (gl ? 2 : 0) | (bs ? 4 : 0);
  }
  /* the frame drawn now: between the last two the FM-1 sent (one frame behind, smooth) */
  function fmFrame(now) {
    if (!fm.cur) return BLANK;
    const u = Math.min(1, (now - fm.at) / 50), a = fm.prev, b = fm.cur, f = b.slice();
    for (let v = 0; v < 8; v++) if (a[v] === b[v]) f[8 + v] = a[8 + v] + (b[8 + v] - a[8 + v]) * u;
    for (let g = 0; g < 4; g++) if (a[16 + g] === b[16 + g]) {
      f[20 + g] = a[20 + g] + (b[20 + g] - a[20 + g]) * u;
      if (b[48 + g] >= a[48 + g]) f[48 + g] = a[48 + g] + (b[48 + g] - a[48 + g]) * u;
    }
    if (a[24] === b[24]) f[25] = a[25] + (b[25] - a[25]) * u;
    for (let k = 0; k < 3; k++) if (b[35 + k] === a[35 + k] && b[29 + k] >= a[29 + k]) {
      f[29 + k] = a[29 + k] + (b[29 + k] - a[29 + k]) * u;
      f[32 + k] = a[32 + k] + (b[32 + k] - a[32 + k]) * u;
    }
    return f;
  }

  // ------------------------------------------------------------------ the loop
  const river = R.create($("#river"));
  function loop(now) {
    handTick(now);
    const f = mode === "fm1" ? fmFrame(now) : (started && pl.live()) || BLANK;
    if (mode !== "fm1") {
      Sh.strikes(f);
      if (f[40] >= 0) S.v[ID.P_BQ_ON] = 1;   /* (the bass's SEQUENCE: not in a factory scene's sound, seen as it plays) */
      Sh.transport(started ? playing(f) : 0);
    }
    Sh.tick(now, f);
    const vis = document.body.classList.contains("vis");
    if (vis || now < both) V.draw(f, now, { n: (p) => Sh.eff(p) / (P[p].max || 1), scene: S.scene, artist: S.artist,
      bare: $("#stage").classList.contains("nohud"), an: mode === "demo" ? pl.analyser() : L.an });
    if (!vis || now < both) river.draw(f, Sh.look(), now);
    requestAnimationFrame(loop);
  }
  $("#version").textContent = $("#pf-ver").textContent = `PERFORM ${VERSION}`;
  Sh.values(Sh.factory[0]);
  Sh.fit();
  document.fonts.ready.then(() => requestAnimationFrame(loop));
})();
