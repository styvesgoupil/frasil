// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// FRASIL's part of the emulator's worklet (base/web/emu/worklet.js loads it first): the River plays a shard,
// {type: "app", op: "scene", bytes} (its scene_t: web_scene_size() bytes), and lets it go, {type: "app", op: "stop"} (app/web.c);
// {type: "app", op: "quiet"}: a page without the screen (the River), the screen's frames not sent;
// {type: "app", op: "param", p, x}: the River's editor, a setting set (web_param: as a MIDI CC learned on it sets it);
// {type: "app", op: "latch", t, n}, {op: "clear", t}, {op: "hold", t, n, down}: THE KEYS, what it plays (web_latch,
// web_clear, web_hold: t 0 the drones, 1 GLINT, 2 the bass; n a note as it sounds);
// {type: "app", op: "live", on}: while on, what sounds (web_live: web_live_size() floats) posted about 30 times a second,
// {type: "live", t (currentTime: what was rendered up to then), v (a Float32Array)}: the River's card follows the sound;
// {type: "app", op: "end"}: this engine is done (another one, fresh, plays the next shard): silent, its memory let go,
// and its processor's process() returning false from now on, so the browser can let it go too
globalThis.fm1App = {
  process(fm1) {
    if (fm1.quiet) fm1.lastFrame = currentTime;   // (worklet.js publishes a frame 1/30 s after the last)
    if (fm1.live && currentTime - fm1.liveAt >= 1 / 30) {
      const ex = fm1.ex, v = new Float32Array(fm1.mem.buffer, ex.web_live(), ex.web_live_size()).slice();
      fm1.liveAt = currentTime;
      fm1.port.postMessage({ type: "live", t: currentTime, v }, [v.buffer]);
    }
  },
  message(fm1, m) {
    if (m.op === "quiet") fm1.quiet = true;
    if (m.op === "end") {
      fm1.live = false;
      fm1.process = () => false;
      fm1.ex = fm1.mem = null;
      return;
    }
    const ex = fm1.ex;
    if (!ex) return;
    if (m.op === "live") {
      fm1.live = !!m.on && !!ex.web_live;
      fm1.liveAt = -1;
    } else if (m.op === "scene" && ex.web_scene_buf) {
      const b = Uint8Array.from(m.bytes || []).subarray(0, ex.web_scene_size());
      if (b.length !== ex.web_scene_size()) return;
      new Uint8Array(fm1.mem.buffer, ex.web_scene_buf(), b.length).set(b);
      ex.web_scene_play();
    } else if (m.op === "param" && ex.web_param) {
      ex.web_param(m.p >>> 0, m.x >>> 0);
    } else if (m.op === "latch" && ex.web_latch) {
      ex.web_latch(m.t >>> 0, m.n >>> 0);
    } else if (m.op === "clear" && ex.web_clear) {
      ex.web_clear(m.t >>> 0);
    } else if (m.op === "hold" && ex.web_hold) {
      ex.web_hold(m.t >>> 0, m.n >>> 0, m.down ? 1 : 0);
    } else if (m.op === "stop" && ex.web_scene_stop) {
      ex.web_scene_stop();
    }
  },
};
