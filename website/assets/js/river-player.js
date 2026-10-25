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
    if (e) {
      e.node.port.postMessage({ type: "app", op: "stop" });   // (its notes let go, then faded)
      retire(e, 1.2);
      clearTimeout(sleepT);
      sleepT = setTimeout(() => { if (!current && ac && ac.state === "running") ac.suspend().catch(() => {}); }, 1800);
    }
    emit("stopped", { id });
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

  const api = {
    base: "emu/",
    play, stop, unlock, wave, level,
    get state() { return !current ? "idle" : current.sounding ? "playing" : "loading"; },
    get id() { return current ? current.id : null; },
    get held() { return !!(current && !current.sounding && ac && ac.state === "suspended" && wasm); },   // (waiting for a click)
    addEventListener: ev.addEventListener.bind(ev),
    removeEventListener: ev.removeEventListener.bind(ev),
  };
  return api;
})();
