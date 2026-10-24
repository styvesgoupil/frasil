// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The River's ▶ in Node: FRASIL's engine in WebAssembly (app/build/emu/app.wasm, ./build.sh --web) plays a shard of
// the example backup (web_scene_play, app/web.c) as the worklet would (app_worklet.js): sound comes; web_scene_stop:
// it fades to silence; a shard that plays nothing sounds a chord on its ROOT.
//   node website/tests/river_audition.mjs
import fs from "fs";
import os from "os";
import path from "path";
import { execFileSync } from "child_process";
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "audition-"));   // the schema's factory scenes (the simulator's)
fs.writeFileSync(`${tmp}/s.txt`, `schema ${tmp}/schema.json\n`);
execFileSync("app/build/host/frasil_host", [`${tmp}/s.txt`, tmp]);
const SCHEMA = JSON.parse(fs.readFileSync(`${tmp}/schema.json`, "utf8"));
const FACTORY = SCHEMA.factory.map((h) => Uint8Array.from(Buffer.from(h, "hex")));
const wasm = fs.readFileSync("app/build/emu/app.wasm");
const stub = new Proxy({ clock_time_get: () => 0, fd_write: () => 0, proc_exit: () => {} }, { get: (t, k) => (k in t ? t[k] : () => 0) });
const env = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: stub, env });
const ex = instance.exports;
if (ex._initialize) ex._initialize();
ex.web_master(2800);
ex.web_boot();
let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "  ok" : "FAIL"} ${msg}`); if (!ok) fails++; };
const rms = (blocks) => {
  let s = 0, n = 0;
  for (let b = 0; b < blocks; b++) {
    ex.web_render(1024);
    const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), 1024), r = new Float32Array(ex.memory.buffer, ex.web_out_r(), 1024);
    for (let i = 0; i < 1024; i++) { s += l[i] * l[i] + r[i] * r[i]; n += 2; }
  }
  return Math.sqrt(s / n);
};
const img = fs.readFileSync("build/website/river/example.frasil");
// a slot's scene_t: its bytes in the scenes' record (object 2), then its bass's sequence (object 3)
const objs = {};
for (let k = 0, pos = 16; k < (img[6] | img[7] << 8); k++) {
  const obj = img[pos] | img[pos + 1] << 8, len = img[pos + 2] | img[pos + 3] << 8;
  objs[obj] = img.subarray(pos + 8, pos + 8 + len);
  pos += 8 + len;
}
const ST = SCHEMA.scene_t, SS = SCHEMA.scenes_t, SB = SCHEMA.bseqs_t, size = ex.web_scene_size();
const slot = (s) => Uint8Array.from([...objs[SCHEMA.objs.scenes].subarray(SS.s + s * ST.rec, SS.s + (s + 1) * ST.rec),
                                     ...objs[SCHEMA.objs.bseqs].subarray(SB.s + s * SB.scene, SB.s + (s + 1) * SB.scene)]);
check(size === ST.size && slot(2).length === size, `a scene_t: ${size} bytes, as the schema has it (the record's ${ST.rec}, the bass's ${SB.scene})`);
rms(40);                                                   // (boot settles)
const quiet = rms(20);
const play = (bytes) => { new Uint8Array(ex.memory.buffer, ex.web_scene_buf(), size).set(bytes); ex.web_scene_play(); };
play(slot(2));
rms(80);                                                   // (ABYSS's attack is slow: about 2 s in)
const abyss = rms(40);
check(abyss > quiet * 4 && abyss > 1e-3, `ABYSS sounds (rms ${abyss.toExponential(2)}, before ${quiet.toExponential(2)})`);
ex.web_scene_stop();
rms(1400);                                                 // (its release and the cloud's tail: about 30 s)
const after = rms(40);
check(after < abyss / 10, `stopped: it fades (rms ${after.toExponential(2)})`);
play(FACTORY[0]);                                          // MIST, as the factory has it: nothing to play
rms(80);
const chord = rms(40);
check(chord > after * 4 && chord > 1e-3, `a shard with nothing to play: a chord on its ROOT (rms ${chord.toExponential(2)})`);
console.log(fails ? `river_audition: ${fails} FAILED` : "river_audition: all ok");
process.exit(fails ? 1 : 0);
