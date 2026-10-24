// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// A simulated FM-1 for the River's tests in Chrome (fm1-base's tests/backup_page_test.mjs FakeFM1, as a fake Web MIDI
// injected before the page: page.addInitScript(FAKE)): its identity, a backup in 128-byte frames, a restore pulled
// with read requests, the question, STOP FIRST. window.__fm1:
//   image          what it keeps (set it first); mutate(image): changes it before the next backup
//   connected      false: unplugged (no MIDI ports at all)
//   id             its identity: FRASIL 1.1's release (FM-1_47010100); another firmware's (FM-1_0103): FRASIL not installed
//   playing        a scene plays: STOP FIRST, unless MIDI's All Sound Off (CC 120) lets it go first, as FRASIL's RELEASE
//                  ALL does (soundOff counts them; ccBeforeRestore: how many had come when the last restore began)
//   held           a key held down: STOP FIRST whatever comes (CC 120 doesn't let a hand go)
//   answer         true: SEL (yes), false: FX (no), null: the question waits until it is set
//   slow           ms between two frames of a backup, between two answers of a restore (0: as fast as it can)
//   asked, backups, restores, events (what happened, in order: "cc120", "backup", "restore", "ask", "restored", ...)
export const FAKE = `(() => {
  const dev = window.__fm1 = { image: null, connected: true, id: "FM-1_47010100", playing: false, held: false, answer: true, slow: 0,
                               asked: 0, backups: 0, restores: 0, soundOff: 0, ccBeforeRestore: -1, events: [], mutate: null };
  const input = { name: "FRASIL FM-1", get state() { return dev.connected ? "connected" : "disconnected"; }, open: async () => {}, onmidimessage: null };
  const emit = (pkt, ms = 1) => setTimeout(() => input.onmidimessage && input.onmidimessage({ data: Uint8Array.from(pkt) }), ms);
  const output = { name: "FRASIL FM-1", get state() { return dev.connected ? "connected" : "disconnected"; }, open: async () => {},
                   send: (d) => { if (!dev.connected) throw new Error("disconnected"); const a = Array.from(d); setTimeout(() => rx(a), 1); } };
  const u32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
  const le32 = (v) => [v & 255, v >>> 8 & 255, v >>> 16 & 255, v >>> 24 & 255];
  const frame = (cmd, body) => { const s = body.reduce((a, v) => a + v, 0); return [0xF0, ...PB.pack7([0, 0x59, cmd, body.length & 255, body.length >> 8, 0, ...body, ~s & 255]), 0xF7]; };
  let pull = null, wanted = null;
  function* puller(n, c) {
    dev.events.push("restore");
    dev.ccBeforeRestore = dev.soundOff;
    if (dev.playing || dev.held) { dev.events.push("stopfirst"); yield [(PB.FAIL_ADDR | PB.PLAYING) >>> 0, 8]; return; }
    const got = [...(yield [0, 16])];
    const nobj = got[6] | got[7] << 8;
    let pos = 16;
    for (let i = 0; i < nobj; i++) {
      const r = yield [pos, 8]; got.push(...r);
      const ln = r[2] | r[3] << 8; pos += 8;
      for (let k = 0; k < ln; k += 512) got.push(...(yield [pos + k, Math.min(512, ln - k)]));
      pos += ln;
    }
    got.push(...(yield [pos, 4]));
    const g = Uint8Array.from(got);
    if (g.length !== n || PB.crc32(g) !== c) { yield [(PB.FAIL_ADDR | 6) >>> 0, 8]; return; }
    try { PB.parse(g); } catch (_) { yield [(PB.FAIL_ADDR | 5) >>> 0, 8]; return; }
    yield [PB.ASK_ADDR, 8];
    dev.asked++;
    dev.events.push("ask");
    if (!dev.answer) { dev.events.push("declined"); yield [(PB.FAIL_ADDR | PB.DECLINED) >>> 0, 8]; return; }
    dev.image = g; dev.restores++;
    dev.events.push("restored");
    yield [PB.OK_ADDR, 8];
  }
  const ask = (addr, n, ms) => { const q = [0, ...le32(addr >>> 0), n & 255, n >> 8, 0]; emit([0xF0, ...PB.pack7([0, 0x59, 0x30, 8, 0, 0, ...q, ~q.reduce((a, v) => a + v, 0) & 255]), 0xF7], ms); wanted = [addr >>> 0, n]; };
  const next = (data) => {
    /* the question waits for its answer (answer null), as the FM-1 waits for SEL or FX */
    if (wanted && wanted[0] === PB.ASK_ADDR && dev.answer == null) { setTimeout(() => next(data), 40); return; }
    const r = pull.next(data);
    if (r.done) { pull = null; return; }
    ask(...r.value, 1 + dev.slow);
  };
  function rx(d) {
    if (!dev.connected) return;
    if (d.length === 3 && (d[0] & 0xF0) === 0xB0 && d[1] === 120) { dev.soundOff++; dev.events.push("cc120"); dev.playing = false; return; }
    if (d.length === PB.HS_QUERY.length && d.every((x, i) => x === PB.HS_QUERY[i])) {
      const t = [...dev.id].map((ch) => ch.charCodeAt(0));
      emit([0xF0, ...PB.pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
      return;
    }
    const f = PB.parseFrame(d);
    if (f && f.cmd === PB.BU_REQ) {
      dev.backups++;
      dev.events.push("backup");
      if (dev.mutate) { dev.image = dev.mutate(dev.image); dev.mutate = null; }
      const img = dev.image;
      let k = 0;
      for (let off = 0; off < img.length; off += 128) emit(frame(PB.BU_REQ, [...le32(off), ...img.subarray(off, off + 128)]), 1 + dev.slow * k++);
      emit(frame(PB.BU_END, [...le32(img.length), ...le32(PB.crc32(img)), 0, dev.playing ? 1 : 0]), 2 + dev.slow * k);
      return;
    }
    if (f && f.cmd === PB.RS_REQ && f.body.length === 8) { pull = puller(u32(f.body, 0), u32(f.body, 4)); next(undefined); return; }
    const u = PB.unpack7(d.slice(1, -1));
    if (pull && wanted && u.length >= 15 && u[2] === 0x30) {
      const [addr, n] = wanted;
      if (u32(u, 7) !== addr || (u[11] | u[12] << 8) !== n) return;
      next(Uint8Array.from(u.slice(14, 14 + n)));
    }
  }
  const ports = (p) => (dev.connected ? new Map([[p === input ? "i" : "o", p]]) : new Map());
  Object.defineProperty(navigator, "requestMIDIAccess", { configurable: true,
    value: async () => ({ get inputs() { return ports(input); }, get outputs() { return ports(output); } }) });
})();`;
