// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The River's ▶ in Node: FRASIL's engine in WebAssembly (app/build/emu/app.wasm, ./build.sh --web) plays a shard of
// the example backup (web_scene_play, app/web.c) as the worklet would (app_worklet.js): sound comes; web_scene_stop:
// it fades to silence; a shard that plays nothing sounds a chord on its ROOT. And what its card follows (web_live):
// GLACIER's drones and GLINT's sequence stepping on, AURORA's bass and its sequence, its LFO 1 moving, NORTH RIVER's
// three LFOs; stopped, every voice let go, no step. THE KEYS, byte by byte in the scene_t the editor saves
// (web_scene_take, web_latch, web_clear, web_param on the sequences' steps, web_hold): chords latched and let go (eight
// at most), the bass held, the arpeggio's notes, GLINT's and the bass's steps, CLEAR; ▶'s chord on the ROOT never kept.
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
check(size === ST.size && slot(7).length === size, `a scene_t: ${size} bytes, as the schema has it (the record's ${ST.rec}, the bass's ${SB.scene})`);
rms(40);                                                   // (boot settles)
const quiet = rms(20);
const play = (bytes) => { new Uint8Array(ex.memory.buffer, ex.web_scene_buf(), size).set(bytes); ex.web_scene_play(); };
play(slot(7));
rms(80);                                                   // (ABYSS's attack is slow: about 2 s in)
const abyss = rms(40);
check(abyss > quiet * 4 && abyss > 1e-3, `ABYSS sounds (rms ${abyss.toExponential(2)}, before ${quiet.toExponential(2)})`);
ex.web_scene_stop();
rms(1400);                                                 // (its release and the cloud's tail: about 30 s)
const after = rms(40);
check(after < abyss / 10, `stopped: it fades (rms ${after.toExponential(2)})`);
play(FACTORY[7]);                                          // slot 8 as the factory has it, NEW SHARD: nothing to play
rms(80);
const chord = rms(40);
check(chord > after * 4 && chord > 1e-3, `a shard with nothing to play: a chord on its ROOT (rms ${chord.toExponential(2)})`);

// ---------------------------------------------------------------- what sounds, for the card (web_live), a fresh engine
const fresh = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: stub, env });
const lx = fresh.instance.exports;
if (lx._initialize) lx._initialize();
lx.web_master(2800);
lx.web_boot();
const lrun = (blocks) => { for (let b = 0; b < blocks; b++) lx.web_render(1024); };
const heard = () => Array.from(new Float32Array(lx.memory.buffer, lx.web_live(), lx.web_live_size()));
const lplay = (bytes) => { new Uint8Array(lx.memory.buffer, lx.web_scene_buf(), size).set(bytes); lx.web_scene_play(); };
check(lx.web_live_size() === 52, "web_live: 52 floats");
lrun(40);
lplay(slot(9));                                            // GLACIER: G2 B2 D3, GLINT on SEQ, no LFO
lrun(80);
let v = heard();
const drones = v.slice(0, 8).filter((n, i) => n >= 0 && v[8 + i] > 0.5).sort((a, b) => a - b);
check(drones.join(" ") === "43 47 50", `GLACIER: its drones sound, G2 B2 D3 (${drones.join(" ")})`);
check(v.slice(26, 29).every((x) => x === 0), "GLACIER: no LFO on");
const seen = new Set();
for (let i = 0; i < 12; i++) { lrun(6); seen.add(heard()[38]); }
check(!seen.has(-1) && seen.size >= 3, `GLACIER: GLINT's sequence steps on (${[...seen].join(" ")})`);
v = heard();
const young = [0, 1, 2, 3].filter((i) => v[16 + i] >= 0).sort((a, b) => v[48 + a] - v[48 + b]);
check(v[47] > 0 && young.length && v[48 + young[0]] < 1, `GLACIER: GLINT's notes struck, counted (${v[47]}), the last ${v[48 + young[0]].toFixed(2)} s ago`);
lx.web_scene_stop();
lrun(1400);
v = heard();
check(v.slice(8, 16).every((l) => l < 0.05) && v[38] === -1 && v[40] === -1, "stopped: every voice let go (its release nearly done), no step");
lplay(slot(10));                                           // AURORA: its bass's sequence, LFO 1 on TIMBRE
lrun(80);
v = heard();
check(v[24] >= 24 && v[25] > 0.3 && v[40] >= 0, `AURORA: its bass sounds (note ${v[24]}, level ${v[25].toFixed(2)}), its sequence on step ${v[40]}`);
check(v[26] === 1 && v[27] === 0 && v[28] === 0, "AURORA: LFO 1 on, LFO 2 and 3 off");
const ph = v[29];
lrun(20);
check(heard()[29] !== ph && Math.abs(heard()[32]) <= 1, "AURORA: LFO 1 moves along its turn");
lx.web_scene_stop();
lrun(1400);
lplay(FACTORY[0]);                                         // NORTH RIVER: its three LFOs
lrun(80);
v = heard();
check(v[26] === 1 && v[27] === 1 && v[28] === 1, "NORTH RIVER: its three LFOs on");

// ---------------------------------------------------------------- THE KEYS: what a shard plays, edited (app/web.c)
const mi = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: stub, env });
const mx = mi.instance.exports;                            // (the editor's own engine, silent: river-edit.js's)
if (mx._initialize) mx._initialize();
mx.web_master(0);
mx.web_boot();
const ID = SCHEMA.ids, SEQ = SCHEMA.seq, BSEQ = SCHEMA.bseq;
const take = (x, bytes) => { new Uint8Array(x.memory.buffer, x.web_scene_buf(), size).set(bytes); x.web_scene_take(); };
const edited = (x) => Uint8Array.from(new Uint8Array(x.memory.buffer, x.web_scene_edited(), size));
const diffs = (a, b) => [...a].map((x, i) => (x !== b[i] ? i : -1)).filter((i) => i >= 0);
const chordsOf = (b) => Array.from({ length: b[ST.nch] }, (_, i) => (b[ST.ch + 5 * i] << 24) >> 24);
const s8 = (x) => (x << 24) >> 24;
take(mx, FACTORY[7]);                                      // NEW SHARD: nothing to play, and no chord held for it
check(diffs(edited(mx), FACTORY[7]).length === 0, "web_scene_take: a shard that plays nothing, taken: as it was, no chord on its ROOT");
take(mx, slot(9));                                         // GLACIER
const g0 = slot(9), gch = chordsOf(g0);
let r = mx.web_latch(0, 62);
let e = edited(mx);
check(r === 1 && e[ST.nch] === g0[ST.nch] + 1 && chordsOf(e).some((x) => !gch.includes(x) && x <= 62 && x >= 59),
      `web_latch, the drones: D4 latches HARMONY's chord on its root (chords ${chordsOf(e).join(" ")})`);
r = mx.web_latch(0, 62);
check(r === 0 && diffs(edited(mx), g0).length === 0, "...again: let go, GLACIER as it was, byte for byte");
let n8 = 0;
for (const k of [36, 41, 48, 53, 60, 65, 72, 77, 84, 89]) if (mx.web_latch(0, k) === 1) n8++;
e = edited(mx);
check(e[ST.nch] === 8 && n8 === 8 - g0[ST.nch], `eight chords at most (${e[ST.nch]}; the rest refused)`);
mx.web_clear(0);
check(edited(mx)[ST.nch] === 0, "web_clear(0): every chord let go");
take(mx, slot(9));
check(edited(mx)[ST.bass] === 255, "GLACIER: no bass held");
r = mx.web_latch(2, 45);
check(r === 1 && edited(mx)[ST.bass] === 45, "web_latch, the bass: A2 held, kept as it sounds (45)");
check(mx.web_latch(2, 47) === 1 && edited(mx)[ST.bass] === 47, "...another note: it goes there (B2)");
check(mx.web_latch(2, 47) === 0 && edited(mx)[ST.bass] === 255, "...again: let go");
mx.web_latch(2, 45);
const held45 = edited(mx), bo = held45[ST.v + ID.P_BS_OCTAVE];
take(mx, held45);                                          // (a shard holding A2: its BASS OCTAVE turned, that alone)
mx.web_param(ID.P_BS_OCTAVE, bo < 2 ? bo + 1 : bo - 1);
check(edited(mx)[ST.bass] === 45 + (bo < 2 ? 12 : -12), `BASS OCTAVE turned, the bass held: kept as it sounds now (${edited(mx)[ST.bass]})`);
take(mx, slot(9));
const oct = (b) => b[ST.v + ID.P_GL_OCTAVE];
const sq = (b, i) => b[ST.seq + i];
mx.web_param(ID.P_SQ_STEP, 5);
mx.web_param(ID.P_SQ_NOTE, 60);
e = edited(mx);
check((sq(e, 5) & 127) === 60 && diffs(e, g0).every((i) => i === ST.seq + 5), `GLINT's sequence: step 6 written through STEP and NOTE (C4; only that byte)`);
mx.web_param(ID.P_SQ_ACC, 1);
mx.web_param(ID.P_SQ_STEP, 6);
mx.web_param(ID.P_SQ_NOTE, SEQ.tie);
e = edited(mx);
check(sq(e, 5) === (60 | SEQ.accent) && sq(e, 6) === SEQ.tie, "...its ACCENT, and a TIE on step 7");
mx.web_param(ID.P_SQ_CLEAR, 2);
e = edited(mx);
check([...e.subarray(ST.seq, ST.seq + 32)].every((x) => x === SEQ.rest), "...CLEAR: every step a REST");
take(mx, slot(10));                                        // AURORA: its bass's sequence
const a0 = slot(10);
mx.web_param(ID.P_BQ_STEP, 3);
mx.web_param(ID.P_BQ_NOTE, 40);
mx.web_param(ID.P_BQ_LEN, 4);
mx.web_param(ID.P_BQ_STEPS, 7);
mx.web_param(ID.P_BQ_TRANS, 16);
e = edited(mx);
check(e[ST.bnote + 3] === 40 && e[ST.blen + 3] === 4 && e[ST.bq + 1] === 7 && e[ST.bq + 2] === 16 &&
      diffs(e, a0).every((i) => [ST.bnote + 3, ST.blen + 3, ST.bq + 1, ST.bq + 2].includes(i)),
      "the bass's sequence: step 4's NOTE and STEP LENGTH, SEQ STEPS, TRANSPOSE (only those bytes)");
mx.web_param(ID.P_BQ_STEP, 4);
mx.web_param(ID.P_BQ_NOTE, BSEQ.hold);
mx.web_param(ID.P_BQ_ON, 0);
e = edited(mx);
check(e[ST.bnote + 4] === BSEQ.hold && e[ST.bq] === 0, "...a HOLD on step 5, SEQUENCE off");
mx.web_param(ID.P_BQ_CLEAR, 2);
e = edited(mx);
check([...e.subarray(ST.bnote, ST.bnote + 32)].every((x) => x === 0), "...CLEAR: every step a CUT of a sixteenth");
take(mx, FACTORY[0]);                                      // NORTH RIVER: GLINT's arpeggio
const n0 = FACTORY[0], arp0 = [...n0.subarray(ST.arp, ST.arp + n0[ST.narp])], top = arp0[arp0.length - 1] + 12 * oct(n0);
r = mx.web_latch(1, top);
e = edited(mx);
check(r === 0 && e[ST.narp] === arp0.length - 1, `web_latch, GLINT: its highest note (${top}, as it sounds) leaves the arpeggio (${[...e.subarray(ST.arp, ST.arp + e[ST.narp])].join(" ")})`);
r = mx.web_latch(1, top);
e = edited(mx);
check(r === 1 && [...e.subarray(ST.arp, ST.arp + e[ST.narp])].join() === arp0.join(), "...again: back in, its notes as they were");
mx.web_clear(1);
check(edited(mx)[ST.narp] === 0, "web_clear(1): the arpeggio's notes gone");
take(mx, slot(7));
mx.web_param(21, 30);                                      // (FX's DECAY: a setting alone leaves what it plays as it was)
check(diffs(edited(mx), slot(7)).every((i) => i === ST.v + 21), "a setting turned: only its byte");
lx.web_scene_stop();                                       // ▶ (the engine playing): its chord on the ROOT, never kept
lrun(400);
lplay(FACTORY[7]);
lrun(40);
const before = heard().slice(0, 8).filter((n, i) => n >= 0 && heard()[8 + i] > 0.3).length;
check(lx.web_latch(0, 64) === 1 && edited(lx)[ST.nch] === 1, `▶ on a shard that plays nothing (${before} voices on its ROOT), a chord latched: that one alone kept`);
lrun(200);
const now = heard(), on = now.slice(0, 8).filter((n, i) => n >= 0 && now[8 + i] > 0.3);
check(on.length > 0 && on.every((n) => n >= 60 && n <= 80), `...the ROOT's chord let go, the new one sounds (${on.join(" ")})`);
lx.web_scene_stop();
lrun(400);
lplay(slot(9));
lrun(60);
const was = edited(lx);
lx.web_hold(0, 84, 1);
lrun(30);
const high = () => { const h2 = heard(); return h2.slice(0, 8).map((n, i) => (n >= 80 ? h2[8 + i] : 0)).filter((l) => l > 0); };
const held = high();
lx.web_hold(0, 84, 0);
lrun(200);                                                 // (GLACIER's drones let go slowly: 4.6 s on, still fading)
const after2 = high();
check(held.length === 3 && after2.length === 3 && after2.every((l) => l < held[0] * 0.65) && diffs(edited(lx), was).length === 0,
      `web_hold: C6's chord while down (${held.length} voices at ${held[0].toFixed(2)}), let go once up (${after2[0].toFixed(2)} now), nothing kept`);
console.log(fails ? `river_audition: ${fails} FAILED` : "river_audition: all ok");
process.exit(fails ? 1 : 0);
