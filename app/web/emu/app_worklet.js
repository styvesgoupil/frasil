// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// FRASIL's part of the emulator's worklet (base/web/emu/worklet.js loads it first): the River plays a shard,
// {type: "app", op: "scene", bytes} (its scene_t: web_scene_size() bytes), and lets it go, {type: "app", op: "stop"} (app/web.c);
// {type: "app", op: "quiet"}: a page without the screen (the River), the screen's frames not sent;
// {type: "app", op: "end"}: this engine is done (another one, fresh, plays the next shard): silent, its memory let go,
// and its processor's process() returning false from now on, so the browser can let it go too
globalThis.fm1App = {
  process(fm1) {
    if (fm1.quiet) fm1.lastFrame = currentTime;   // (worklet.js publishes a frame 1/30 s after the last)
  },
  message(fm1, m) {
    if (m.op === "quiet") fm1.quiet = true;
    if (m.op === "end") {
      fm1.process = () => false;
      fm1.ex = fm1.mem = null;
      return;
    }
    const ex = fm1.ex;
    if (!ex) return;
    if (m.op === "scene" && ex.web_scene_buf) {
      const b = Uint8Array.from(m.bytes || []).subarray(0, ex.web_scene_size());
      if (b.length !== ex.web_scene_size()) return;
      new Uint8Array(fm1.mem.buffer, ex.web_scene_buf(), b.length).set(b);
      ex.web_scene_play();
    } else if (m.op === "stop" && ex.web_scene_stop) {
      ex.web_scene_stop();
    }
  },
};
