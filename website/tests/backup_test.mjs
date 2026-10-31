// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The backup page's model (assets/js/backup-model.js) on the page as assembled (build/website/frasil/backup/: fm1-base's
// protocol and the schema inline), against FRASIL's own backups from the simulator (website/tests/backup_test.sh):
//   node website/tests/backup_test.mjs make DIR    device.frasil read; slot 15's scene put into slot 3 (through a
//                                                  scene package) and, with a bass's sequence and DECAY, SUSTAIN,
//                                                  PULSE of its own, into slot 7 (patched.frasil); packages of the
//                                                  earlier layouts (FRSi, FRSh) read; its backup as made before the
//                                                  bass's sequence (two objects, FRSh and FRSG: old10.frasil) and the
//                                                  patched one as made before DECAY, SUSTAIN and PULSE (FRSh, FRSH and
//                                                  OBJ_BSEQ: old11.frasil) and before GLINT's RELEASE (FRSj, FRSI,
//                                                  SCATTER where it is: old12.frasil) and 2.5's, before SOFT (FRSL,
//                                                  FRSq: old15.frasil), read; never patched
//   node website/tests/backup_test.mjs check DIR   after.frasil (the simulator's backup once patched.frasil was
//                                                  restored): slots 3 and 7 hold those bytes, the rest as it was;
//                                                  after10r.frasil, after11r.frasil, after12r.frasil: those restored,
//                                                  made over
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), "../..");
const [mode, dir] = process.argv.slice(2);
const page = fs.readFileSync(process.env.BACKUP_PAGE || path.join(root, "build/website/river/index.html"), "utf8");
const inline = [...page.matchAll(/<script>\n([\s\S]*?)<\/script>/g)].map((m) => m[1]).find((s) => s.includes("backup protocol: begin"));
if (!inline) { console.log("FAIL no backup protocol in the assembled page (tools/website.sh first)"); process.exit(1); }
const model = fs.readFileSync(path.join(root, "website/assets/js/backup-model.js"), "utf8");
const { PB, FB, FRASIL_VERSION } = new Function(`${inline}\n${model}\nreturn { PB, FB, FRASIL_VERSION };`)();

let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "  ok" : "FAIL"} ${msg}`); if (!ok) fails++; };
const load = (f) => { const image = new Uint8Array(fs.readFileSync(path.join(dir, f))); return { image, ...FB.read(PB.parse(image)) }; };
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
const S = FB.S, ST = S.scene_t, O = S.objs, objsOf = (image) => PB.parse(image).objs.map((o) => o.obj).join();
/* an image of exactly these objects (FB.build makes every one this FRASIL backs up) */
const raw = (objs) => {
  const le = (v, n) => Array.from({ length: n }, (_, i) => (v >>> (8 * i)) & 255);
  const head = [...PB.MAGIC, ...le(PB.VERSION, 2), ...le(objs.length, 2), ...le(0, 4)];
  const out = [...head, ...le(PB.crc32(Uint8Array.from(head)), 4)];
  for (const { obj, data } of objs) out.push(...le(obj, 2), ...le(data.length, 2), ...le(PB.crc32(data), 4), ...data);
  return Uint8Array.from([...out, ...le(PB.crc32(Uint8Array.from(out)), 4)]);
};
const emptyBass = (b) => !b.on && b.notes.every((x) => !x) && b.lens.every((x) => !x) &&
  b.steps === S.params[S.ids.P_BQ_STEPS].def && b.trans === S.params[S.ids.P_BQ_TRANS].def;
/* the earlier layouts, from this one's bytes: a scene_t as an earlier shard kept it (its nv sounds, then the rest to its
 * size: GLINT's sequence in its record, its bass's or some of it, or none), one made over (the sound's new settings at
 * their defaults; its bass, its TEMPO kept or at its default, or an empty bass; GLINT's RELEASE where SCATTER was) */
const N = S.sizes, SG = S.settings_t, SS = S.scenes_t, SB = S.bseqs_t, OS = Object.fromEntries(S.old_shards.map((x) => [x.magic, x]));
const OV = Object.fromEntries(S.old_settings_over.map((x) => [x.magic, x])), ONV = OS.FRSi.nv, ADD = N.NSOUND - ONV;
const OB = Object.fromEntries(S.old_bseqs.map((x) => [x.magic, x])), BN = ST.bnote;
const EMPTY_BQ = new Uint8Array(ST.size - BN);             // (an empty bass's sequence, from bnote on)
EMPTY_BQ[ST.bq - BN + 1] = S.params[S.ids.P_BQ_STEPS].def;
EMPTY_BQ[ST.bq - BN + 2] = S.params[S.ids.P_BQ_TRANS].def;
const TEMPO_DEF = S.params[S.ids.P_TEMPO].def;
EMPTY_BQ[ST.tempo - BN] = TEMPO_DEF;
const asOld = (b, nv, size) => Uint8Array.from([...b.subarray(0, ST.v + nv), ...b.subarray(ST.v + N.NSOUND, ST.v + N.NSOUND + size - ST.v - nv)]);
const noTempo = (b) => asOld(b, OS.FRSm.nv, OS.FRSm.size);   // (a scene_t as FRSm and FRSk kept it)
const frsi = (b) => asOld(b, ONV, OS.FRSi.size);
const frsh = (b) => asOld(b, ONV, OS.FRSh.size);
/* GLINT's RELEASE where every earlier layout kept SCATTER: as its DECAY let a glint go then (the schema's table) */
const GO = S.scatter_gone, gone = (b, v) => { const o = Uint8Array.from(b); o[v + GO.p] = GO.of[o[v + GO.decay]]; return o; };
const madeOver = (b, bass, nv = ONV, scatter = true, tempo = TEMPO_DEF) => {
  const o = Uint8Array.from([...b.subarray(0, ST.v + nv), ...S.params.slice(nv, N.NSOUND).map((p) => p.def),
                             ...b.subarray(ST.v + N.NSOUND, BN), ...(bass ? [...b.subarray(BN, ST.size - 1), tempo] : EMPTY_BQ)]);
  return scatter ? gone(o, ST.v) : o;
};
/* a backup's records as an earlier FRASIL kept them: its settings (OV: FRSJ, FRSI, FRSH, FRSG; a CC learned after the
 * sound numbered so), its scenes' record (each scene's nv sounds, the rest to rec: GLINT's sequence in it), its basses'
 * (OB: FRBT, FRBS, from the bass's notes on) */
const setDown = (p, magic) => {
  const l = OV[magic], add = N.NSOUND - l.nv, b = new Uint8Array(l.size);
  [...magic].forEach((c, i) => { b[i] = c.charCodeAt(0); });
  b.set(p.subarray(SG.v, SG.v + l.nv), 4);
  b.set(p.subarray(SG.octave, SG.octave + l.end - 4 - l.nv), 4 + l.nv);
  for (let i = 0; i < N.NLEARN; i++) { const at = SG.learn_p - add + i; if (b[at] >= N.NSOUND) b[at] -= add; }
  return b;
};
const fullOf = (p, q, s) => Uint8Array.from([...p.subarray(SS.s + s * ST.rec, SS.s + (s + 1) * ST.rec),
                                             ...q.subarray(SB.s + s * SB.scene, SB.s + (s + 1) * SB.scene)]);
const scenesDown = (p, q, magic, nv, rec) => {
  const b = new Uint8Array(8 + N.NSCENES * rec);
  b.set(p.subarray(0, 8));
  [...magic].forEach((c, i) => { b[i] = c.charCodeAt(0); });
  for (let s = 0; s < N.NSCENES; s++) b.set(asOld(fullOf(p, q, s), nv, rec), 8 + s * rec);
  return b;
};
const bseqDown = (q, magic) => {
  const o = OB[magic], b = new Uint8Array(SB.s + N.NSCENES * o.scene);
  [...magic].forEach((c, i) => { b[i] = c.charCodeAt(0); });
  for (let s = 0; s < N.NSCENES; s++) b.set(q.subarray(SB.s + s * SB.scene + o.at, SB.s + s * SB.scene + o.at + o.scene), SB.s + s * o.scene);
  return b;
};

const dev = load("device.frasil");
check(dev.current && dev.layout === FB.S.magic.scenes, `the scenes in this layout (${dev.layout})`);
check(dev.scenes.length === 16, "16 slots");
const src = dev.scenes[14];
check(src.state === "saved" && src.chords.length === 2, `slot 15 saved, two chords latched (${src.name}: ${src.chords.map((c) => c.root)})`);
check(dev.scenes[0].state === "factory" && dev.scenes[0].name === "NORTH RIVER" && dev.scenes[0].tempo === 60, "slot 1 the factory's NORTH RIVER, at 90 BPM");
check(dev.scenes[9].state === "empty" && dev.scenes[9].name === "SCENE 10", "slot 10 empty, SCENE 10");
check(dev.settings && dev.settings.current && dev.settings.scene === 14, "the settings: slot 15 last");
check(objsOf(dev.image) === [O.settings, O.scenes, O.bseqs].join() && emptyBass(src.bseq) && src.bytes.length === ST.size,
      `three objects; slot 15's scene_t ${src.bytes.length} bytes, its bass empty`);
/* slot 15's scene with a bass's sequence of its own: a NOTE 1/4, a HOLD 1/8, a CUT 1/16, a NOTE 1/2; STEPS 4, TRANSPOSE +2 */
const bassy = Uint8Array.from(src.bytes);
bassy.set([50, S.bseq.hold, S.bseq.cut, 57], ST.bnote);
bassy.set([2, 1, 0, 4], ST.blen);
bassy.set([1, 3, S.params[S.ids.P_BQ_TRANS].def + 2], ST.bq);
bassy.set([20, 40, 3], ST.v + ONV);                      // DECAY 20, SUSTAIN 40, PULSE 1/2 BEAT
check([S.ids.P_BS_DECAY, S.ids.P_BS_SUSTAIN, S.ids.P_BS_PULSE].join() === [ONV, ONV + 1, ONV + 2].join() &&
      S.pages.find((pg) => pg.name === "BASS ADSR").p.join() === [S.ids.P_BS_ATTACK, ONV, ONV + 1, S.ids.P_BS_RELEASE].join() &&
      S.pages.find((pg) => pg.name === "BASS II").p[3] === S.ids.P_BS_PULSE,
      "DECAY, SUSTAIN and PULSE after the sound's earlier settings; BASS ADSR in ADSR's order, PULSE on BASS II's K4");
check(S.pages.find((pg) => pg.name === "GLINT ADSR").p.join() === [S.ids.P_GL_ATTACK, S.ids.P_GL_DECAY, S.ids.P_GL_SUSTAIN, S.ids.P_GL_RELEASE].join() &&
      GO.p === S.ids.P_GL_RELEASE && GO.decay === S.ids.P_GL_DECAY && S.params[GO.p].name === "RELEASE" &&
      !S.params.some((p) => p.name === "SCATTER") && GO.of[70] === S.params[GO.p].def,
      "GLINT ADSR in ADSR's order, RELEASE where SCATTER was (DECAY's default letting a glint go in RELEASE's default)");
const p31 = FB.S.params[31];
check(p31.name === "TIMBRE" && Array.isArray(p31.texts) && p31.texts.length === p31.max + 1, "TIMBRE's words, every value");

if (mode === "make") {
  const pack = FB.readPack(FB.packOf([src, dev.scenes[0]], FRASIL_VERSION));
  check(pack.scenes.length === 2 && same(pack.scenes[0].bytes, src.bytes) && pack.scenes[0].slot === 14,
        "a scene package: the same bytes, its slot");
  let bad = false;
  try { FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: "FRSf", scenes: [] })); } catch (_) { bad = true; }
  check(bad, "a package in another layout: refused");
  const b64 = (b) => Buffer.from(b).toString("base64");
  for (const [was, data, bass] of [["FRSi", frsi(bassy), true], ["FRSh", frsh(bassy), false]]) {
    const old = FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: was, scenes: [{ name: src.name, from: 15, data: b64(data) }] }));
    const sc = old.scenes[0], v = sc.raw_v;
    check(data.length === OS[was].size && same(sc.bytes, madeOver(bassy, bass)) && sc.slot === 14 && (bass ? sc.bseq.on && sc.bseq.steps === 3 : emptyBass(sc.bseq)) &&
          v[ONV] === S.params[ONV].def && v[ONV + 1] === 127 && v[ONV + 2] === 0,
          `a package made before (${was}, ${data.length} bytes): brought up to date, DECAY, SUSTAIN 100 % and PULSE OFF at their defaults, ${bass ? "its bass's sequence kept" : "an empty bass"}`);
    bad = false;
    try { FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: was, scenes: [{ name: src.name, data: b64(bassy) }] })); } catch (_) { bad = true; }
    check(bad, `... a ${ST.size}-byte scene in it: damaged`);
  }
  {                                                      // a package made before GLINT's RELEASE (FRSk): SCATTER 99 there
    const bk = Uint8Array.from(bassy);
    bk[ST.v + GO.p] = 99;
    bk[ST.v + GO.decay] = 127;
    const frsk = noTempo(bk);
    const sc = FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: "FRSk", scenes: [{ name: src.name, from: 15, data: b64(frsk) }] })).scenes[0];
    check(frsk.length === OS.FRSk.size && same(sc.bytes, madeOver(bk, true, OS.FRSk.nv)) && sc.raw_v[GO.p] === GO.of[127] && GO.of[127] === 112 &&
          sc.bseq.on && sc.tempo === TEMPO_DEF,
          `a package made before GLINT's RELEASE (FRSk): SCATTER 99 made over, RELEASE as DECAY 127 let it go (${GO.of[127]}), TEMPO at its default, the rest as it was`);
  }
  {                                                      // ... and before the scenes' TEMPO (FRSm): RELEASE 33 kept
    const bm = Uint8Array.from(bassy);
    bm[ST.v + GO.p] = 33;
    const frsm = noTempo(bm);
    const sc = FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: "FRSm", scenes: [{ name: src.name, from: 15, data: b64(frsm) }] })).scenes[0];
    check(frsm.length === OS.FRSm.size && !OS.FRSm.scatter && same(sc.bytes, madeOver(bm, true, OS.FRSm.nv, false)) && sc.raw_v[GO.p] === 33,
          "a package made before the scenes' TEMPO (FRSm): its TEMPO at the default, its RELEASE as it was, the rest too");
  }
  {                                                      // ... and before MASTER EQ, MASTER FILTER and LOFI TAPE (FRSn)
    const frsn = asOld(bassy, OS.FRSn.nv, OS.FRSn.size);
    const sc = FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: "FRSn", scenes: [{ name: src.name, from: 15, data: b64(frsn) }] })).scenes[0];
    check(frsn.length === OS.FRSn.size && !OS.FRSn.scatter && same(sc.bytes, madeOver(bassy, true, OS.FRSn.nv, false, bassy[ST.tempo])) &&
          sc.raw_v[S.ids.P_EQ_PRESET] === 0 && sc.raw_v[S.ids.P_FL_TYPE] === 0 && sc.raw_v[S.ids.P_TP_ON] === 0 && sc.bseq.on && sc.tempo === bassy[ST.tempo],
          "a package made before MASTER EQ, MASTER FILTER and LOFI TAPE (FRSn): those off, at their defaults, its TEMPO and the rest as they were");
  }
  {                                                      // ... and in 2.5 (FRSr), before CHOIR III's SOFT: SOFT 0
    const bs = Uint8Array.from(bassy);
    bs[ST.v + S.ids.P_CH_SOFT] = 64;
    const frsr = asOld(bs, OS.FRSr.nv, OS.FRSr.size);
    const sc = FB.readPack(JSON.stringify({ format: "frasil-scenes", version: 1, layout: "FRSr", scenes: [{ name: src.name, from: 15, data: b64(frsr) }] })).scenes[0];
    check(frsr.length === OS.FRSr.size && OS.FRSr.nv === S.ids.P_CH_SOFT && S.ids.P_CH_SOFT === N.NSOUND - 1 && !OS.FRSr.scatter &&
          same(sc.bytes, madeOver(bs, true, OS.FRSr.nv, false, bs[ST.tempo])) && sc.raw_v[S.ids.P_CH_SOFT] === 0 &&
          S.params[S.ids.P_CH_SOFT].def === 0 && sc.bseq.on && sc.tempo === bs[ST.tempo],
          "a package made in 2.5 (FRSr), before CHOIR III's SOFT: SOFT 0 (the wave as it was), its TEMPO and the rest as they were");
  }
  const out = FB.patch(dev.image, [{ slot: 2, bytes: pack.scenes[0].bytes }, { slot: 6, bytes: bassy }]);
  const bq = FB.read(PB.parse(out)).scenes[6].bseq;
  check(objsOf(out) === [O.settings, O.scenes, O.bseqs].join(), "the patched image checks whole (PB.parse), its objects in order");
  check(bq.on && bq.steps === 3 && bq.notes.slice(0, 4).join() === `50,${S.bseq.hold},${S.bseq.cut},57` && bq.lens.slice(0, 4).join() === "2,1,0,4",
        "slot 7's bass's sequence: in OBJ_BSEQ, read back");
  fs.writeFileSync(path.join(dir, "patched.frasil"), out);
  // a backup from before the bass's sequence (1.1's test builds): its settings (FRSG) and its scenes' record (FRSh), no
  // OBJ_BSEQ
  const o = PB.parse(dev.image).objs, of = (objs, obj) => objs.find((x) => x.obj === obj).data;
  const old10 = raw([{ obj: O.settings, data: setDown(of(o, O.settings), "FRSG") },
                     { obj: O.scenes, data: scenesDown(of(o, O.scenes), of(o, O.bseqs), "FRSh", ONV, OS.FRSh.rec) }]);
  const r10 = FB.read(PB.parse(old10));
  check(!r10.current && r10.layout === "FRSh" && r10.scenes[14].state === "saved" && r10.scenes[14].name === src.name &&
        r10.scenes[14].chords.length === 2 && r10.scenes.every((x) => emptyBass(x.bseq) || x.state !== "saved"),
        "a backup without OBJ_BSEQ (two objects, FRSh's scenes): read as an earlier FRASIL's, the names and what plays, every bass empty");
  check(r10.settings.current && r10.settings.layout === "FRSG" && same(r10.settings.raw_v, dev.settings.raw_v) && r10.settings.scene === 14 &&
        emptyBass(r10.settings.bseq), "... its settings (FRSG) made over as this FRASIL's, the bass empty");
  bad = false;                                           // (an FM-1 that doesn't back OBJ_BSEQ up would refuse it)
  try { FB.patch(old10, [{ slot: 4, bytes: bassy }]); } catch (_) { bad = true; }
  check(bad, "an FM-1 whose backup has no OBJ_BSEQ: no shard put into it");
  fs.writeFileSync(path.join(dir, "old10.frasil"), old10);
  // the patched backup as made before DECAY, SUSTAIN and PULSE: FRSH (CC 22 learned on TEMPO, 129 then), FRSh, OBJ_BSEQ
  const po = PB.parse(out).objs, set11 = setDown(of(po, O.settings), "FRSH");
  set11[SG.learn_cc - ADD] = 22;
  set11[SG.learn_p - ADD] = S.ids.P_TEMPO - ADD;
  const old11 = raw([{ obj: O.settings, data: set11 }, { obj: O.scenes, data: scenesDown(of(po, O.scenes), of(po, O.bseqs), "FRSh", ONV, OS.FRSh.rec) },
                     { obj: O.bseqs, data: of(po, O.bseqs) }]);
  const r11 = FB.read(PB.parse(old11));
  check(!r11.current && r11.layout === "FRSh" && r11.scenes[2].name === src.name && r11.scenes[6].state === "saved" &&
        r11.scenes[6].bseq.on && r11.scenes[6].bseq.steps === 3 && r11.scenes[6].bseq.notes.slice(0, 4).join() === `50,${S.bseq.hold},${S.bseq.cut},57`,
        "a backup from before DECAY, SUSTAIN and PULSE (FRSh): an earlier FRASIL's scenes, slot 7's bass's sequence read beside them");
  check(r11.settings.current && r11.settings.layout === "FRSH" && same(r11.settings.raw_v, dev.settings.raw_v) &&
        r11.settings.learned.length === 1 && r11.settings.learned[0].cc === 22 && r11.settings.learned[0].p === S.ids.P_TEMPO,
        `... its settings (FRSH) made over: the sound's new settings at their defaults, CC 22 on TEMPO (${S.ids.P_TEMPO - ADD} -> ${S.ids.P_TEMPO})`);
  bad = false;
  try { FB.patch(old11, [{ slot: 4, bytes: bassy }]); } catch (_) { bad = true; }
  check(bad, "an FM-1 keeping FRSh's scenes: no shard put into it");
  fs.writeFileSync(path.join(dir, "old11.frasil"), old11);
  // the patched backup as made before GLINT's RELEASE: FRSI (SCATTER 99 where RELEASE is, CC 23 learned on it), FRSj
  // (each scene SCATTER 99 there), OBJ_BSEQ
  const set12 = setDown(of(po, O.settings), "FRSI"), at12 = N.NSOUND - OV.FRSI.nv;
  const sc12 = scenesDown(of(po, O.scenes), of(po, O.bseqs), "FRSj", OS.FRSn.nv, OS.FRSn.rec);
  set12[4 + GO.p] = 99;
  set12[SG.learn_cc - at12] = 23;
  set12[SG.learn_p - at12] = GO.p;
  for (let s = 0; s < N.NSCENES; s++) sc12[8 + s * OS.FRSn.rec + ST.v + GO.p] = 99;
  const old12 = raw([{ obj: O.settings, data: set12 }, { obj: O.scenes, data: sc12 }, { obj: O.bseqs, data: of(po, O.bseqs) }]);
  const r12 = FB.read(PB.parse(old12));
  check(!r12.current && r12.layout === "FRSj" && r12.scenes[6].state === "saved" && r12.scenes[6].bseq.on,
        "a backup from before GLINT's RELEASE (FRSj): an earlier FRASIL's scenes, read");
  check(r12.settings.current && r12.settings.layout === "FRSI" && same(r12.settings.raw_v, gone(dev.settings.raw_v, 0)) &&
        !r12.settings.learned.some((l) => l.cc === 23), "... its settings (FRSI) made over: RELEASE as DECAY let it go, CC 23 on SCATTER learned no more");
  fs.writeFileSync(path.join(dir, "old12.frasil"), old12);
  // the patched backup as an FM-1 keeps it just updated, before its basses' TEMPO (FRBS, a scene's bass without it):
  // the settings at 70 BPM (40); read, an earlier FRASIL's scenes, each at the settings' TEMPO; never patched
  const set13 = Uint8Array.from(of(po, O.settings));
  set13[SG.tempo] = 40;
  const old13 = raw([{ obj: O.settings, data: set13 }, { obj: O.scenes, data: scenesDown(of(po, O.scenes), of(po, O.bseqs), "FRSl", OS.FRSn.nv, OS.FRSn.rec) },
                     { obj: O.bseqs, data: bseqDown(of(po, O.bseqs), "FRBS") }]);
  const r13 = FB.read(PB.parse(old13));
  check(!r13.current && r13.layout === "FRSl" && OB.FRBS.scene === OB.FRBT.scene - 1 && r13.scenes[6].tempo === 40 && r13.scenes[6].bseq.on &&
        r13.scenes[6].bseq.steps === 3 && r13.scenes[2].tempo === 40,
        "a backup with its basses before the scenes' TEMPO (FRBS): an earlier FRASIL's, each scene at the settings' TEMPO, its bass kept");
  bad = false;
  try { FB.patch(old13, [{ slot: 4, bytes: bassy }]); } catch (_) { bad = true; }
  check(bad, "... an FM-1 keeping its basses so: no shard put into it");
  fs.writeFileSync(path.join(dir, "old13.frasil"), old13);
  // the patched backup as 2.1's first builds kept it, before MASTER EQ, MASTER FILTER and LOFI TAPE: FRSJ (CC 24 learned
  // on TEMPO, numbered then), FRSl (GLINT's sequence in each scene), FRBT; read, an earlier FRASIL's, each its TEMPO
  const set14 = setDown(of(po, O.settings), "FRSJ"), at14 = N.NSOUND - OV.FRSJ.nv;
  set14[SG.learn_cc - at14] = 24;
  set14[SG.learn_p - at14] = S.ids.P_TEMPO - at14;
  set14[SG.speaker_off - at14] = 3;                      // (MORPH's byte then, as an earlier FRASIL may have left it)
  const old14 = raw([{ obj: O.settings, data: set14 }, { obj: O.scenes, data: scenesDown(of(po, O.scenes), of(po, O.bseqs), "FRSl", OS.FRSn.nv, OS.FRSn.rec) },
                     { obj: O.bseqs, data: bseqDown(of(po, O.bseqs), "FRBT") }]);
  const r14 = FB.read(PB.parse(old14));
  check(!r14.current && r14.layout === "FRSl" && r14.scenes[6].state === "saved" && r14.scenes[6].bseq.on && r14.scenes[6].bseq.steps === 3 &&
        r14.scenes[6].tempo === bassy[ST.tempo] && r14.settings.current && r14.settings.layout === "FRSJ" &&
        r14.settings.learned.some((l) => l.cc === 24 && l.p === S.ids.P_TEMPO) && r14.settings.speaker,
        "a backup from before MASTER EQ, MASTER FILTER and LOFI TAPE (FRSJ, FRSl, FRBT): an earlier FRASIL's scenes, their basses and TEMPO, CC 24 on TEMPO, SPEAKER on");
  fs.writeFileSync(path.join(dir, "old14.frasil"), old14);
  // the patched backup as 2.5 keeps it, before CHOIR III's SOFT: FRSL (ANIMATIONS LOW, CLOCK INTERNAL, CC 25 learned on
  // TEMPO, numbered then), FRSq (each scene's sound without SOFT, GLINT's sequence in OBJ_BSEQ), FRBU as it is
  const set15 = setDown(of(po, O.settings), "FRSL"), at15 = N.NSOUND - OV.FRSL.nv;
  set15[SG.learn_cc - at15] = 25;
  set15[SG.learn_p - at15] = S.ids.P_TEMPO - at15;
  set15[SG.anim - at15] = 1;
  set15[SG.clock - at15] = 1;
  const l15 = S.old_scenes.find((x) => x.magic === "FRSq"), rec15 = l15.name + l15.nv + 2 + N.SC_CHORDS * 5 + N.SC_ARP + 1;
  const old15 = raw([{ obj: O.settings, data: set15 }, { obj: O.scenes, data: scenesDown(of(po, O.scenes), of(po, O.bseqs), "FRSq", l15.nv, rec15) },
                     { obj: O.bseqs, data: of(po, O.bseqs) }]);
  const r15 = FB.read(PB.parse(old15));
  check(!r15.current && r15.layout === "FRSq" && !l15.seq && l15.nv === OS.FRSr.nv && r15.scenes[6].state === "saved" &&
        r15.scenes[6].bseq.on && r15.scenes[6].bseq.steps === 3 && r15.scenes[6].tempo === bassy[ST.tempo] && r15.scenes[2].name === src.name,
        "a backup from 2.5, before CHOIR III's SOFT (FRSq, FRBU): an earlier FRASIL's scenes, their basses and TEMPO");
  check(r15.settings.current && r15.settings.layout === "FRSL" && same(r15.settings.raw_v, dev.settings.raw_v) && r15.settings.anim === 1 &&
        r15.settings.learned.some((l) => l.cc === 25 && l.p === S.ids.P_TEMPO),
        "... its settings (FRSL) made over: SOFT at its default, ANIMATIONS LOW kept, CC 25 on TEMPO");
  bad = false;
  try { FB.patch(old15, [{ slot: 4, bytes: bassy }]); } catch (_) { bad = true; }
  check(bad, "an FM-1 keeping 2.5's scenes: no shard put into it");
  fs.writeFileSync(path.join(dir, "old15.frasil"), old15);
  const parsed = PB.parse(dev.image);                    // (the FM-1's scenes record in another layout)
  const objs = parsed.objs.map((o) => ({ obj: o.obj, data: Uint8Array.from(o.data) }));
  objs.find((o) => o.obj === FB.S.objs.scenes).data[3] = "f".charCodeAt(0);
  bad = false;
  try { FB.patch(FB.build(objs), [{ slot: 2, bytes: src.bytes }]); } catch (_) { bad = true; }
  check(bad, "an FM-1 keeping another layout: not written over");
} else {
  const after = load("after.frasil");
  check(same(after.scenes[2].bytes, src.bytes) && same(after.scenes[6].bytes, bassy), "slots 3 and 7 hold slot 15's scene, 7 with its bass's sequence");
  check(after.scenes[2].state === "saved" && after.scenes[6].state === "saved", "and are saved");
  check(after.scenes.every((s, i) => i === 2 || i === 6 || s.state === dev.scenes[i].state &&
        (s.state !== "saved" || same(s.bytes, dev.scenes[i].bytes))), "the other slots as they were");
  check(same(after.settings.raw_v, dev.settings.raw_v) && after.settings.scene === dev.settings.scene, "the settings as they were");
  const a10 = load("after10r.frasil");                                           // (slot 15's bass: empty already)
  check(a10.current && same(a10.scenes[14].bytes, src.bytes) && a10.scenes.every((x) => x.state !== "saved" || emptyBass(x.bseq)) &&
        same(a10.settings.raw_v, dev.settings.raw_v), "the two-object backup restored, made over: slot 15, every bass empty, the settings");
  const a11 = load("after11r.frasil");
  check(a11.current && same(a11.scenes[2].bytes, src.bytes) && same(a11.scenes[6].bytes, madeOver(bassy, true)) &&
        a11.scenes.every((s, i) => i === 2 || i === 6 || s.state === dev.scenes[i].state && (s.state !== "saved" || same(s.bytes, dev.scenes[i].bytes))),
        "the FRSh backup restored, made over: slots 3 and 7 (its bass's sequence; DECAY, SUSTAIN and PULSE at their defaults), the others as they were");
  check(same(a11.settings.raw_v, dev.settings.raw_v) && a11.settings.learned.some((l) => l.cc === 22 && l.p === S.ids.P_TEMPO),
        "... its settings, CC 22 on TEMPO still");
  const a13 = load("after13r.frasil");
  check(a13.current && a13.scenes[6].tempo === 40 && a13.scenes[2].tempo === 40 && a13.scenes[6].bseq.on && a13.scenes[6].bseq.steps === 3 &&
        a13.settings.tempo === 40, "the FRBS backup restored, made over by the FM-1: each scene at the settings' TEMPO (70 BPM), its bass kept");
  const a14 = load("after14r.frasil");
  check(a14.current && same(a14.scenes[6].bytes, madeOver(bassy, true, OS.FRSn.nv, false, bassy[ST.tempo])) &&
        same(a14.scenes[2].bytes, madeOver(src.bytes, true, OS.FRSn.nv, false, src.bytes[ST.tempo])) &&
        same(a14.settings.raw_v, dev.settings.raw_v) && a14.settings.learned.some((l) => l.cc === 24 && l.p === S.ids.P_TEMPO) &&
        a14.settings.speaker,
        "the FRSJ / FRSl / FRBT backup restored, made over by the FM-1: MASTER EQ, MASTER FILTER and LOFI TAPE off, the rest as it was, CC 24 on TEMPO, SPEAKER on");
  const a15 = load("after15r.frasil");
  check(a15.current && same(a15.scenes[6].bytes, madeOver(bassy, true, OS.FRSr.nv, false, bassy[ST.tempo])) &&
        same(a15.scenes[2].bytes, madeOver(src.bytes, true, OS.FRSr.nv, false, src.bytes[ST.tempo])) &&
        same(a15.settings.raw_v, dev.settings.raw_v) && a15.settings.anim === 1 &&
        a15.settings.learned.some((l) => l.cc === 25 && l.p === S.ids.P_TEMPO),
        "the FRSL / FRSq backup restored, made over by the FM-1: SOFT 0, the rest as it was, ANIMATIONS LOW, CC 25 on TEMPO");
  const a12 = load("after12r.frasil");
  check(a12.current && same(a12.scenes[6].bytes, gone(bassy, ST.v)) && a12.scenes[6].raw_v[GO.p] !== 99 &&
        a12.scenes.every((s) => s.state !== "saved" || s.raw_v[GO.p] === GO.of[s.raw_v[GO.decay]]) &&
        same(a12.settings.raw_v, gone(dev.settings.raw_v, 0)) && !a12.settings.learned.some((l) => l.cc === 23),
        "the FRSj backup restored, made over by the FM-1: each scene's RELEASE as its DECAY let it go, the settings' too, CC 23 learned no more");
}
console.log(fails ? `backup_test ${mode}: ${fails} FAILED` : `backup_test ${mode}: all ok`);
process.exit(fails ? 1 : 0);
