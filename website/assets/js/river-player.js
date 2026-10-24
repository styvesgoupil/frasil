/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The River's ▶: a shard played in the browser by FRASIL's own engine (the Play page's app.wasm, in fm1-base's
 * AudioWorklet: base/web/emu/worklet.js; FRASIL's part, app/web/emu/app_worklet.js, takes {type: "app", op: "scene",
 * bytes} and {op: "stop"}: app/web.c). Nothing loads until the first play.
 *   RiverPlayer.play(bytes, id)   a scene_t (Uint8Array); one shard at a time (another one replaces it) -> true if it sounds
 *   RiverPlayer.stop()            let it go: a short fade, then the audio sleeps until the next play
 *   RiverPlayer.wave(n)           what sounds now, n points of its waveform (-1..1), or null
 *   RiverPlayer.level()           its loudness now (RMS, 0..1)
 *   RiverPlayer.state             "idle" | "loading" | "playing";  RiverPlayer.id: the id given to play()
 *   events (addEventListener): loading, playing, stopped, error; event.detail = {id, message?, code?}
 *   (code "missing": the engine is not on this site yet; "unsupported": no AudioWorklet in this browser)
 * RiverPlayer.base: where emu/ is (default "emu/", beside the page). */
"use strict";
window.RiverPlayer = (() => {
  const ev = new EventTarget();
  const emit = (type, detail) => ev.dispatchEvent(new CustomEvent(type, { detail }));
  const fail = (message, code) => Object.assign(new Error(message), { code });
  const MISSING = "Sound needs a moment: the engine isn't on this site yet. Try again a little later.";
  let ac = null, node = null, gain = null, tap = null, booting = null, sleepT = 0, seq = 0;
  let current = null;              // {id, sounding}
  let tbuf = null;

  function teardown() {
    try { if (node) node.disconnect(); } catch (_) { /* gone already */ }
    try { if (ac) ac.close(); } catch (_) { /* closed already */ }
    ac = node = gain = tap = null;
  }

  /* the engine: an AudioContext (made at once, inside the click), the worklet's modules, app.wasm, booted */
  function boot() {
    if (booting) return booting;
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return Promise.reject(fail("This browser can't play sound here.", "unsupported"));
    try { ac = new AC({ sampleRate: 44100, latencyHint: "playback" }); } catch (_) { ac = new AC(); }
    if (!ac.audioWorklet) { teardown(); return Promise.reject(fail("This browser can't run FRASIL's engine (no AudioWorklet). Chrome, Edge, Firefox and Safari can.", "unsupported")); }
    const base = api.base;
    booting = (async () => {
      const wasm = await fetch(base + "app.wasm").then((r) => {
        if (!r.ok) throw fail(MISSING, "missing");
        return r.arrayBuffer();
      }, () => { throw fail(MISSING, "missing"); });
      try {
        await ac.audioWorklet.addModule(base + "app_worklet.js");
        await ac.audioWorklet.addModule(base + "worklet.js");
      } catch (_) { throw fail(MISSING, "missing"); }
      node = new AudioWorkletNode(ac, "fm1", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
      gain = ac.createGain();
      tap = ac.createAnalyser();
      tap.fftSize = 2048;
      tap.smoothingTimeConstant = 0.6;
      node.connect(gain);
      gain.connect(ac.destination);
      gain.connect(tap);
      node.onprocessorerror = () => {
        const id = current && current.id;
        current = null;
        teardown();
        booting = null;
        emit("error", { id, message: "The sound engine stopped. Press play again.", code: "crash" });
      };
      await new Promise((res, rej) => {
        const t = setTimeout(() => rej(fail("The sound engine didn't start. Press play again.", "timeout")), 20000);
        node.port.onmessage = (e) => {
          if (e.data && e.data.type === "ready") {
            clearTimeout(t);
            node.port.postMessage({ type: "app", op: "quiet" });   // (no screen here: its frames not sent)
            res();
          }
        };
        node.port.postMessage({ type: "load", wasm, store: {}, master: 2800 }, [wasm]);
      });
    })();
    booting.catch(() => { teardown(); booting = null; });   // (the next play tries again from the start)
    return booting;
  }

  async function play(bytes, id) {
    if (!(bytes instanceof Uint8Array) || !bytes.length) throw new TypeError("RiverPlayer.play: a scene_t, as a Uint8Array");
    const token = ++seq;
    if (current && current.id !== id) emit("stopped", { id: current.id, replaced: true });
    current = { id, sounding: false };
    clearTimeout(sleepT);
    const cold = !node;
    if (cold) emit("loading", { id });
    try {
      await boot();
      if (token !== seq) return false;                 // (another play, or stop, came meanwhile)
      if (ac.state !== "running") await ac.resume();
      if (token !== seq) return false;
      const t = ac.currentTime;
      gain.gain.cancelScheduledValues(t);
      gain.gain.setValueAtTime(cold ? 1 : Math.min(gain.gain.value, 1), t);
      gain.gain.linearRampToValueAtTime(1, t + 0.06);
      node.port.postMessage({ type: "app", op: "scene", bytes: Array.from(bytes) });
      current.sounding = true;
      emit("playing", { id });
      return true;
    } catch (e) {
      if (token === seq) {
        current = null;
        emit("error", { id, message: e.message || String(e), code: e.code || "error" });
      }
      return false;
    }
  }

  function stop() {
    if (!current) return;
    const { id } = current;
    current = null;
    seq++;
    if (node && ac) {
      node.port.postMessage({ type: "app", op: "stop" });
      const t = ac.currentTime;
      gain.gain.cancelScheduledValues(t);
      gain.gain.setValueAtTime(gain.gain.value, t);
      gain.gain.setTargetAtTime(0, t, 0.28);
      clearTimeout(sleepT);
      sleepT = setTimeout(() => { if (!current && ac && ac.state === "running") ac.suspend().catch(() => {}); }, 1800);
    }
    emit("stopped", { id });
  }

  function wave(n = 64) {
    if (!tap || !current || !current.sounding) return null;
    if (!tbuf) tbuf = new Float32Array(tap.fftSize);
    tap.getFloatTimeDomainData(tbuf);
    const out = new Float32Array(n), step = tbuf.length / n;
    for (let i = 0; i < n; i++) {                    // each point the mean of its stretch: a calm line, not hiss
      let s = 0;
      const a = Math.floor(i * step), b = Math.floor((i + 1) * step);
      for (let j = a; j < b; j++) s += tbuf[j];
      out[i] = s / Math.max(1, b - a);
    }
    return out;
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
    play, stop, wave, level,
    get state() { return !current ? "idle" : current.sounding ? "playing" : "loading"; },
    get id() { return current ? current.id : null; },
    addEventListener: ev.addEventListener.bind(ev),
    removeEventListener: ev.removeEventListener.bind(ev),
  };
  return api;
})();
