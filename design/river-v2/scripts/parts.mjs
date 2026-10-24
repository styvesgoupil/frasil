// FRASIL River v2 mockups: the shared parts (icons, the header, a shard's glyph and its line of what plays, the rows,
// the opened card, the device hints, the FM-1 illustration, the dialogs' frame). build.mjs assembles the pages.
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
const HERE = path.dirname(fileURLToPath(import.meta.url));
export const D = JSON.parse(fs.readFileSync(path.join(HERE, "../data/scenes.json"), "utf8"));
export const real = (name) => D.scenes.find((s) => s.name === name && s.state === "saved") || D.scenes.find((s) => s.name === name);
const r2 = (v) => Math.round(v * 100) / 100;

// ================================================================ icons (16 px, the River's line)
const sv = (d, w = 16, extra = "") => `<svg viewBox="0 0 16 16" width="${w}" height="${w}" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"${extra}>${d}</svg>`;
export const I = {
  play: (w = 13) => `<svg viewBox="0 0 16 16" width="${w}" height="${w}" aria-hidden="true"><path d="M5.2 3.1v9.8l7.6-4.9z" fill="currentColor"/></svg>`,
  stop: (w = 12) => `<svg viewBox="0 0 16 16" width="${w}" height="${w}" aria-hidden="true"><rect x="4" y="4" width="8" height="8" rx="1.2" fill="currentColor"/></svg>`,
  get: (w = 17) => sv(`<path d="M8 1.6v8.1M4.7 6.5 8 9.8l3.3-3.3"/><path d="M1.8 13.1c1.55 0 1.55 1 3.1 1s1.55-1 3.1-1 1.55 1 3.1 1 1.55-1 3.1-1" stroke-width="1.3"/>`, w),
  send: (w = 17) => sv(`<path d="M8 10.2V2M4.7 5.3 8 2l3.3 3.3"/><path d="M1.8 13.1c1.55 0 1.55 1 3.1 1s1.55-1 3.1-1 1.55 1 3.1 1 1.55-1 3.1-1" stroke-width="1.3"/>`, w),
  star: (w = 15, on = false) => `<svg viewBox="0 0 16 16" width="${w}" height="${w}" aria-hidden="true"><path d="M8 2.1L9.25 6.75L14.3 8L9.25 9.25L8 13.9L6.75 9.25L1.7 8L6.75 6.75Z" fill="${on ? "currentColor" : "none"}" stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"/></svg>`,
  link: (w = 15) => sv(`<path d="M6.6 9.4l2.8-2.8M7 4.6l1.3-1.3a2.6 2.6 0 0 1 3.7 3.7L10.7 8.3M9 11.4l-1.3 1.3A2.6 2.6 0 0 1 4 9l1.3-1.3" stroke-width="1.4"/>`, w),
  edit: (w = 15) => sv(`<path d="M10.6 2.6l2.8 2.8-7.6 7.6H3v-2.8z" stroke-width="1.4"/>`, w),
  share: (w = 15) => sv(`<path d="M1.5 11c2 0 2-1.6 4-1.6s2 1.6 4 1.6 2-1.6 4-1.6" stroke-width="1.35"/><circle cx="8" cy="4.6" r="2.1" stroke-width="1.35"/>`, w),
  dl: (w = 15) => sv(`<path d="M8 2.5v7.5M4.8 6.8 8 10l3.2-3.2M3 13.5h10"/>`, w),
  x: (w = 16) => sv(`<path d="M3.5 3.5l9 9M12.5 3.5l-9 9"/>`, w),
  q: (w = 16) => sv(`<circle cx="7" cy="7" r="4.6" stroke-width="1.4"/><path d="M10.4 10.4L14 14" stroke-width="1.4"/>`, w),
  dn: (w = 10) => `<svg viewBox="0 0 10 10" width="${w}" height="${w}" aria-hidden="true"><path d="M2 3.6L5 6.6L8 3.6" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round" stroke-linejoin="round"/></svg>`,
  chev: () => `<svg class="chev" width="10" height="10" viewBox="0 0 10 10" aria-hidden="true"><path d="M3.5 1.5L7 5L3.5 8.5" fill="none" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"></path></svg>`,
  up: (w = 14) => sv(`<path d="M3.5 10L8 5.5L12.5 10"/>`, w),
  ext: (w = 12) => sv(`<path d="M6 3H3.5v9.5H13V10M9 3h4v4M13 3L7.5 8.5" stroke-width="1.4"/>`, w),
  open: (w = 15) => sv(`<path d="M9 2H4.2c-.7 0-1.2.5-1.2 1.2v9.6c0 .7.5 1.2 1.2 1.2h7.6c.7 0 1.2-.5 1.2-1.2V6L9 2zM9 2v4h4" stroke-width="1.35"/>`, w),
  undo: (w = 15) => sv(`<path d="M4 6.5h6a3 3 0 0 1 0 6H6.5M6.5 3.5L3.5 6.5l3 3" stroke-width="1.4"/>`, w),
  fm1: (w = 22) => `<svg viewBox="0 0 22 14" width="${w}" height="${r2(w * 14 / 22)}" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.1"><rect x=".6" y=".6" width="20.8" height="12.8" rx="2.2"/><rect x="7" y="2.4" width="4.6" height="4.6" rx=".6"/><path d="M2 10.4h18" stroke-dasharray="1 .9"/></svg>`,
};

// ================================================================ the page's frame
export function header(cur, who) {
  const c = (k) => (cur === k ? ' aria-current="page"' : "");
  const w = who ? `<a class="rvh-who is-in" href="#"${c("profile")}>${who}</a>` : `<a class="rvh-who" href="#">Sign in</a>`;
  return `<header class="rvh">
  <a class="rvh-mark" href="#" aria-label="FRASIL River">
    <svg class="rvh-logo" viewBox="0 0 150 64" width="150" height="64" aria-hidden="true"><image href="assets/logo-duo.png" x="-10.93" y="3.68" width="172.12" height="34.94"/><text class="rvh-river" x="2" y="62" textLength="146" lengthAdjust="spacing">RIVER</text></svg>
    <span class="rvh-tag" aria-hidden="true">Community shard library</span>
  </a>
  <nav class="rvh-nav" aria-label="FRASIL River">
    <a class="p-yours" href="#"${c("yours")}><span class="nav-long">Your </span>Private<span class="nav-long"> River</span></a>
    <a class="p-nav" href="#"${c("community")}>Community<span class="nav-long"> River</span></a>
    <a class="p-tools" href="#"${c("tools")}>Tools</a>
    ${w}
  </nav>
</header>`;
}
export function page({ title, cur, who, body, over = "", cls = "" }) {
  return `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>${title} · FRASIL River v2 (mockup)</title><link rel="stylesheet" href="v2.css"></head>
<body class="v2 ${cls}">${D.defs}
${header(cur, who)}
<main class="wrap">${body}</main>
${over}
</body></html>`;
}

// ================================================================ a shard's own glyph (river-cards.js's, seeded by its name)
function hashStr(s) { let x = 2166136261 >>> 0; for (const c of s) { x ^= c.charCodeAt(0) & 255; x = Math.imul(x, 16777619) >>> 0; } return x; }
function shardPath(seed) {
  let t = seed >>> 0;
  const rnd = () => { t = (t + 0x6D2B79F5) >>> 0; let r = Math.imul(t ^ (t >>> 15), 1 | t); r ^= r + Math.imul(r ^ (r >>> 7), 61 | r); return ((r ^ (r >>> 14)) >>> 0) / 4294967296; };
  const j = (a, b) => a + (b - a) * rnd();
  const up = j(11.5, 15), dn = j(7.5, 10.5), w = j(4.8, 6.6), tilt = j(-1.6, 1.6);
  const pts = [[tilt, -up], [w * j(0.5, 0.95), -up * j(0.3, 0.68)], [w * j(0.8, 1.05), dn * j(-0.05, 0.35)],
               [w * j(-0.25, 0.35), dn], [-w * j(0.6, 1), dn * j(0.15, 0.55)], [-w * j(0.75, 1.05), -up * j(0.1, 0.5)]];
  const d = "M" + pts.map(([x, y]) => `${r2(x)} ${r2(y)}`).join("L") + "Z";
  const mid = [r2(j(-1.6, 1.6)), r2(j(-3, 1))];
  return { d, facet: `M${r2(pts[0][0])} ${r2(pts[0][1])}L${mid[0]} ${mid[1]}L${r2(pts[3][0])} ${r2(pts[3][1])}` };
}
export function glyph(name, size = 26, state = "saved", line = true) {
  const { d, facet } = shardPath(hashStr(name));
  const sc = state === "factory" ? 0.78 : 1;
  const parts = ["dn", "up"].map((p) => `<g class="g-${p}" clip-path="url(#rv-clip-${p})"><path class="g-body" d="${d}"/><path class="g-facet" d="${facet}"/></g>`).join("");
  return `<svg class="shard-g is-${state}" viewBox="-13 -17 26 30" width="${r2(size * 26 / 30)}" height="${size}" aria-hidden="true">${line ? '<line class="g-line" x1="-13" x2="13" y1="0" y2="0"/>' : ""}<g transform="scale(${sc})">${parts}</g></svg>`;
}

// ================================================================ what plays, in one line (river-cards.js's mini)
const X = (n) => `calc(6px + (100% - 12px) * ${r2((n - 24) / 72)})`;
export function ms(sp, playing = false) {
  let h = `<div class="ms" role="img"><span class="ms-line"></span>`;
  for (let c = 24; c <= 96; c += 12) h += `<span class="ms-c" style="left:${X(c)}"></span>`;
  const st = {};
  const PRI = { arpr: 1, arp: 2, note: 3, root: 4 };
  const set = (n, w) => { if (n >= 24 && n <= 96 && (!st[n] || PRI[w] > PRI[st[n]])) st[n] = w; };
  (sp.chords || []).forEach(([root, ...notes]) => { set(root, "root"); notes.forEach((n) => set(n, "note")); });
  (sp.arp || []).forEach((n) => { set(n, "arp"); set(n + 12, "arpr"); });
  for (const [n, w] of Object.entries(st)) h += `<span class="ms-n ms-${w}" style="left:${X(+n)}"></span>`;
  [...new Set(sp.seq || [])].forEach((n) => { h += `<span class="ms-seq" style="left:${X(n)}"></span>`; });
  if (sp.bass) h += `<span class="ms-bass" style="left:${X(sp.bass)}"></span>`;
  [...new Set(sp.bq || [])].forEach((n) => { h += `<span class="ms-bq" style="left:${X(n)}"></span>`; });
  if (!Object.keys(st).length && !sp.bass && !(sp.seq || []).length && !(sp.bq || []).length) h += `<span class="ms-only">SOUND ONLY</span>`;
  const pts = playing ? Array.from({ length: 49 }, (_, i) => { const x = i / 48, e = Math.sin(Math.PI * x); return `${r2(x * 100)},${r2(10 - 0.7 * 8 * e * (0.62 * Math.sin(x * 9.4 - 2.1) + 0.38 * Math.sin(x * 23 + 1.3)))}`; }).join(" ") : "0,10 100,10";
  h += `<svg class="ms-wave" viewBox="0 0 100 20" preserveAspectRatio="none" aria-hidden="true"${playing ? ' style="opacity:1"' : ""}><polyline points="${pts}"></polyline></svg></div>`;
  return h;
}

// ================================================================ the library: real scenes and a plausible river of others
const SCALES = { MAJOR: [0, 2, 4, 5, 7, 9, 11], MINOR: [0, 2, 3, 5, 7, 8, 10], DORIAN: [0, 2, 3, 5, 7, 9, 10], PHRYGIAN: [0, 1, 3, 5, 7, 8, 10],
  LYDIAN: [0, 2, 4, 6, 7, 9, 11], MIXOLYDIAN: [0, 2, 4, 5, 7, 9, 10], "PENTA MIN": [0, 3, 5, 7, 10], "PENTA MAJ": [0, 2, 4, 7, 9], HARMONIC: [0, 2, 3, 5, 7, 8, 11] };
const CH = { FIFTH: [0, 7], TRIAD: [0, 4, 7], MIN: [0, 3, 7], SEVENTH: [0, 4, 7, 10], SUS2: [0, 2, 7], SUS4: [0, 5, 7], ADD9: [0, 4, 7, 14], QUARTAL: [0, 5, 10], SINGLE: [0], MAJ7: [0, 4, 7, 11], MIN7: [0, 3, 7, 10] };
const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
function rng(seed) { let t = seed >>> 0; return () => { t = (t + 0x6D2B79F5) >>> 0; let r = Math.imul(t ^ (t >>> 15), 1 | t); r ^= r + Math.imul(r ^ (r >>> 7), 61 | r); return ((r ^ (r >>> 14)) >>> 0) / 4294967296; }; }
/* [name, root, scale, chord, drone, glint, desc, author, date, shared, favs], o {bseq} -> a shard: its line of what plays made up
 * from its key; o.bseq: the bass plays a sequence (BASS SEQ: up to 16 steps, each a NOTE, a CUT or a HOLD) */
export function shardOf([name, root, scale, chord, drone, glint, desc = "", author = "", date = "", shared = false, favs = 0], o = {}) {
  const R = rng(hashStr(name)), r = NOTE.indexOf(root), sc = SCALES[scale] || SCALES.MINOR, ch = CH[chord] || [0, 7];
  const chords = [];
  const n = 1 + Math.floor(R() * 2.4);
  for (let i = 0; i < n; i++) {
    const base = 36 + r + 12 * Math.floor(R() * 2) + (i ? sc[Math.floor(R() * sc.length)] : 0);
    chords.push([base, ...ch.slice(1).map((x) => base + x)]);
  }
  const sp = { chords };
  if (glint === "ARP") sp.arp = [0, 2, 4].map((k) => 60 + r + sc[(k + Math.floor(R() * 2)) % sc.length]);
  if (glint === "SEQ") sp.seq = Array.from({ length: 6 + Math.floor(R() * 4) }, () => 60 + r + sc[Math.floor(R() * sc.length)] + 12 * Math.floor(R() * 2));
  if (R() > 0.45) sp.bass = 24 + r + (R() > 0.5 ? 12 : 0);
  if (chord === "SINGLE" && glint === "OFF" && R() > 0.6) { sp.chords = []; sp.bass = 0; }
  if (o.bseq) { sp.bass = 0; sp.bq = [0, 4, 3, 1, 5].slice(0, 3 + Math.floor(R() * 3)).map((d) => 36 + r + sc[d % sc.length]); }
  return { name, key: `${root} ${scale}`, chord, drone, glint, desc, author, date, shared, favs, sp, bseq: !!o.bseq };
}
/* the example's real ones, with their real lines of what plays */
export function realShard(name, extra = {}) {
  const s = real(name);
  return { name, key: s.key.split(" · ")[0], chord: s.key.split(" · ")[1], drone: s.drone, glint: s.glint, msHtml: s.ms, ...extra };
}

// ================================================================ a row
export function row(s, o = {}) {
  const lead = o.pub ? "" : `<span class="ck${o.sel ? " on" : ""}"></span>`;
  const pl = `<span class="pl${o.playing ? " on" : ""}">${o.playing ? I.stop() : I.play()}</span>`;
  const desc = s.desc ? `<span class="nm-d">${s.desc}</span>` : "";
  const line = s.msHtml ? (o.playing ? s.msHtml.replace('class="ms-wave"', 'class="ms-wave" style="opacity:1"') : s.msHtml) : ms(s.sp, o.playing);
  const gt = s.glint === "OFF" ? `<span class="gt off">—</span>` : `<span class="gt">${s.glint}</span>`;
  const bq = s.bseq ? `<span class="bq" title="The bass plays its sequence">SEQ</span>` : `<span></span>`;
  const status = o.pub || o.fav ? `<span class="by">${s.author}</span>` : s.shared ? `<span class="st">SHARED</span>` : `<span></span>`;
  const end = o.pub ? `<span class="fav-n">${I.star(12)} ${s.favs}</span><span class="dt">${s.date}</span>` : `<span class="dt">${s.date}</span>`;
  const side = `<span class="side">${s.shared && !o.pub ? '<span class="st"></span>' : o.pub ? `<span class="dt" style="display:block">${s.author}</span>` : ""}</span>`;
  return `<div class="sr${o.sel ? " is-sel" : ""}${o.playing ? " is-playing" : ""}${o.hover ? " is-hover" : ""}">${lead}${pl}
  <span class="nm">${glyph(s.name, 26)}<span class="nm-t"><span class="nm-n">${s.name}</span>${desc}</span></span>
  ${line}<span class="kv">${s.key} <i>· ${s.chord}</i></span>${gt}${bq}${status}${end}${side}</div>`;
}
export function listHead(o = {}) {
  const sort = o.sort || "ADDED";
  const c = (t, r) => `<span class="${t === sort ? "is-sort" : ""}${r ? " r" : ""}">${t}${t === sort ? " ↓" : ""}</span>`;
  if (o.pub) return `<div class="lh"><span></span>${c("NAME")}<span>WHAT PLAYS</span>${c("KEY")}${c("GLINT")}${c("BASS")}${c("AUTHOR")}<span class="r">${I.star(11)}</span>${c("SHARED", true)}</div>`;
  return `<div class="lh"><span></span><span></span>${c("NAME")}<span>WHAT PLAYS</span>${c("KEY")}${c("GLINT")}${c("BASS")}<span>${o.fav ? "AUTHOR" : ""}</span>${c("ADDED", true)}</div>`;
}

// ================================================================ where a page is on the FM-1: the right-hand block of buttons, lit
export const PANEL = [["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]];
export function dhMap(lit, k = 1) {
  const w = 6 * k, hgt = 3.8 * k, gx = 2 * k, gy = 2.4 * k;
  let s = `<svg class="dh-map" width="${r2(6 * w + 5 * gx)}" height="${r2(2 * hgt + gy)}" viewBox="0 0 ${r2(6 * w + 5 * gx)} ${r2(2 * hgt + gy)}" aria-hidden="true">`;
  PANEL.forEach((rw, y) => rw.forEach((b, x) => { s += `<rect${lit.includes(b) ? ' class="on"' : ""} x="${r2(x * (w + gx))}" y="${r2(y * (hgt + gy))}" width="${r2(w)}" height="${r2(hgt)}" rx="${r2(0.9 * k)}"/>`; }));
  return s + "</svg>";
}
export const dh = (keys) => `<span class="dh">${dhMap(keys)}<span class="dh-k">${keys.map((k) => `<b>${k}</b>`).join(" · ")}</span></span>`;
const PAGE_BTN = { "DRONE PARAMS": "EDIT", CHOIR: "EDIT", "CHOIR II": "EDIT", PARTIALS: "EDIT", "PARTIALS II": "EDIT", BREATH: "EDIT", "BREATH II": "EDIT",
  "DRONE SHAPE": "ENV", "DRONE MOTION": "LFO", HARMONY: "SEL", MIXER: "HOME", MASTERING: "GLO", GLOBAL: "GLO", MIDI: "GLO",
  "GENERATIVE FX": "FX", CLOUD: "FX", "CLOUD II": "FX", "CLOUD III": "FX", PUMP: "FX", "PUMP II": "FX", BASS: "SEQ", "BASS II": "SEQ", "BASS III": "SEQ" };
const pageBtn = (n) => PAGE_BTN[n] || (n.startsWith("GLINT") ? "ARP" : n.startsWith("LFO") ? "LFO" : "");

// ================================================================ an opened shard
/* o: {mine, pub, author, desc, meta, playing, open (the sections opened), edit, live} */
export function opened(name, o = {}) {
  const s = real(name);
  const pl = `<span class="pl${o.playing ? " on" : ""}">${o.playing ? I.stop() : I.play()}</span>`;
  const acts = o.mine
    ? `<span class="a a-ice">${I.send(15)} SEND TO FM-1</span><span class="a">${I.edit()} EDIT</span>${o.shared ? `<span class="a a-on">${I.share()} SHARED</span><span class="a">${I.link()} COPY LINK</span>` : `<span class="a">${I.share()} SHARE</span>`}<span class="a a-txt">DELETE</span>`
    : `<span class="a a-ice">${I.send(15)} SEND TO FM-1</span><span class="a${o.fav ? " a-on" : ""}">${I.star(15, o.fav)} FAVORITE</span><span class="a">${I.link()} COPY LINK</span><span class="a a-txt">REPORT</span>`;
  const lead = o.pub ? "" : `<span class="ck"></span>`;
  const head = `<div class="oc-head">${lead}${pl}<span class="oc-nm">${glyph(name, 26)}<span class="oc-n">${o.label || name}</span>${o.live ? '<span class="later-tag" style="margin-left:10px">LATER · LIVE</span>' : ""}${o.edit ? '<span class="later-tag" style="margin-left:10px">LATER · EDITOR</span>' : ""}</span><span class="oc-acts">${o.edit ? o.editActs : acts}</span><span class="oc-x">${I.up(15)}</span></div>`;
  const intro = `<div class="oc-intro">${o.desc ? `<p class="oc-desc">${o.desc}</p>` : o.mine ? `<p class="oc-none">No description. <span style="font-style:normal;color:var(--text-2);text-decoration:underline;text-decoration-color:var(--steel);text-underline-offset:.2em">Add one</span></p>` : ""}<p class="oc-meta">${o.meta}</p></div>`;
  const editBar = o.edit ? `<div class="edit-bar"><b>EDITING</b><span>3 changes, and the sound follows them. The shard itself stays as it was until you save.</span></div>` : "";
  let kb = s.keyboard;
  if (o.live) kb = liveKeys(kb, o.live.keys || []);
  let strip = s.strip;
  if (o.live && strip && o.live.step != null) strip = playhead(strip, o.live.step);
  const left = `<div class="oc-left"><div class="cap">WHAT PLAYS <span>C1 – C7</span></div><div class="scr">${kb}</div>${s.legend}
    <section class="sh-glint" style="border-top:1px solid var(--rule)"><div class="sh-sub gl-sub"><h4>GLINT</h4><span class="gl-type${s.glint === "OFF" ? " is-off" : ""}">${s.glintHead.type}</span><span class="gl-meta"><span class="sig-l">PRESET</span><span class="gl-v">${s.glintHead.meta[0]}</span><span class="sig-l">OCTAVE</span><span class="gl-v">${s.glintHead.meta[1]}</span></span></div>
    ${strip ? `<div class="scr">${strip}</div>` : ""}<div class="cells4">${o.edit ? editCells(s.cells, o.edit) : s.cells}</div></section></div>`;
  const secs = s.secs.map((x, i) => {
    const isOpen = (o.open || []).includes(x.name);
    const body = isOpen ? `<div class="sx-b">${x.strip ? `<div class="scr sx-strip">${x.strip}</div>` : ""}${x.pages.slice(0, o.pagesMax || 4).map((pg) => `<div class="pg"><div class="pg-name">${pg.name}<span class="pg-k">${pageBtn(pg.name)}</span></div>${o.edit && x.name === o.edit.sec ? editPage(pg, o.edit) : pg.cells.join("")}</div>`).join("")}</div>` : "";
    return `<div class="sx${isOpen ? " open" : ""}"><div class="sx-r">${I.chev()}<span class="sx-n">${x.name}</span><span class="sx-m">${(o.mids || {})[x.name] || x.mid}</span>${dh(x.keys)}</div>${body}</div>`;
  }).join("");
  const right = `<div class="oc-right"><div class="sx-h"><span>EVERY PAGE</span><span>ON THE FM-1</span></div>${secs}</div>`;
  const body = o.live ? `<div class="oc-body" style="grid-template-columns:230px minmax(0,5fr) minmax(0,6fr)">${o.live.col}${left}${right}</div>` : `<div class="oc-body">${left}${right}</div>`;
  return `<div class="oc">${head}${intro}${editBar}${body}</div>`;
}
/* the edited cells: changed (lamp), one being turned */
function editCells(cells, e) { return cells; }
function editPage(pg, e) {
  return pg.cells.map((c, i) => {
    const key = `${pg.name}/${i}`;
    if (e.grab === key) return c.replace('class="cell"', 'class="cell grab"').replace(/<span class="v">[^<]*<\/span>/, `<span class="v">${e.vals[key]}</span>`).replace(/--f:[\d.]+/, `--f:${e.f[key]}`);
    if (e.changed.includes(key)) return c.replace('class="cell"', 'class="cell changed"').replace(/<span class="v">[^<]*<\/span>/, `<span class="v">${e.vals[key]}</span>`).replace(/--f:[\d.]+/, `--f:${e.f[key]}`);
    return c;
  }).join("");
}
/* the keyboard, live: the keys sounding now, lit brighter (white x of a note as river-cards.js lays it) */
function keyX(n) { const WH = [0, 2, 4, 5, 7, 9, 11]; let wi = 0; for (let m = 24; m < n; m++) if (WH.includes(m % 12)) wi++; return WH.includes(n % 12) ? wi * 12 + 6 : wi * 12; }
function liveKeys(svg, keys) {
  const add = keys.map(({ n, c }) => { const x = keyX(n), col = c === "ice" ? "#6ed0ff" : c === "lamp" ? "#f2a65a" : "#f3f6f9";
    return `<circle cx="${x}" cy="33" r="16" fill="${col}" opacity=".22"/><circle cx="${x}" cy="33" r="7" fill="${col}" opacity=".5"/><rect x="${x - 1}" y="2" width="2" height="31" fill="${col}" opacity=".85"/>`; }).join("");
  return svg.replace(/(<rect x="0" y="32.5"[^>]*><\/rect>)/, `$1${add}`);
}
function playhead(strip, step) {
  const x = r2(40 + step * (504 - 40) / 31);
  return strip.replace("</svg>", `<rect x="${x - 7}" y="4" width="14" height="88" fill="#6ed0ff" opacity=".07" rx="3"/><line x1="${x}" y1="4" x2="${x}" y2="92" stroke="#6ed0ff" stroke-width="1.2" opacity=".9"/><circle cx="${x}" cy="92" r="2.6" fill="#f3f6f9"/></svg>`);
}


// ================================================================ the FM-1, the laptop and the cable (the guide's panel, drawn small)
/* what the FM-1's screen shows: the device's own screens, shot by the simulator (scripts/device-screens.sh) */
const SCREEN = { reading: "dev-play", sending: "dev-play", ask: "dev-ask", restored: "dev-restored", notrestored: "dev-not-restored", stopfirst: "dev-stop-first" };
/* the buttons lit: only the ones that answer (SEL yes / OK, FX no) */
const LIT = { ask: ["FX", "SEL"], restored: ["SEL"], notrestored: ["SEL"], stopfirst: ["SEL"] };
const NS = 'vector-effect="non-scaling-stroke"';
const ICE = "#6ed0ff", LAMP = "#f2a65a";
/* a glint: FRASIL's four-pointed star, glowing (cx, cy, size) */
const glintStar = (x, y, k = 1, id = "gl") => `<circle cx="${r2(x)}" cy="${r2(y)}" r="${r2(11 * k)}" fill="url(#${id})"/><path transform="translate(${r2(x - 8 * k * 0.9)} ${r2(y - 8 * k * 0.9)}) scale(${r2(0.9 * k)})" d="M8 2.1L9.25 6.75L14.3 8L9.25 9.25L8 13.9L6.75 9.25L1.7 8L6.75 6.75Z" fill="#f3f6f9"/>`;

/* the FM-1 in the guide's units (1180 x 790): o.labels names every button (the close-up); o.hand rings the button the
 * River waits for in lamp, the hand's colour */
function fm1Body(state, o = {}) {
  const off = state === "off";
  const stroke = off ? "#26303c" : "#3a4654";
  const lit = LIT[state] || [];
  let fm = `<rect x="8" y="8" width="1164" height="774" rx="40" fill="${off ? "#0c1015" : "#111821"}" stroke="${off ? "#222b36" : "#2c3744"}" stroke-width="1.2" ${NS}/>`;
  // the hands' knobs, the octave
  for (const [x, y] of [[118, 112], [250, 112], [118, 246], [250, 246]]) fm += `<circle cx="${x}" cy="${y}" r="26" fill="#161d26" stroke="${stroke}" stroke-width="1" ${NS}/><line x1="${x}" y1="${y - 20}" x2="${x}" y2="${y - 11}" stroke="${off ? "#3a4654" : "#aeb7c2"}" stroke-width="1.2" ${NS}/>`;
  for (const x of [76, 190]) fm += `<rect x="${x}" y="346" width="102" height="38" rx="9" fill="#18202a" stroke="${stroke}" stroke-width="1" ${NS}/>`;
  // the screen: the device's own
  const cid = `scr-${state}${o.labels ? "-c" : ""}`;
  fm += `<rect x="350" y="46" width="262" height="262" rx="26" fill="#06080c" stroke="#2a3440" stroke-width="1" ${NS}/>`;
  fm += `<clipPath id="${cid}"><rect x="361" y="57" width="240" height="240" rx="6"/></clipPath><g clip-path="url(#${cid})"><rect x="361" y="57" width="240" height="240" fill="${off ? "#030405" : "#0a0d12"}"/>`;
  if (SCREEN[state]) fm += `<image href="assets/${SCREEN[state]}.png" x="361" y="57" width="240" height="240"${o.dim ? ' opacity=".55"' : ""}/>`;
  if (state === "nofrasil") {
    for (let i = 0; i < 4; i++) for (let j = 0; j < 3; j++) fm += `<rect x="${379 + i * 54}" y="${87 + j * 62}" width="46" height="52" rx="4" fill="${["#c0392b", "#e67e22", "#27ae60", "#2980b9", "#8e44ad", "#16a085"][(i + j * 2) % 6]}" opacity=".16"/>`;
    fm += `<rect x="377" y="127" width="208" height="100" rx="10" fill="#0a0d12" stroke="${LAMP}" stroke-width="1.6" ${NS}/>`;
    fm += `<text x="481" y="169" text-anchor="middle" font-family="Barlow Semi Condensed" font-weight="700" font-size="30" letter-spacing="3" fill="${LAMP}">FRASIL</text><text x="481" y="203" text-anchor="middle" font-family="Barlow Semi Condensed" font-weight="700" font-size="26" letter-spacing="2.4" fill="${LAMP}">NOT INSTALLED</text>`;
  }
  fm += `</g>`;
  // K1..K4
  for (const x of [692, 812, 932, 1052]) fm += `<circle cx="${x}" cy="112" r="24" fill="#161d26" stroke="${stroke}" stroke-width="1" ${NS}/><line x1="${x}" y1="94" x2="${x}" y2="103" stroke="${off ? "#3a4654" : "#aeb7c2"}" stroke-width="1.2" ${NS}/>`;
  if (o.labels) ["K1", "K2", "K3", "K4"].forEach((k, i) => { fm += `<text x="${692 + i * 120}" y="166" text-anchor="middle" font-family="Barlow Semi Condensed" font-weight="600" font-size="15" letter-spacing="1.6" fill="#4a5563">${k}</text>`; });
  // the twelve buttons: only the ones that answer lit
  fm += `<rect x="636" y="226" width="508" height="246" rx="18" fill="#0b1016"/>`;
  PANEL.forEach((rw, j) => rw.forEach((b, i) => {
    const x = 648 + i * 81, y = j ? 350 : 238, on = lit.includes(b);
    if (o.hand === b) fm += `<rect x="${x - 17}" y="${y - 17}" width="98" height="74" rx="20" fill="none" stroke="${LAMP}" stroke-width="1.2" opacity=".3" ${NS}/><rect x="${x - 9}" y="${y - 9}" width="82" height="58" rx="15" fill="${LAMP}" opacity=".1" stroke="${LAMP}" stroke-width="1.4" ${NS}/>`;
    if (on) fm += `<rect x="${x - 8}" y="${y - 8}" width="80" height="56" rx="14" fill="#f3f6f9" opacity=".07"/>`;
    fm += `<rect x="${x}" y="${y}" width="64" height="40" rx="9" fill="${on ? "#2b3644" : "#18202a"}" stroke="${on ? "#f3f6f9" : stroke}" stroke-width="${on ? 1.4 : 1}" ${NS}/>`;
    if (on || o.labels) fm += `<text x="${x + 32}" y="${y + 26}" text-anchor="middle" font-family="Barlow Semi Condensed" font-weight="700" font-size="${o.labels ? 16 : 19}" letter-spacing="1.5" fill="${on ? "#f3f6f9" : "#4a5563"}">${b}</text>`;
  }));
  // the keys: 11 black above, 16 white
  fm += `<rect x="40" y="486" width="1100" height="282" rx="18" fill="#0b1016"/>`;
  for (const x of [93.0, 160.3, 227.5, 362.0, 429.3, 563.8, 631.0, 698.3, 832.8, 900.0, 1034.5]) fm += `<rect x="${x}" y="504" width="52.5" height="93.1" rx="26.2" fill="#141b23" stroke="${off ? "#1f2731" : "#2f3a47"}" stroke-width="1" ${NS}/>`;
  for (let i = 0; i < 16; i++) fm += `<rect x="${r2(59.4 + i * 67.25)}" y="607.3" width="52.5" height="146.6" rx="26.2" fill="${off ? "#141a22" : "#1c2530"}" stroke="${off ? "#222b36" : "#3a4654"}" stroke-width="1" ${NS}/>`;
  // its port, on the left edge
  fm += off ? `<rect x="-14" y="380" width="42" height="80" rx="14" fill="${LAMP}" opacity=".1"/><rect x="2" y="400" width="10" height="40" rx="4" fill="#06080c" stroke="${LAMP}" stroke-width="1.4" ${NS}/>` : `<rect x="2" y="400" width="10" height="40" rx="4" fill="#06080c" stroke="${stroke}" stroke-width="1" ${NS}/>`;
  return fm;
}

/* the whole scene: the laptop with the River on its screen, the USB-C cable (the river between them), the FM-1.
 * state: off | reading | nofrasil | sending */
export function illus(state, o = {}) {
  const S = 0.36, OX = 268, OY = 8;
  const lap = `<g>
    <rect x="10" y="84" width="166" height="108" rx="7" fill="#0b1016" stroke="#3a4654" stroke-width="1.2"/>
    <rect x="17" y="91" width="152" height="94" rx="2" fill="#080b0f"/>
    <g transform="translate(17 91) scale(1.1) translate(-21 -95)">${miniRiver()}</g>
    <path d="M0 196 H186 L181 202 H5 Z" fill="#111821" stroke="#3a4654" stroke-width="1.1" stroke-linejoin="round"/>
    <rect x="79" y="192" width="28" height="4" rx="1.5" fill="#0b1016"/>
    <rect x="183" y="196.6" width="5" height="3" rx="1" fill="#06080c" stroke="#3a4654" stroke-width=".8"/>
  </g>`;
  const port = { x: OX + 2 * S, y: OY + 420 * S };
  let cable;
  if (state === "off") {
    cable = `<path d="M188 198 C 204 198, 204 228, 216 238 S 230 247, 238 247" fill="none" stroke="#3a4654" stroke-width="2" stroke-linecap="round"/>
      <rect x="237" y="242" width="11" height="10" rx="3" fill="#18202a" stroke="#4a5563" stroke-width="1"/><rect x="248" y="244" width="9" height="6" rx="2" fill="#26303c" stroke="#77818f" stroke-width=".9"/>`;
  } else {
    const flowing = state === "reading" || state === "sending";
    // two curves: down from the laptop onto the table, then up into the FM-1's port (u: 0 at the laptop, 1 at the FM-1)
    const E = [port.x - 24, port.y];
    const C = [[[188, 198], [208, 198], [196, 262], [218, 262]], [[218, 262], [240, 262], [226, E[1]], E]];
    const bez = (P, t) => [0, 1].map((k) => (1 - t) ** 3 * P[0][k] + 3 * (1 - t) ** 2 * t * P[1][k] + 3 * (1 - t) * t * t * P[2][k] + t ** 3 * P[3][k]);
    const at = (u) => (u < 0.5 ? bez(C[0], u * 2) : bez(C[1], u * 2 - 1));
    const d = `M${C[0][0].join(" ")} C ${C[0][1].join(" ")}, ${C[0][2].join(" ")}, ${C[0][3].join(" ")} C ${C[1][1].join(" ")}, ${C[1][2].join(" ")}, ${E.map(r2).join(" ")}`;
    cable = `<defs><linearGradient id="cg" x1="0" x2="1"><stop offset="0" stop-color="#4a5563" stop-opacity=".7"/><stop offset=".5" stop-color="${flowing ? "#5d6e80" : "#77818f"}" stop-opacity=".9"/><stop offset="1" stop-color="#4a5563" stop-opacity=".7"/></linearGradient>
      <radialGradient id="gl"><stop offset="0" stop-color="#f3f6f9" stop-opacity=".9"/><stop offset=".3" stop-color="${ICE}" stop-opacity=".55"/><stop offset="1" stop-color="${ICE}" stop-opacity="0"/></radialGradient></defs>
      <path d="${d}" fill="none" stroke="url(#cg)" stroke-width="2" stroke-linecap="round"/>
      <rect x="${r2(port.x - 24)}" y="${r2(port.y - 5)}" width="12" height="10" rx="3" fill="#18202a" stroke="#4a5563" stroke-width="1"/>
      <rect x="${r2(port.x - 12)}" y="${r2(port.y - 3)}" width="12" height="6" rx="1.6" fill="#26303c" stroke="#77818f" stroke-width=".9"/>`;
    if (flowing) {
      // the glint travels: from the FM-1 to the River while reading, the other way while sending; its wake behind it
      const head = state === "reading" ? 0.4 : 0.7, dir = state === "reading" ? 1 : -1;
      for (let k = 18; k > 0; k--) {                     // the wake: brighter and wider near the glint
        const a = at(head + dir * k * 0.018), b = at(head + dir * (k - 1) * 0.018), f = 1 - k / 19;
        cable += `<line x1="${r2(a[0])}" y1="${r2(a[1])}" x2="${r2(b[0])}" y2="${r2(b[1])}" stroke="${ICE}" stroke-width="${r2(1.2 + 2.2 * f)}" stroke-linecap="round" opacity="${r2(0.08 + 0.75 * f * f)}"/>`;
      }
      const [hx, hy] = at(head);
      cable += glintStar(hx, hy, 1.15);
    }
  }
  return `<svg class="ill" viewBox="0 0 700 296" width="${o.w || 600}" aria-hidden="true" style="max-width:${o.w || 600}px">${lap}${cable}<g transform="translate(${OX} ${OY}) scale(${S})">${fm1Body(state, o)}</g></svg>`;
}

/* the FM-1 close: its screen (the device's own, legible) and its buttons, named, lit as the device lights them.
 * state: ask | restored | notrestored | stopfirst */
export function illusClose(state, o = {}) {
  const VX = 318, VW = 862, VH = 438;
  const fade = `<defs><linearGradient id="fl" x1="0" x2="1"><stop offset="0" stop-color="#0c1117"/><stop offset="1" stop-color="#0c1117" stop-opacity="0"/></linearGradient>
    <linearGradient id="fb" x1="0" x2="0" y1="0" y2="1"><stop offset="0" stop-color="#0c1117" stop-opacity="0"/><stop offset="1" stop-color="#0c1117"/></linearGradient></defs>`;
  const veil = `<rect x="${VX - 2}" y="0" width="30" height="${VH + 2}" fill="url(#fl)"/><rect x="${VX - 2}" y="${VH - 64}" width="${VW + 4}" height="66" fill="url(#fb)"/>`;
  return `<svg class="ill ill-close" viewBox="${VX} 0 ${VW} ${VH}" width="${o.w || 680}" aria-hidden="true" style="max-width:${o.w || 680}px">${fade}${fm1Body(state, { labels: true, hand: o.hand })}${veil}</svg>`;
}
/* the River, tiny, on the laptop's screen */
function miniRiver() {
  let s = `<rect x="27" y="101" width="16" height="2.4" rx="1" fill="#3480ec"/><rect x="27" y="104.6" width="11" height="1.6" rx=".8" fill="#6ed0ff" opacity=".7"/>`;
  s += `<rect x="112" y="102" width="12" height="1.6" fill="#77818f"/><rect x="128" y="102" width="12" height="1.6" fill="#dce3ea"/><rect x="144" y="102" width="9" height="1.6" fill="#77818f"/>`;
  s += `<line x1="27" y1="112.5" x2="153" y2="112.5" stroke="#8b95a4" stroke-width=".5" opacity=".6"/>`;
  for (let i = 0; i < 5; i++) {
    const y = 120 + i * 11.5;
    s += `<circle cx="31" cy="${y}" r="2.6" fill="none" stroke="#4a5563" stroke-width=".7"/><rect x="37" y="${y - 1.4}" width="${18 + (i * 7) % 12}" height="2.2" rx=".6" fill="#dce3ea" opacity=".75"/>`;
    s += `<line x1="70" y1="${y + 1}" x2="112" y2="${y + 1}" stroke="#4a5563" stroke-width=".5"/>`;
    for (const k of [0, 1, 2]) s += `<rect x="${78 + ((i * 13 + k * 9) % 30)}" y="${y - 3}" width=".9" height="4" fill="${k ? "#dce3ea" : "#6ed0ff"}" opacity=".8"/>`;
    s += `<rect x="120" y="${y - 1}" width="16" height="1.8" fill="#77818f" opacity=".7"/>`;
    s += `<line x1="27" y1="${y + 5.6}" x2="153" y2="${y + 5.6}" stroke="#1a212a" stroke-width=".5"/>`;
  }
  return s;
}

// ================================================================ dialogs: titled as the FM-1 titles its pages (the subject in tracked capitals, a
// count in a light weight, a waterline under it)
export const dlg = ({ title, sub = "", body, foot = "", cls = "" }) => `<div class="scrim"></div><div class="dlg ${cls}" role="dialog">
  <div class="dlg-h"><p class="dlg-t">${title}${sub ? `<span>${sub}</span>` : ""}</p><span class="dlg-x">${I.x()}</span></div>
  <div class="dlg-b"${foot ? "" : ' style="padding-bottom:34px"'}>${body}</div>${foot ? `<div class="dlg-f">${foot}</div>` : ""}</div>`;
