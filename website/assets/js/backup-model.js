/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The backup page's model: what a FRASIL backup holds, read with the schema the firmware's own code writes (the
 * simulator's "schema FILE": app.c schema_write; website/assemble.py puts it in the page as SCHEMA). No layout is
 * copied here by hand: the offsets, the sizes, the words for each value all come from it.
 *   FB.read(image)            a backup file (checked by PB.parse first) -> {flags, settings, scenes}
 *   FB.packOf(scenes, fw)     a scene package (.frasilscenes): JSON, each scene's bytes in base64
 *   FB.readPack(text)         -> {layout, firmware, scenes}
 *   FB.patch(image, places)   the device's fresh backup with scenes put into slots ([{slot, bytes}]) -> a new image
 * A scene: {slot, state ("saved", "factory", "empty"), current (in this FRASIL's layout), bytes (scene_t, current
 * only), name, v (its sound, the drone's preset applied, as the screen shows it), chords [{root, notes}], arp [notes],
 * bass (-1 none), seq [32 bytes] (current only), bseq {notes, lens, on, steps, trans} (the bass's sequence: empty for
 * an earlier FRASIL's without OBJ_BSEQ)}. Its scene_t is the scenes' record's (OBJ_SCENES) then its bass's (OBJ_BSEQ:
 * none, empty). A package or a shard of an earlier layout (S.old_shards) comes in made over, as the FM-1 would. */
"use strict";
const FB = ((S) => {
  S.params.forEach((p) => { if (typeof p.texts === "number") p.texts = S.texts[p.texts]; });   // (assemble.py shares them)
  const ST = S.scene_t, SS = S.scenes_t, SB = S.bseqs_t, SG = S.settings_t, N = S.sizes, ID = S.ids;
  const PACK = "frasil-scenes", REC = ST.rec, BQ = SB.scene;   // a scene's bytes in OBJ_SCENES, its bass's in OBJ_BSEQ
  const OBJS = [S.objs.settings, S.objs.scenes, S.objs.bseqs];   // a backup's, in order (app.h APP_BACKUP_OBJS)

  const u16 = (b, o) => b[o] | b[o + 1] << 8;
  const u32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
  const magicAt = (b, o) => String.fromCharCode(b[o], b[o + 1], b[o + 2], b[o + 3]);
  const s8 = (x) => (x > 127 ? x - 256 : x);
  const fromHex = (h) => Uint8Array.from(h.match(/../g), (x) => parseInt(x, 16));
  const text = (b, o, n) => {
    let s = "";
    for (let i = 0; i < n && b[o + i]; i++) s += String.fromCharCode(b[o + i]);
    return s;
  };
  const b64 = (bytes) => btoa(String.fromCharCode(...bytes));
  const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
  const param = (p) => S.params[ID[p]];
  /* an empty bass: every step a CUT of a sixteenth, SEQUENCE off, all its steps, not transposed (app.c bseq_empty) */
  const EMPTY_BQ = new Uint8Array(BQ);
  EMPTY_BQ[ST.bq - REC + 1] = param("P_BQ_STEPS").def;
  EMPTY_BQ[ST.bq - REC + 2] = param("P_BQ_TRANS").def;
  const joined = (rec, bq) => { const b = new Uint8Array(ST.size); b.set(rec.subarray(0, REC)); b.set(bq, REC); return b; };
  const objOf = (objs, obj) => objs.find((o) => o.obj === obj);

  // the sound as the screen shows it once loaded: a drone preset's settings over CUSTOM's (app.c drone_take)
  function sounding(v) {
    const out = Uint8Array.from(v);
    const k = v[ID.P_DR_PRESET];
    if (k) S.drone.p.forEach((p, i) => { out[p] = S.drone.presets[(k - 1) % S.drone.presets.length][i]; });
    return out;
  }

  // what a scene plays (the same bytes in every layout: chords, arpeggio, bass; at their offsets for a name of nl
  // and nv settings)
  function plays(b, o, nl, nv, extra) {
    const at = o + nl + nv, nch = b[at], narp = b[at + 1], chords = [], arp = [];
    for (let i = 0; i < Math.min(nch, N.SC_CHORDS); i++) {
      const c = at + 2 + i * 5, root = s8(b[c]);
      if (root < 0) continue;
      const notes = [];
      for (let j = 1; j < 5; j++) if (s8(b[c + j]) >= 0) notes.push(s8(b[c + j]));
      chords.push({ root, notes });
    }
    for (let i = 0; i < Math.min(narp, N.SC_ARP); i++) arp.push(s8(b[at + 2 + N.SC_CHORDS * 5 + i]));
    const bass = extra ? s8(b[at + 2 + N.SC_CHORDS * 5 + N.SC_ARP]) : -1;
    return { chords, arp, bass };
  }

  // the bass's sequence: its notes (bseq.cut, bseq.hold, else a note), lengths (bseq.ticks' index), SEQUENCE, STEPS,
  // TRANSPOSE (q: SEQUENCE's offset, STEPS and TRANSPOSE after it; out of range: their defaults, as scene_take has it)
  function bassSeq(b, note, len, q) {
    const st = param("P_BQ_STEPS"), tr = param("P_BQ_TRANS");
    return { notes: Array.from(b.subarray(note, note + N.SC_BSEQ), (x) => x & 127), lens: Array.from(b.subarray(len, len + N.SC_BSEQ)),
             on: !!b[q], steps: b[q + 1] > st.max ? st.def : b[q + 1], trans: b[q + 2] > tr.max ? tr.def : b[q + 2] };
  }
  const NO_BSEQ = bassSeq(joined(new Uint8Array(REC), EMPTY_BQ), ST.bnote, ST.blen, ST.bq);

  /* every earlier layout kept SCATTER where GLINT's RELEASE is: RELEASE as its DECAY let a glint go then (the
   * schema's table, app.c glint_release_of) */
  const SG_ = S.scatter_gone;
  const scatterGone = (b, v) => { b[v + SG_.p] = SG_.of[b[v + SG_.decay]]; };

  /* a shard of an earlier layout (one of S.old_shards) in this one: its name and its sound, the settings the sound has
   * gained since at their defaults (GLINT's RELEASE where SCATTER was), the rest of its record, then its bass's
   * sequence (none: empty), as the FM-1 makes it over (app.c scenes_over, sound_over) */
  function upgrade(b, old) {
    const out = new Uint8Array(ST.size), sv = ST.v + old.nv;
    out.set(b.subarray(0, sv));
    for (let p = old.nv; p < N.NSOUND; p++) out[ST.v + p] = S.params[p].def;
    out.set(b.subarray(sv, old.rec), ST.v + N.NSOUND);
    out.set(old.size > old.rec ? b.subarray(old.rec) : EMPTY_BQ, REC);
    scatterGone(out, ST.v);
    return out;
  }

  // a scene_t in this layout -> the scene
  function scene(bytes) {
    const b = bytes, v = b.subarray(ST.v, ST.v + N.NSOUND);
    return {
      current: true, bytes: Uint8Array.from(b), name: text(b, ST.name, ST.name_len), v: sounding(v), raw_v: Uint8Array.from(v),
      ...plays(b, ST.name, ST.name_len, N.NSOUND, 1), seq: Array.from(b.subarray(ST.seq, ST.seq + N.SC_SEQ)),
      bseq: bassSeq(b, ST.bnote, ST.blen, ST.bq),
    };
  }

  const FACTORY = S.factory.map((h) => scene(fromHex(h)));

  // the scenes' record and their basses' (none: each empty) -> 16 scenes, slot by slot (a slot with nothing saved: the
  // factory's, as scene_get has it)
  function scenes(p, q) {
    const out = [];
    const empty = (slot) => ({ ...FACTORY[slot], slot, state: slot < N.NFACTORY ? "factory" : "empty" });
    if (!p || p.length < 8) {                         // (none saved yet)
      for (let s = 0; s < N.NSCENES; s++) out.push(empty(s));
      return { layout: S.magic.scenes, current: true, scenes: out };
    }
    const magic = magicAt(p, 0), used = u16(p, SS.used);
    if (magic === S.magic.scenes && p.length === SS.rec) {
      const bq = q && q.length === SB.size && magicAt(q, 0) === S.magic.bseqs ? q : null;
      for (let s = 0; s < N.NSCENES; s++) {
        const o = SS.s + s * REC, b = joined(p.subarray(o, o + REC), bq ? bq.subarray(SB.s + s * BQ, SB.s + (s + 1) * BQ) : EMPTY_BQ);
        out.push(used >> s & 1 ? { ...scene(b), slot: s, state: "saved" } : empty(s));
      }
      return { layout: magic, current: true, scenes: out };
    }
    const l = S.old_scenes.find((x) => x.magic === magic);
    const rs = l && l.name + l.nv + 2 + N.SC_CHORDS * 5 + N.SC_ARP + (l.extra ? 1 : 0) + (l.extra >= 2 ? N.SC_SEQ : 0);
    if (!l || p.length !== 8 + N.NSCENES * rs) throw new Error(`The scenes in this file are in a layout this page doesn't know (${magic}). It may come from a newer FRASIL.`);
    const bq = q && q.length === SB.size && magicAt(q, 0) === S.magic.bseqs ? q : null;   // (FRSh's: beside them)
    for (let s = 0; s < N.NSCENES; s++) {             // an earlier FRASIL's: the names and what plays (its sound is
      const o = 8 + s * rs;                           // brought up to date by the FM-1 when restored)
      const bs = bq ? bassSeq(bq, SB.s + s * BQ, SB.s + s * BQ + N.SC_BSEQ, SB.s + s * BQ + 2 * N.SC_BSEQ) : NO_BSEQ;
      out.push(used >> s & 1 ? { slot: s, state: "saved", current: false, name: text(p, o, l.name), ...plays(p, o, l.name, l.nv, l.extra), bseq: bs }
                             : empty(s));
    }
    return { layout: magic, current: false, scenes: out };
  }

  // the settings' record -> what it keeps (this layout's, or one settings_over makes over byte by byte, FRSI, FRSH and
  // FRSG: the sound's new settings at their defaults, GLINT's RELEASE where SCATTER was, a setting learned after the
  // sound numbered as now, one learned on SCATTER learned no more, FRSG's bass's sequence empty; an earlier one's: only
  // that it is there)
  function settings(p) {
    if (!p || !p.length) return null;
    const magic = magicAt(p, 0), over = S.old_settings_over.find((x) => x.magic === magic && x.size === p.length);
    if (over) {
      const b = new Uint8Array(SG.size), add = N.NSOUND - over.nv;
      for (let i = 0; i < 4; i++) b[i] = S.magic.settings.charCodeAt(i);
      b.set(p.subarray(4, 4 + over.nv), SG.v);
      for (let i = over.nv; i < N.NSOUND; i++) b[SG.v + i] = S.params[i].def;
      b[SG.bq_steps] = param("P_BQ_STEPS").def;
      b[SG.bq_trans] = param("P_BQ_TRANS").def;
      b.set(p.subarray(4 + over.nv, over.end), SG.octave);
      for (let i = 0; i < N.NLEARN; i++) {
        const x = b[SG.learn_p + i];
        if (x >= over.nv && x < N.NPARAM - add) b[SG.learn_p + i] = x + add;
        if (x === SG_.p) b[SG.learn_cc + i] = b[SG.learn_p + i] = 0;
      }
      scatterGone(b, SG.v);
      return { ...settings(b), layout: magic };
    }
    if (magic !== S.magic.settings || p.length !== SG.size) return { current: false, layout: magic };
    const learned = [];
    for (let i = 0; i < N.NLEARN; i++) if (p[SG.learn_cc + i]) learned.push({ cc: p[SG.learn_cc + i], p: p[SG.learn_p + i] });
    const v = p.subarray(SG.v, SG.v + N.NSOUND);
    return {
      current: true, layout: magic, v: sounding(v), raw_v: Uint8Array.from(v), octave: s8(p[SG.octave]),
      keys: p[SG.keys], channel: p[SG.channel], ch_glint: p[SG.ch_glint], ch_bass: p[SG.ch_bass],
      scene: p[SG.scene] % N.NSCENES, tempo: p[SG.tempo], metro: !p[SG.metro_off], morph: p[SG.morph],
      load: p[SG.load], learned, seq: Array.from(p.subarray(SG.seq, SG.seq + N.SC_SEQ)),
      bseq: bassSeq(p, SG.bnote, SG.blen, SG.bq_on),
    };
  }

  // a backup image (PB.parse's objs) -> what it holds
  function read(parsed) {
    const by = Object.fromEntries(parsed.objs.map((o) => [o.obj, o.data]));
    return { flags: parsed.flags, settings: settings(by[S.objs.settings]), ...scenes(by[S.objs.scenes], by[S.objs.bseqs]) };
  }

  // ----------------------------------------------------------------- scene packages (.frasilscenes)
  function packOf(list, firmware) {
    for (const sc of list) if (!sc.current) throw new Error(`${sc.name} was made by an earlier FRASIL. Restore the whole backup first, then back up again.`);
    return JSON.stringify({ format: PACK, version: 1, layout: S.magic.shard, firmware,
                            scenes: list.map((sc) => ({ name: sc.name, from: sc.slot + 1, data: b64(sc.bytes) })) }, null, 1);
  }

  function readPack(txt) {
    let j;
    try { j = JSON.parse(txt); } catch (_) { throw new Error("This file is not a FRASIL scene package."); }
    if (!j || j.format !== PACK || !Array.isArray(j.scenes)) throw new Error("This file is not a FRASIL scene package.");
    if (j.version !== 1) throw new Error(`This scene package is from a newer FRASIL (format ${j.version}).`);
    const old = S.old_shards.find((x) => x.magic === j.layout);   // (made before: brought up to date, upgrade)
    if (j.layout !== S.magic.shard && !old) throw new Error(`These scenes were made by another FRASIL version (${j.layout}). Update FRASIL on the FM-1 and this page, then try again.`);
    const list = j.scenes.map((x, i) => {
      let bytes;
      try { bytes = unb64(String(x.data || "")); } catch (_) { bytes = null; }
      if (bytes && old) bytes = bytes.length === old.size ? upgrade(bytes, old) : null;
      if (!bytes || bytes.length !== ST.size) throw new Error(`Scene ${i + 1} in this package is damaged.`);
      for (let p = 0; p < N.NSOUND; p++)
        if (bytes[ST.v + p] > S.params[p].max) throw new Error(`Scene ${i + 1} in this package has a setting out of range.`);
      const from = Number.isInteger(x.from) && x.from >= 1 && x.from <= N.NSCENES ? x.from - 1 : i % N.NSCENES;
      return { ...scene(bytes), slot: from, state: "saved" };
    });
    return { layout: j.layout, firmware: String(j.firmware || ""), scenes: list };
  }

  // ----------------------------------------------------------------- an image, rebuilt (device/backup.c's format)
  // every object this FRASIL backs up, in order (one it isn't given: absent, empty)
  function build(objs, flags = 0) {
    const crc = PB.crc32;
    objs = [...OBJS.map((obj) => objOf(objs, obj) || { obj, data: new Uint8Array(0) }), ...objs.filter((o) => !OBJS.includes(o.obj))];
    const head = [...PB.MAGIC, PB.VERSION & 0xFF, PB.VERSION >> 8, objs.length & 0xFF, objs.length >> 8,
                  flags & 0xFF, (flags >> 8) & 0xFF, (flags >> 16) & 0xFF, flags >>> 24];
    const le32 = (v) => [v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, v >>> 24];
    const out = [...head, ...le32(crc(Uint8Array.from(head)))];
    for (const { obj, data } of objs) out.push(obj & 0xFF, obj >> 8, data.length & 0xFF, data.length >> 8, ...le32(crc(data)), ...data);
    return Uint8Array.from([...out, ...le32(crc(Uint8Array.from(out)))]);
  }

  // the FM-1's fresh backup with scenes put into slots: its settings and the other slots as they are on it. Only into
  // the layout the FM-1 itself keeps (an earlier or a newer FRASIL's: never written over; a FRASIL without OBJ_BSEQ
  // doesn't list it, and would refuse it). Each scene's record into OBJ_SCENES, its bass into OBJ_BSEQ (nothing in it
  // yet: made, the other slots' basses empty, as the FM-1 reads them)
  function patch(image, places) {
    const { objs } = PB.parse(image);
    const sc = objOf(objs, S.objs.scenes), bo = objOf(objs, S.objs.bseqs);
    let p, q;
    if (!sc) throw new Error("The FM-1's backup has no place for scenes.");
    if (!bo) throw new Error("The FM-1 runs another version of FRASIL. Update it on the Install page, then try again.");
    if (sc.data.length) {
      if (magicAt(sc.data, 0) !== S.magic.scenes || sc.data.length !== SS.rec)
        throw new Error("The FM-1 runs another version of FRASIL. Update it on the Install page, then try again.");
      p = Uint8Array.from(sc.data);
    } else {                                           // nothing saved on it yet: a record of factory slots
      p = new Uint8Array(SS.rec);
      for (let i = 0; i < 4; i++) p[i] = S.magic.scenes.charCodeAt(i);
      FACTORY.forEach((f, s) => p.set(f.bytes.subarray(0, REC), SS.s + s * REC));
    }
    if (bo && bo.data.length === SB.size && magicAt(bo.data, 0) === S.magic.bseqs) q = Uint8Array.from(bo.data);
    else {
      q = new Uint8Array(SB.size);
      for (let i = 0; i < 4; i++) q[i] = S.magic.bseqs.charCodeAt(i);
      for (let s = 0; s < N.NSCENES; s++) q.set(EMPTY_BQ, SB.s + s * BQ);
    }
    let used = u16(p, SS.used);
    for (const { slot, bytes } of places) {
      if (bytes.length !== ST.size || slot < 0 || slot >= N.NSCENES) throw new Error("A scene doesn't fit.");
      p.set(bytes.subarray(0, REC), SS.s + slot * REC);
      q.set(bytes.subarray(REC), SB.s + slot * BQ);
      used |= 1 << slot;
    }
    p[SS.used] = used & 0xFF;
    p[SS.used + 1] = used >> 8;
    return build([...objs.filter((o) => o.obj !== S.objs.scenes && o.obj !== S.objs.bseqs),
                  { obj: S.objs.scenes, data: p }, { obj: S.objs.bseqs, data: q }], 0);
  }

  return { S, read, packOf, readPack, patch, build, sounding, FACTORY };
})(SCHEMA);
