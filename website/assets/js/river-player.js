/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's ▶ (and the landing's LISTEN): a shard played in the browser by FRASIL's own engine (the Play page's
 * app.wasm, in fm1-base's AudioWorklet: base/web/emu/worklet.js; FRASIL's part, app/web/emu/app_worklet.js, takes
 * {type: "app", op: "scene", bytes}, {op: "stop"} and {op: "end"}: app/web.c). Each shard starts in an engine of its
 * own, fresh: nothing of the one before (its reverb, its echoes, its LFOs) blends into it; the one before fades out
 * and is let go. One AudioContext for the page, made at the first play. Nothing loads until then.
 *   RiverPlayer.play(bytes, id)   a scene_t (Uint8Array); one shard at a time (another one replaces it) -> true if it sounds
 *   RiverPlayer.stop()            let it go: a short fade, then the audio sleeps until the next play
 *   RiverPlayer.unlock()          in a click or a key: lets a play started before any (held back by the browser) sound
 *   RiverPlayer.wave(n)           what sounds now, n points of its waveform (-1..1), or null
 *   RiverPlayer.level()           its loudness now (RMS, 0..1)
 *   RiverPlayer.param(p, x)       the shard playing, a setting set as on the FM-1 (the editor: app_worklet.js op "param")
 *   RiverPlayer.keys(op)          the shard playing, what it plays: THE KEYS' {op: "latch", t, n} | {op: "clear", t} |
 *                                 {op: "hold", t, n, down} (app_worklet.js; app/web.c web_latch, web_clear, web_hold)
 *   RiverPlayer.scene(bytes)      the shard playing taken again as it is now (the editor's UNDO of what it plays)
 *   RiverPlayer.follow            true: each engine sends what sounds (app_worklet.js op "live"; the River's cards)
 *   RiverPlayer.live()            what sounds as it is heard now: app/web.c web_live's floats (Float32Array), or null
 *   RiverPlayer.state             "idle" | "loading" | "playing";  RiverPlayer.id: the id given to play()
 *   RiverPlayer.held              true while a play waits for the browser to let sound start (a click, a key)
 *   events (addEventListener): loading, playing, stopped, error; event.detail = {id, message?, code?}
 *   (code "missing": the engine is not on this site yet; "unsupported": no AudioWorklet in this browser)
 * RiverPlayer.base: where emu/ is (default "emu/", beside the page). */
"use strict";
window.RiverPlayer = (() => {
  const ev = new EventTarget();
  const emit = (type, detail) => ev.dispatchEvent(new CustomEvent(type, { detail }));
  const fail = (message, code) => Object.assign(new Error(message), { code });
  const MISSING = "Sound needs a moment: the engine isn't on this site yet. Try again a little later.";
  let ac = null, out = null, tap = null, wasm = null, ready = null, sleepT = 0, seq = 0;
  let engine = null;               // the one playing or starting: {node, gain}
  let current = null;              // {id, sounding}
  let tbuf = null;
  let heard = [];                  // what the one playing sent (follow): {t, v}, its last second or so
  let pend = [];                   // what the editor sends while it starts (param, keys): sent once it plays

  /* the page's audio: an AudioContext (made at once, inside the click), the worklet's modules and app.wasm, once */
  function setup() {
    if (ready) return ready;
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return Promise.reject(fail("This browser can't play sound here.", "unsupported"));
    try { ac = new AC({ sampleRate: 44100, latencyHint: "playback" }); } catch (_) { ac = new AC(); }
    if (!ac.audioWorklet) {
      try { ac.close(); } catch (_) { /* closed already */ }
      ac = null;
      return Promise.reject(fail("This browser can't run FRASIL's engine (no AudioWorklet). Chrome, Edge, Firefox and Safari can.", "unsupported"));
    }
    out = ac.createGain();
    tap = ac.createAnalyser();
    tap.fftSize = 2048;
    tap.smoothingTimeConstant = 0.6;
    out.connect(ac.destination);
    out.connect(tap);
    const base = api.base, m = document.querySelector('meta[name="frasil-engine"]');
    const v = m && m.content ? "?v=" + encodeURIComponent(m.content) : "";   // (the engine's version: no cache's old one)
    ready = (async () => {
      wasm = await fetch(base + "app.wasm" + v).then((r) => {
        if (!r.ok) throw fail(MISSING, "missing");
        return r.arrayBuffer();
      }, () => { throw fail(MISSING, "missing"); });
      try {
        await ac.audioWorklet.addModule(base + "app_worklet.js" + v);
        await ac.audioWorklet.addModule(base + "worklet.js" + v);
      } catch (_) { throw fail(MISSING, "missing"); }
    })();
    ready.catch(() => {                 // (the next play tries again from the start)
      try { ac.close(); } catch (_) { /* closed already */ }
      ac = out = tap = wasm = ready = null;
    });
    return ready;
  }

  /* a fresh engine: its own node and gain into the page's output, app.wasm booted (a copy: each its own memory) */
  async function start() {
    const node = new AudioWorkletNode(ac, "fm1", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
    const gain = ac.createGain();
    const e = { node, gain };
    node.connect(gain);
    gain.connect(out);
    node.onprocessorerror = () => {
      if (engine !== e) return;
      const id = current && current.id;
      current = null;
      retire(e, 0);
      engine = null;
      emit("error", { id, message: "The sound engine stopped. Press play again.", code: "crash" });
    };
    try {
      await new Promise((res, rej) => {
        const t = setTimeout(() => rej(fail("The sound engine didn't start. Press play again.", "timeout")), 20000);
        node.port.onmessage = (m) => {
          if (m.data && m.data.type === "ready") {
            clearTimeout(t);
            node.port.postMessage({ type: "app", op: "quiet" });   // (no screen here: its frames not sent)
            node.port.onmessage = (m2) => {                        // (what sounds, while it is the one playing)
              if (!m2.data || m2.data.type !== "live" || engine !== e) return;
              heard.push(m2.data);
              if (heard.length > 48) heard.splice(0, heard.length - 48);
            };
            if (api.follow) node.port.postMessage({ type: "app", op: "live", on: true });
            res();
          }
        };
        const w = wasm.slice(0);
        node.port.postMessage({ type: "load", wasm: w, store: {}, master: 2800 }, [w]);
      });
    } catch (err) {
      retire(e, 0);
      throw err;
    }
    return e;
  }

  /* an engine let go: faded over `fade` seconds (0: at once), then ended (silent, its memory freed) and disconnected */
  function retire(e, fade) {
    if (!e) return;
    const t = ac.currentTime;
    e.gain.gain.cancelScheduledValues(t);
    e.gain.gain.setValueAtTime(e.gain.gain.value, t);
    if (fade) e.gain.gain.setTargetAtTime(0, t, fade / 4);
    else e.gain.gain.setValueAtTime(0, t);
    setTimeout(() => {
      try { e.node.port.postMessage({ type: "app", op: "end" }); } catch (_) { /* gone already */ }
      try { e.node.disconnect(); e.gain.disconnect(); } catch (_) { /* gone already */ }
    }, Math.round(fade * 1000) + 60);
  }

  async function play(bytes, id) {
    if (!(bytes instanceof Uint8Array) || !bytes.length) throw new TypeError("RiverPlayer.play: a scene_t, as a Uint8Array");
    const token = ++seq;
    if (current && current.id !== id) emit("stopped", { id: current.id, replaced: true });
    current = { id, sounding: false };
    clearTimeout(sleepT);
    const old = engine;
    engine = null;
    heard = [];
    pend = [];
    if (old) retire(old, 0.12);         // (the one before fades out quickly while the fresh one boots)
    emit("loading", { id });
    try {
      await setup();
      if (token !== seq) return false;                 // (another play, or stop, came meanwhile)
      if (ac.state !== "running") await ac.resume();
      if (token !== seq) return false;
      const e = await start();
      if (token !== seq) { retire(e, 0); return false; }
      engine = e;
      e.node.port.postMessage({ type: "app", op: "scene", bytes: Array.from(bytes) });
      for (const m of pend) e.node.port.postMessage(m);
      pend = [];
      current.sounding = true;
      emit("playing", { id });
      return true;
    } catch (err) {
      if (token === seq) {
        current = null;
        emit("error", { id, message: err.message || String(err), code: err.code || "error" });
      }
      return false;
    }
  }

  function stop() {
    if (!current) return;
    const { id } = current;
    current = null;
    seq++;
    const e = engine;
    engine = null;
    heard = [];
    if (e) {
      e.node.port.postMessage({ type: "app", op: "stop" });   // (its notes let go, then faded)
      retire(e, 1.2);
      clearTimeout(sleepT);
      sleepT = setTimeout(() => { if (!current && ac && ac.state === "running") ac.suspend().catch(() => {}); }, 1800);
    }
    emit("stopped", { id });
  }

  /* the shard playing (or starting), a setting set: its engine as a MIDI CC learned on it sets it (app/web.c web_param).
   * One still starting gets it once its scene is in, in the order they came */
  function param(p, x) { send({ op: "param", p, x }); }
  /* THE KEYS: what it plays, edited (latch, clear) or played while a key is down (hold); the same order as the rest */
  function keys(o) { if (o && ["latch", "clear", "hold"].includes(o.op)) send(o); }
  /* the shard playing taken again as it is now (bytes: its scene_t), every note let go first, its sequences from the top */
  function scene(bytes) { if (current) send({ op: "scene", bytes: Array.from(bytes) }); }
  function send(o) {
    const m = { type: "app", ...o };
    if (engine) engine.node.port.postMessage(m);
    else if (current) pend.push(m);
  }

  /* inside a click or a key: the audio let start, where the browser held it back (a play waiting for it goes on) */
  function unlock() {
    if (ac && ac.state === "suspended") ac.resume().catch(() => {});
  }

  function wave(n = 64) {
    if (!tap || !current || !current.sounding) return null;
    if (!tbuf) tbuf = new Float32Array(tap.fftSize);
    tap.getFloatTimeDomainData(tbuf);
    const res = new Float32Array(n), step = tbuf.length / n;
    for (let i = 0; i < n; i++) {                    // each point the mean of its stretch: a calm line, not hiss
      let s = 0;
      const a = Math.floor(i * step), b = Math.floor((i + 1) * step);
      for (let j = a; j < b; j++) s += tbuf[j];
      res[i] = s / Math.max(1, b - a);
    }
    return res;
  }
  function level() {
    if (!tap || !current || !current.sounding) return 0;
    if (!tbuf) tbuf = new Float32Array(tap.fftSize);
    tap.getFloatTimeDomainData(tbuf);
    let s = 0;
    for (const v of tbuf) s += v * v;
    return Math.sqrt(s / tbuf.length);
  }

  /* what sounds as it is heard: the engine's frames (each what it had rendered at its time t) kept a moment, the one
   * the speakers are at now (the output's timestamp: the visuals with the sound, not ahead of it), blended with the
   * next where a value glides (a voice's level and the time since it was struck, while its note stays; an LFO within a
   * turn; the bass's time in its step; the meters); before the sound comes out, null. Its floats as app/web.c web_live
   * lays them out */
  const GLIDE = [8, 9, 10, 11, 12, 13, 14, 15, 20, 21, 22, 23, 25, 42, 43, 44, 45, 46, 48, 49, 50, 51];
  const NOTE = (k) => (k < 16 ? k - 8 : k < 24 ? k - 4 : k === 25 ? 24 : k >= 48 ? k - 32 : -1);   // (a voice's own note)
  function live() {
    if (!current || !current.sounding || !heard.length || !ac) return null;
    let at = ac.currentTime - (ac.outputLatency || 0) - (ac.baseLatency || 0);
    if (ac.getOutputTimestamp) {
      const o = ac.getOutputTimestamp();
      if (o && o.contextTime > 0) at = o.contextTime;
    }
    let i = heard.length - 1;
    while (i >= 0 && heard[i].t > at) i--;
    if (i < 0) return null;
    if (i > 2) heard.splice(0, i - 2), i = 2;                  // (what was heard long ago let go)
    const a = heard[i], b = heard[i + 1], v = a.v.slice();
    if (!b) return v;
    const f = Math.min(1, (at - a.t) / Math.max(1e-3, b.t - a.t));
    for (const k of GLIDE) {
      const note = NOTE(k);
      if (note < 0 || (a.v[note] === b.v[note] && (k < 48 || b.v[k] >= a.v[k]))) v[k] = a.v[k] + (b.v[k] - a.v[k]) * f;
    }
    for (let k = 29; k < 32; k++)                                // an LFO within a turn: its place, SHAPE's value
      if (b.v[k] >= a.v[k] && b.v[k + 6] === a.v[k + 6]) {
        v[k] = a.v[k] + (b.v[k] - a.v[k]) * f;
        if (Math.abs(b.v[k + 3] - a.v[k + 3]) < 0.5) v[k + 3] = a.v[k + 3] + (b.v[k + 3] - a.v[k + 3]) * f;   // (not across an edge)
      }
    if (a.v[40] === b.v[40] && b.v[41] >= a.v[41]) v[41] = a.v[41] + (b.v[41] - a.v[41]) * f;
    return v;
  }

  const api = {
    base: "emu/",
    follow: false,
    play, stop, unlock, wave, level, live, param, keys, scene,
    get state() { return !current ? "idle" : current.sounding ? "playing" : "loading"; },
    get id() { return current ? current.id : null; },
    get held() { return !!(current && !current.sounding && ac && ac.state === "suspended" && wasm); },   // (waiting for a click)
    addEventListener: ev.addEventListener.bind(ev),
    removeEventListener: ev.removeEventListener.bind(ev),
  };
  return api;
})();
