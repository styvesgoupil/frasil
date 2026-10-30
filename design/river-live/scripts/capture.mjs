// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The live view's mockups: what FRASIL's engine plays (app/build/emu/app.wasm, ./build.sh --web; web_live, app/web.c),
// 30 frames a second, while shards of the example backup play, as the River's ▶ plays them (a fresh engine each).
//   node design/river-live/scripts/capture.mjs '[["NAME", SLOT (or -N: factory scene N), SECONDS], ...]' OUT.json
import fs from "fs";
import os from "os";
import path from "path";
import { execFileSync } from "child_process";
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "cap-"));
fs.writeFileSync(`${tmp}/s.txt`, `schema ${tmp}/schema.json\n`);
execFileSync("app/build/host/frasil_host", [`${tmp}/s.txt`, tmp]);
const SCHEMA = JSON.parse(fs.readFileSync(`${tmp}/schema.json`, "utf8"));
const wasm = fs.readFileSync("app/build/emu/app.wasm");
const img = fs.readFileSync("build/website/river/example.frasil");
const objs = {};
for (let k = 0, pos = 16; k < (img[6] | img[7] << 8); k++) {
  const obj = img[pos] | img[pos + 1] << 8, len = img[pos + 2] | img[pos + 3] << 8;
  objs[obj] = img.subarray(pos + 8, pos + 8 + len);
  pos += 8 + len;
}
const ST = SCHEMA.scene_t, SS = SCHEMA.scenes_t, SB = SCHEMA.bseqs_t;
const slot = (s) => Uint8Array.from([...objs[SCHEMA.objs.scenes].subarray(SS.s + s * ST.rec, SS.s + (s + 1) * ST.rec),
                                     ...objs[SCHEMA.objs.bseqs].subarray(SB.s + s * SB.scene, SB.s + (s + 1) * SB.scene)]);
const out = {};
for (const [name, s, secs] of JSON.parse(process.argv[2])) {
  const stub = new Proxy({ clock_time_get: () => 0, fd_write: () => 0, proc_exit: () => {} }, { get: (t, k) => (k in t ? t[k] : () => 0) });
  const env = new Proxy({}, { get: () => () => 0 });
  const { instance } = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: stub, env });
  const ex = instance.exports;
  if (ex._initialize) ex._initialize();
  ex.web_master(2800);
  ex.web_boot();
  const size = ex.web_scene_size();
  new Uint8Array(ex.memory.buffer, ex.web_scene_buf(), size).set(s < 0 ? Uint8Array.from(Buffer.from(SCHEMA.factory[-s - 1], "hex")) : slot(s));
  ex.web_scene_play();
  const frames = [];
  for (let f = 0; f < secs * 30; f++) {
    for (let k = 0; k < 1470; k += 294) ex.web_render(294);
    frames.push(Array.from(new Float32Array(ex.memory.buffer, ex.web_live(), ex.web_live_size())).map((v) => Math.round(v * 1000) / 1000));
  }
  out[name] = frames;
}
fs.writeFileSync(process.argv[3], JSON.stringify(out));
console.log("ok", Object.keys(out).map((k) => k + ":" + out[k].length).join(" "));
