// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// THE KEYS and the card laid out again (river-keys.js, river-cards.js body()), in Chrome, with the service: Styves keeps
// NORTH RIVER (GLINT on ARP, the bass's sequence, three LFOs), GLACIER (GLINT on SEQ, no bass) and AURORA (GLINT off,
// the bass's sequence). Checked: no WHAT PLAYS; the left column (GLINT's strip, the bass's strip or its line, three
// LFOs); no HARMONY among the pages; an opened LFO's picture, live; THE KEYS: KEYS on GLINT for an arpeggio, a
// sequence; DRONE: HARMONY's settings, a key's chord ghosted, latched (the draft's bytes), let go (no draft); GLINT's
// arpeggio, a note out and in; GLINT's grid: a note placed, longer (TIEs), another note, ACCENT, gone, LENGTH; the bass's
// steps: one selected, a key writes it (AUTO NEXT STEP on, off), CUT, HOLD, STEP LENGTH; HOLD: dim and ▶ nudged with
// nothing playing, a chord while a key is down, nothing kept; UNDO of what it plays, the sound taken again; SAVE AS
// keeps the chord; a phone: no horizontal page scroll, the keys scroll; no console errors.
//   node website/tests/river_keys.mjs [OUT_DIR]
// It starts the service itself (server/.venv's uvicorn, a fresh database, port RIVER_TEST_PORT or 8057, the River as
// tools/website.sh built it). PLAYWRIGHT: Playwright's index.mjs (default: the global one).
import fs from "fs";
import os from "os";
import path from "path";
import { spawn } from "child_process";
import { fileURLToPath } from "url";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");

const REPO = path.join(path.dirname(fileURLToPath(import.meta.url)), "../..");
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), "river-keys-")));
fs.mkdirSync(OUT, { recursive: true });
let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "  ok" : "FAIL"} ${msg}`); if (!ok) fails++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const SCHEMA = JSON.parse(fs.readFileSync(path.join(REPO, "server/schema.json"), "utf8"));
const ST = SCHEMA.scene_t, ID = SCHEMA.ids;

const port = process.env.RIVER_TEST_PORT || "8057", BASE = `http://localhost:${port}`;
try { await fetch(BASE + "/api/health"); console.log(`FAIL port ${port} is taken (RIVER_TEST_PORT)`); process.exit(1); } catch (_) { /* free */ }
const db = path.join(fs.mkdtempSync(path.join(os.tmpdir(), "river-keys-db-")), "river.sqlite");
let log = "";
const srv = spawn(path.join(REPO, "server/.venv/bin/uvicorn"), ["app:create_app", "--factory", "--port", port], {
  cwd: path.join(REPO, "server"),
  env: { ...process.env, RIVER_URL: BASE, COOKIE_SECURE: "0", RIVER_STATIC: path.join(REPO, "build/website/river"), RIVER_DB: db },
});
srv.stdout.on("data", (d) => { log += d; });
srv.stderr.on("data", (d) => { log += d; });
process.on("exit", () => srv.kill());
for (let i = 0; i < 60; i++) { try { if ((await fetch(BASE + "/api/health")).ok) break; } catch (_) { /* not yet */ } await sleep(250); }

const browser = await chromium.launch({ channel: "chrome", args: ["--autoplay-policy=no-user-gesture-required"] });
const errors = [];
async function person(name, email, size = { width: 1440, height: 900 }, more = {}) {
  const ctx = await browser.newContext({ viewport: size, ...more });
  const page = await ctx.newPage();
  page.on("console", (m) => { if ((m.type() === "error" || m.type() === "warning") && !/\/api\/auth\/session|status of 40[14]/.test(m.text())) errors.push(`${name}: ${m.type()}: ${m.text()}`); });
  page.on("pageerror", (e) => { console.log(`    ${name} pageerror: ${e.message}`); errors.push(`${name}: pageerror: ${e.message}`); });
  if (!email) return { ctx, page };
  await page.goto(BASE + "/yours/#register");
  await page.waitForSelector("#register:not([hidden])");
  await page.fill('#register [name="display_name"]', name);
  await page.fill('#register [name="email"]', email);
  await page.fill('#register [name="password"]', "drone in the fog");
  await page.click('#register [type="submit"]');
  await page.waitForSelector("#mine:not([hidden])");
  let tok = null;
  for (let i = 0; i < 40 && !tok; i++) { const m = /#verify=([\w-]+)/.exec(log.slice(log.lastIndexOf(`the email to ${email}`))); tok = m && m[1]; await sleep(150); }
  await page.evaluate((t) => RiverAPI.post("/api/auth/verify", { token: t }), tok);
  return { ctx, page };
}
const add = (page, list) => page.evaluate(async (list) => {
  const ex = FB.read(PB.parse(new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()))).scenes.filter((s) => s.bytes && s.state !== "empty");
  const out = [];
  for (const name of list) {
    const sc = ex.find((s) => s.name === name && s.state === "saved") || ex.find((s) => s.name === name);
    out.push(await RiverAPI.post("/api/shards", { name, layout: FB.S.magic.shard, firmware: FRASIL_VERSION, data: RiverAPI.b64(sc.bytes), from_slot: sc.slot }));
  }
  return out;
}, list);
const api = (page, m, p, b) => page.evaluate(([m, p, b]) => RiverAPI[m](p, b), [m, p, b]);

const { page } = await person("Styves", "styves@river.test");
const [north, glacier, aurora] = await add(page, ["NORTH RIVER", "GLACIER", "AURORA"]);
await page.goto(BASE + "/yours/");
await page.waitForSelector(`#m-${north.id}`);
await page.waitForFunction(() => window.RiverEdit && window.RiverKeys && true);
const open = async (sel) => { await page.click(`${sel} .sr-open`); await page.waitForSelector(`${sel}.open .oc-keys`); await page.waitForTimeout(700); };
const sec = (art, name) => page.evaluate(([art, name]) => { const d = [...document.querySelectorAll(`${art} details.sec`)].find((x) => x.querySelector(".sec-name").firstChild.textContent === name); d.open = true; }, [art, name]);
const cur = (shard) => page.evaluate((id) => { const d = RiverEdit.drafts.get("shard:" + id); return d ? Array.from(d.cur) : null; }, shard.id);
const b0 = (shard) => Array.from(Buffer.from(shard.data, "base64"));
const track = (art, t) => page.click(`${art} .oc-keys [data-k="KEYS:${t}"]`).then(() => page.waitForTimeout(150));
/* a key of THE KEYS, where the pointer goes (a white key low, under the black ones) */
const keyPt = (art, n) => page.evaluate(([art, n]) => {
  const k = document.querySelector(`${art} .oc-keys .k-key[data-n="${n}"]`);
  k.scrollIntoView({ block: "center" });
  const r = k.getBoundingClientRect(), white = r.height > 100 * (r.width / 22);
  return { x: r.left + r.width / 2, y: r.top + r.height * (k.getAttribute("height") > 100 ? 0.86 : 0.5) };
}, [art, n]);
const clickKey = async (art, n) => { const p = await keyPt(art, n); await page.mouse.click(p.x, p.y); await page.waitForTimeout(250); };
/* a cell of GLINT's grid (step i, note as it sounds), where the pointer goes; end: its right edge */
const gridPt = (art, i, note, end) => page.evaluate(([art, i, note, end]) => {
  const b = document.querySelector(`${art} .oc-keys .k-board`), a = b._draw._at, svg = b._draw.querySelector("svg");
  b.scrollIntoView({ block: "center" });
  const r = svg.getBoundingClientRect(), H = svg.viewBox.baseVal.height;
  const x = 72 + (end ? i + 1 - 0.08 : i + 0.5) * a.cw, y = (a.hi - note) * 13 + 18 + 6.5;
  return { x: r.left + x * r.width / 1200, y: r.top + y * r.height / H };
}, [art, i, note, end]);
const gridAt = (art) => page.evaluate((art) => { const a = document.querySelector(`${art} .oc-keys .k-board`)._draw._at; return { lo: a.lo, hi: a.hi, n: a.n, len: a.len }; }, art);
const seqOf = (bytes) => bytes.slice(ST.seq, ST.seq + 32);

// ---------------------------------------------------------------- the card laid out again (NORTH RIVER)
const N = `#m-${north.id}`;
await open(N);
check(!(await page.$(`${N} .oc-body .sh-plays`)), "no WHAT PLAYS: THE KEYS show what plays");
check(await page.$$eval(`${N} .oc-left > *`, (e) => e.map((x) => x.className).join(" ")) === "sh-glint sh-bass oc-lfo", "the left column: GLINT, BASS, the LFOs");
check(!!(await page.$(`${N} .sh-glint .strip`)) && !!(await page.$(`${N} .sh-bass .strip`)) && (await page.$$(`${N} .oc-left .oc-lfo .lf`)).length === 3,
      "...GLINT's walk, the bass's sequence, three LFOs abreast");
check(!(await page.$(`${N} .oc-left .cells4`)), "...GLINT's cells gone from it (THE KEYS have them)");
check(!(await page.$$eval(`${N} .oc-right details.sec .sec-name`, (e) => e.map((x) => x.firstChild.textContent))).includes("HARMONY"), "no HARMONY among the pages");
await sec(N, "BASS");
await page.waitForTimeout(150);
check(!(await page.$(`${N} .oc-right details.sec[open] .sec-body > .scr`)), "BASS's page: its sequence no longer drawn there (the left column has it)");
check(await page.$eval(`${N} .oc-keys [data-k="KEYS:overview"]`, (b) => b.classList.contains("on")) && !(await page.$(`${N} .oc-keys [data-k^="KEY MODE"]`)) && !(await page.$(`${N} .oc-kset .k-bottom`)),
      "THE KEYS open on OVERVIEW: everything it plays, nothing to edit (no KEY MODE, no settings)");
const latchedOf = (art) => page.$eval(`${art} .oc-keys .k-kb`, (svg) => { const f = (c) => [...svg.querySelectorAll(".k-key")].filter((k) => k.getAttribute("fill") === c).length;
  return { white: f("#f3f6f9"), ice: f("#6ed0ff"), bed: [...svg.querySelectorAll("rect")].filter((r) => r.getAttribute("fill") === "#f2a65a" && r.getAttribute("height") === "3").length }; });
const all = await latchedOf(N);
check(all.white >= 2 && all.ice >= 3 && /DRONES/.test(await page.textContent(`${N} .oc-keys .lg`)) && /ARP/.test(await page.textContent(`${N} .oc-keys .lg`)),
      `...its drones, its arpeggio on the keys, the legend all of it (${all.white} roots, ${all.ice} arpeggio notes)`);
await track(N, "glint");
const gl = await latchedOf(N), glg = await page.textContent(`${N} .oc-keys .lg`);
check(gl.white === 0 && gl.ice === all.ice && !/DRONES|BASS/.test(glg) && /ARP/.test(glg) && await page.$eval(`${N} .oc-kset .k-set .k-l`, (e) => e.textContent) === "GLINT ARP",
      "GLINT: its arpeggio alone on the keys and in the legend (no drone, no bass), GLINT ARP's settings under them");
const kb = await page.$eval(`${N} .oc-keys .k-kb`, (s) => { const k = [...s.querySelectorAll(".k-key")].map((e) => +e.dataset.n); return [k.length, Math.min(...k), Math.max(...k)]; });
check(kb[0] === 85 && kb[1] === 24 && kb[2] === 108, `the keyboard: C1 to C8 (${kb[0]} keys)`);
await track(N, "overview");
await page.click(`${N} .play`);
await page.waitForFunction((a) => document.querySelector(a).dataset.sound === "playing", N, { timeout: 30000 });
await sec(N, "LFO 1");
await page.waitForTimeout(1500);
check(await page.$eval(`${N} .lf-wide .lf-sc`, (s) => [...s.querySelectorAll("circle")].some((c) => +c.getAttribute("opacity") > 0.5)), "LFO 1 opened: its picture, wide, its glint moving");
check(await page.$eval(`${N} .oc-keys .k-kb`, (s) => [...s.querySelectorAll("rect.k-on")].filter((r) => +r.getAttribute("opacity") > 0.05).length) > 0, "THE KEYS lit by what sounds (OVERVIEW: all of it)");
await track(N, "bass");
await page.waitForTimeout(400);
check(await page.$eval(`${N} .oc-keys .k-kb`, (s) => { const lit = [...s.querySelectorAll("rect.k-on")].filter((r) => +r.getAttribute("opacity") > 0.05); return lit.every((r) => /^#[0-9a-f]{6}$/.test(r.getAttribute("fill")) && r.getAttribute("fill") !== "#dce3ea" && r.getAttribute("fill") !== "#6ed0ff"); }),
      "...BASS: the drones' and GLINT's lights gone from the keys, the bass's alone");
await page.click(`${N} .play`);
await page.waitForTimeout(300);

// ---------------------------------------------------------------- DRONE: a chord latched, let go
await track(N, "drone");
check(JSON.stringify(await page.$$eval(`${N} .oc-kset .k-cells .cell`, (c) => c.map((x) => +x.dataset.p))) === JSON.stringify([ID.P_ROOT, ID.P_SCALE, ID.P_CHORD, ID.P_VOICING]) &&
      await page.$eval(`${N} .oc-kset .k-set .k-l`, (e) => e.textContent) === "HARMONY", "DRONE: HARMONY's ROOT, SCALE, CHORD, VOICING under the keys");
check(await page.$eval(`${N} .oc-kset .k-cells .cell`, (c) => c.getAttribute("role") === "slider"), "...sliders, as the editor's");
const nb = b0(north), chords0 = nb[ST.nch];
const hp = await keyPt(N, 62);
await page.mouse.move(hp.x, hp.y);
await page.waitForTimeout(150);
const tag = await page.$eval(`${N} .oc-keys .k-tag text`, (t) => t.textContent).catch(() => "");
check(/^D4 · SEVENTH WIDE: /.test(tag) && (await page.$$(`${N} .oc-keys .k-ghost rect`)).length >= 3, `D4 hovered: its chord ghosted (${tag})`);
await page.mouse.click(hp.x, hp.y);
await page.waitForTimeout(300);
let c = await cur(north);
check(!!c && c[ST.nch] === chords0 + 1, `D4 clicked: its chord latched (the draft: ${c && c[ST.nch]} chords, was ${chords0})`);
check(/Its notes changed/.test(await page.textContent(`${N} .ed-band`)) && !!(await page.$(`${N} .oc-keys .k-title .ed-dot`)), "...EDITED: its notes changed, THE KEYS marked");
check(await page.$eval(`${N} .oc-keys [data-k="KEYS:drone"]`, (b) => b.classList.contains("on")), "...drawn again: THE KEYS still on DRONE");
await clickKey(N, 62);
check(!(await cur(north)) && !(await page.$(`${N} .ed-band`)), "again: let go, NORTH RIVER as it was (no draft)");

// ---------------------------------------------------------------- GLINT's arpeggio: a note out, in
await track(N, "glint");
const arp = nb.slice(ST.arp, ST.arp + nb[ST.narp]), oct = nb[ST.v + ID.P_GL_OCTAVE], hi = arp[arp.length - 1] + 12 * oct;
await clickKey(N, hi);
c = await cur(north);
check(!!c && c[ST.narp] === arp.length - 1, `GLINT, ARP: its highest note (${hi}) clicked, out of the arpeggio`);
await clickKey(N, hi);
check(!(await cur(north)), "...again: back in (no draft)");
await clickKey(N, 36 + 12 * oct);
c = await cur(north);
check(!!c && c[ST.narp] === arp.length + 1 && [...c.slice(ST.arp, ST.arp + c[ST.narp])].includes(36), "a new note: into the arpeggio");
await page.click(`${N} [data-act="undo"]`);
await page.waitForTimeout(300);
check(!(await cur(north)), "Undo: as it was");

// ---------------------------------------------------------------- HOLD: dim with nothing playing, a chord while down
await track(N, "drone");
await page.click(`${N} .oc-keys [data-k="KEY MODE:hold"]`);
await page.waitForTimeout(150);
check(await page.$eval(`${N} .oc-keys .k-kb`, (s) => s.classList.contains("is-dim")) && !(await page.$(`${N} .oc-kset .k-tools`)), "HOLD, nothing playing: the keys dim, no tools");
await clickKey(N, 72);
check(await page.$eval(`${N} .play`, (b) => b.classList.contains("nudge")) && !(await cur(north)), "...a key pressed: ▶ nudged, nothing changed");
await page.click(`${N} .play`);
await page.waitForFunction((a) => document.querySelector(a).dataset.sound === "playing", N, { timeout: 30000 });
await page.waitForTimeout(1500);
check(!(await page.$eval(`${N} .oc-keys .k-kb`, (s) => s.classList.contains("is-dim"))), "...playing: the keys lit");
const drones = () => page.evaluate(() => { const v = RiverPlayer.live(); return v ? v.slice(0, 8).filter((n, i) => n >= 0 && v[8 + i] > 0.2) : []; });
const before = await drones();
const p96 = await keyPt(N, 96);
await page.mouse.move(p96.x, p96.y);
await page.mouse.down();
await page.waitForTimeout(900);
const during = await drones();
await page.mouse.up();
await page.waitForTimeout(300);
check(during.some((n) => n >= 92) && !before.some((n) => n >= 92), `...C7 held: its chord sounds (${during.filter((n) => n >= 92).join(" ")})`);
check(!(await cur(north)), "...let go: nothing kept");
await page.click(`${N} .oc-keys [data-k="KEY MODE:latch"]`);
await page.waitForTimeout(150);

// ---------------------------------------------------------------- UNDO of what it plays, while it plays: the shard taken again
await page.evaluate(() => { window.__sc = 0; const f = RiverPlayer.scene; RiverPlayer.scene = (b) => { window.__sc++; return f(b); }; });
await clickKey(N, 50);
await page.waitForTimeout(600);
c = await cur(north);
const old = new Set(Array.from({ length: chords0 }, (_, i) => nb[ST.ch + 5 * i]));
const added = Array.from({ length: c ? c[ST.nch] : 0 }, (_, i) => c.slice(ST.ch + 5 * i, ST.ch + 5 * i + 5)).find((ch) => !old.has(ch[0]));
const heardNow = await drones();
check(!!added && added.slice(1).filter((n) => n < 128).some((n) => heardNow.includes(n)), `playing: a chord latched sounds at once (${added && added.slice(1).filter((n) => n < 128).join(" ")})`);
await page.click(`${N} [data-act="undo"]`);
await page.waitForTimeout(400);
check(await page.evaluate(() => window.__sc) === 1 && !(await cur(north)), "...Undo: the sound taken again (RiverPlayer.scene), the draft gone");
await page.click(`${N} .play`);
await page.waitForTimeout(300);

// ---------------------------------------------------------------- what it plays beside the controls; CHORD MODE; ZOOM; THE KEYS held at the window's bottom
await track(N, "bass");
check(/BASS/.test(await page.textContent(`${N} .oc-keys .k-top .lg`)) && !!(await page.$(`${N} .oc-keys [data-k^="KEY MODE"]`)) && !(await page.$(`${N} .oc-keys [data-k^="CHORD MODE"]`)),
      "BASS: what it plays in the controls' row, beside KEY MODE; no CHORD MODE");
await track(N, "drone");
const cmOn = (v) => page.$eval(`${N} .oc-keys [data-k="CHORD MODE:${v}"]`, (b) => b.classList.contains("on"));
const order = await page.$$eval(`${N} .oc-keys .k-top > *`, (e) => e.map((x) => x.querySelector(".k-seg") ? x.querySelector(".k-seg").getAttribute("aria-label") : x.className));
check(JSON.stringify(order.slice(2)) === JSON.stringify(["KEY MODE", "CHORD MODE", "lg k-legend"]), `DRONE: KEY MODE, CHORD MODE, then what it plays (${order.join(", ")})`);
check(nb[ST.v + ID.P_CHORD] === 3 && await cmOn("on"), "...CHORD MODE ON (HARMONY's CHORD: SEVENTH)");
await page.click(`${N} .oc-keys [data-k="CHORD MODE:off"]`);
await page.waitForTimeout(400);
c = await cur(north);
check(!!c && c[ST.v + ID.P_CHORD] === 0 && /SINGLE/.test(await page.textContent(`${N} .oc-kset .cell[data-p="${ID.P_CHORD}"]`)) && await cmOn("off"),
      "CHORD MODE OFF: HARMONY's CHORD at SINGLE");
await page.click(`${N} .oc-keys [data-k="CHORD MODE:on"]`);
await page.waitForTimeout(400);
c = await cur(north);
check((!c || c[ST.v + ID.P_CHORD] === 3) && /SEVENTH/.test(await page.textContent(`${N} .oc-kset .cell[data-p="${ID.P_CHORD}"]`)) && await cmOn("on"),
      "...ON: back to the chord it was (SEVENTH)");
for (let i = 0; i < 3 && await cur(north); i++) { await page.click(`${N} [data-act="undo"]`); await page.waitForTimeout(300); }
check(!(await cur(north)), "...Undo: NORTH RIVER as it was");
const zb = () => page.$eval(`${N} .oc-keys .k-board`, (b) => {
  const st = b.parentElement.querySelector(".k-strip"), svg = b.querySelector("svg.k-kb");
  return { z: +b.style.getPropertyValue("--kz"), sw: b.scrollWidth, cw: b.clientWidth, sl: Math.round(b.scrollLeft), h: Math.round(svg.getBoundingClientRect().height),
           can: st.classList.contains("can"), minus: st.querySelector('[data-k="zoom:-"]').getAttribute("aria-disabled") === "true",
           plus: st.querySelector('[data-k="zoom:+"]').getAttribute("aria-disabled") === "true",
           thumb: parseFloat(st.querySelector(".k-thumb").style.width) };
});
let z = await zb();
const h1 = z.h;
check(z.z === 1 && !z.can && z.minus && !z.plus, "ZOOM: the keys as wide as the card, nothing to scroll, - off");
await page.click(`${N} .oc-keys [data-k="zoom:+"]`);
await page.click(`${N} .oc-keys [data-k="zoom:+"]`);
await page.waitForTimeout(200);
z = await zb();
check(z.z === 2 && z.sw > z.cw * 1.9 && z.can && Math.abs(z.sl - (z.sw - z.cw) / 2) < 30 && Math.abs(z.h - h1) <= 1 && z.thumb > 45 && z.thumb < 55,
      `+ twice: the keys twice as wide, as tall (${h1} → ${z.h}), from the middle (${z.sl}), the strip's window half of it (${z.thumb}%)`);
await clickKey(N, 62);
c = await cur(north);
check(!!c && c[ST.nch] === chords0 + 1, "...zoomed, D4 clicked: its chord latched");
await page.click(`${N} [data-act="undo"]`);
await page.waitForTimeout(400);
{                                                                        // the strip's window dragged: the keys go with it
  const t = await page.$eval(`${N} .oc-keys .k-thumb`, (e) => { e.scrollIntoView({ block: "center" }); const r = e.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
  const s0 = (await zb()).sl;
  await page.mouse.move(t.x, t.y);
  await page.mouse.down();
  await page.mouse.move(t.x - 200, t.y, { steps: 6 });
  await page.mouse.up();
  const s1 = (await zb()).sl;
  check(s1 < s0 - 150, `...the strip's window dragged left: the keys too (${s0} → ${s1})`);
}
await page.$eval(`${N} .oc-keys .k-board`, (b) => { const r = b.getBoundingClientRect();
  b.dispatchEvent(new WheelEvent("wheel", { ctrlKey: true, deltaY: 60, clientX: r.left + r.width / 2, clientY: r.top + 40, bubbles: true, cancelable: true })); });
await page.waitForTimeout(150);
z = await zb();
check(z.z < 2 && z.z >= 1, `a trackpad's pinch (Ctrl and the wheel): out (${z.z})`);
{                                                                        // zoomed in all the way: the keys' cursor kept in sight
  for (let i = 0; i < 4; i++) await page.click(`${N} .oc-keys [data-k="zoom:+"]`);
  check(await page.evaluate(() => document.activeElement && document.activeElement.dataset.k) === "zoom:+" && (await zb()).plus,
        "...+ up to its end: + off, the focus still on it");
  await page.focus(`${N} .oc-keys .k-board`);
  for (let i = 0; i < 40; i++) await page.keyboard.press("ArrowRight");
  const inView = await page.$eval(`${N} .oc-keys .k-board`, (b) => { const k = b.querySelector(".k-ghost rect"), rb = b.getBoundingClientRect(), r = k && k.getBoundingClientRect();
    return !!r && r.left >= rb.left - 1 && r.right <= rb.right + 1; });
  check(inView, "...the keys' cursor moved far right with the arrows: the keys scroll to keep it in sight");
  await page.keyboard.press("Escape");
}
for (let i = 0; i < 6; i++) await page.click(`${N} .oc-keys [data-k="zoom:-"]`).catch(() => {});
z = await zb();
check(z.z === 1 && !z.can && z.minus, "- down to the whole keyboard: - off again");
{                                                                        // the card's top on the screen: THE KEYS at the window's bottom
  await page.evaluate((a) => { const r = document.querySelector(a).getBoundingClientRect(); scrollBy(0, r.top - 60); }, N);
  await page.waitForTimeout(300);
  const dk = await page.$eval(`${N} .oc-keys`, (e) => ({ on: e.classList.contains("is-stuck"), b: Math.round(e.getBoundingClientRect().bottom), ih: innerHeight }));
  check(dk.on && dk.b === dk.ih, `the card's top on the screen: THE KEYS held at the window's bottom (${dk.b} of ${dk.ih})`);
  const k = await page.evaluate((a) => { const e = document.querySelector(`${a} .oc-keys .k-key[data-n="62"]`), r = e.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height * 0.86 }; }, N);
  const colsTop = () => page.$eval(`${N} .oc-cols`, (e) => Math.round(e.getBoundingClientRect().top));
  const t0 = await page.$eval(`${N} .oc-keys .k-board`, (b) => Math.round(b.getBoundingClientRect().top)), o0 = await colsTop();
  await page.mouse.click(k.x, k.y);
  await page.waitForTimeout(400);
  c = await cur(north);
  const t1 = await page.$eval(`${N} .oc-keys .k-board`, (b) => Math.round(b.getBoundingClientRect().top)), o1 = await colsTop();
  check(!!c && c[ST.nch] === chords0 + 1 && Math.abs(o1 - o0) < 2 && Math.abs(t1 - t0) < 2 && await page.$eval(`${N} .oc-keys`, (e) => e.classList.contains("is-stuck")),
        `...a key clicked there: latched, EDITED above, the keys and what is behind them where they were (${o0} → ${o1}, ${t0} → ${t1})`);
  await page.click(`${N} [data-act="undo"]`);
  await page.waitForTimeout(400);
  const kTop = () => page.$eval(`${N} .oc-keys [data-k="KEYS:drone"]`, (b) => Math.round(b.getBoundingClientRect().top));
  const k0 = await kTop();
  await track(N, "glint");
  const k1 = await kTop();
  await track(N, "drone");
  const k2 = await kTop();
  check(Math.abs(k1 - k0) < 2 && Math.abs(k2 - k0) < 2, `...held there, DRONE → GLINT → DRONE: KEYS where they were (${k0}, ${k1}, ${k2})`);
  await page.$eval(`${N} .oc-kset`, (e) => e.scrollIntoView({ block: "end" }));
  await page.waitForTimeout(300);
  check(!(await page.$eval(`${N} .oc-keys`, (e) => e.classList.contains("is-stuck"))), "...scrolled down to them: in their place, their settings under them");
}

// ---------------------------------------------------------------- GLINT's grid (GLACIER)
const G = `#m-${glacier.id}`;
await open(G);
await track(G, "glint");
check(await page.$eval(`${G} .oc-keys [data-k="KEYS:glint"]`, (b) => b.classList.contains("on")) && !!(await page.$(`${G} .oc-keys .k-board.is-grid .k-grid`)),
      "GLACIER, GLINT on SEQ: its grid, not the keyboard");
check(/CLICK A NOTE: IT GOES/.test(await page.textContent(`${G} .oc-keys .k-how`)), "...one terse line for the gestures that can't be seen");
{                                                                        // a note placed, then clicked at its end: it goes
  const w0 = await gridAt(G), s0 = seqOf(b0(glacier)), i = s0.findIndex((b, k) => k > 0 && k < 15 && (b & 127) === 0 && (s0[k + 1] & 127) === 0);
  let q = await gridPt(G, i, w0.lo + 6);
  const top0 = await page.$eval(`${G} .oc-keys .k-board`, (b) => b.getBoundingClientRect().top);
  await page.mouse.click(q.x, q.y);
  await page.waitForTimeout(300);
  const top1 = await page.$eval(`${G} .oc-keys .k-board`, (b) => b.getBoundingClientRect().top);
  check(!!(await cur(glacier)) && Math.abs(top1 - top0) < 1, `a note placed: the grid stays where it was on the screen (${Math.round(top0)} → ${Math.round(top1)})`);
  q = await gridPt(G, i, w0.lo + 6, true);
  await page.mouse.click(q.x, q.y);
  await page.waitForTimeout(300);
  check(!(await cur(glacier)), "...clicked again, at its end: it goes");
}
{                                                                        // a note placed near the top: it stays under the pointer
  const w0 = await gridAt(G), s0 = seqOf(b0(glacier)), i = s0.findIndex((b, k) => k < 15 && (b & 127) === 0);
  let q = await gridPt(G, i, w0.hi - 1);
  await page.mouse.click(q.x, q.y);
  await page.waitForTimeout(400);
  const w1 = await gridAt(G), at = await page.evaluate(([art, i]) => {                  // (where the note is now)
    const b = document.querySelector(`${art} .oc-keys .k-board`), a = b._draw._at, svg = b._draw.querySelector("svg"), r = svg.getBoundingClientRect();
    const bar = a.bars.find((x) => x.i === i);
    return bar ? r.top + ((a.hi - bar.note) * 13 + 18 + 6.5) * r.height / svg.viewBox.baseVal.height : null;
  }, [G, i]);
  check(w1.hi > w0.hi && at != null && Math.abs(at - q.y) < 3, `a note placed near the grid's top: rows added over it, the note still under the pointer (${Math.round(q.y)} → ${Math.round(at)})`);
  await page.mouse.click(q.x, q.y);
  await page.waitForTimeout(400);
  check(!(await cur(glacier)), "...clicked again, where it was: it goes");
}
check(!(await page.$(`${G} .sh-bass .strip`)) && /NO SEQUENCE · NOTHING HELD/.test(await page.textContent(`${G} .sh-bass`)), "its BASS at the left: NO SEQUENCE · NOTHING HELD");
const gb = b0(glacier), goct = gb[ST.v + ID.P_GL_OCTAVE], g0 = seqOf(gb), win = await gridAt(G);
const free = g0.findIndex((b, i) => i < 15 && (b & 127) === 0 && (g0[i + 1] & 127) === 0);
const row = win.lo + 4;
let p = await gridPt(G, free, row);
await page.mouse.move(p.x, p.y);
await page.waitForTimeout(120);
check(new RegExp(`^STEP ${free + 1} · `).test(await page.$eval(`${G} .oc-keys .k-ghost text`, (t) => t.textContent).catch(() => "")), `step ${free + 1} hovered: its tag`);
await page.mouse.click(p.x, p.y);
await page.waitForTimeout(300);
c = await cur(glacier);
check(!!c && (c[ST.seq + free] & 127) === row - 12 * goct, `a cell clicked: a note on step ${free + 1} (${row - 12 * goct})`);
p = await gridPt(G, free, row, true);
const p2 = await gridPt(G, free + 1, row, true);
await page.mouse.move(p.x, p.y);
await page.mouse.down();
for (let i = 1; i <= 6; i++) await page.mouse.move(p.x + (p2.x - p.x) * i / 6, p.y);
await page.mouse.up();
await page.waitForTimeout(300);
c = await cur(glacier);
check(c[ST.seq + free + 1] === SCHEMA.seq.tie, "its end dragged a step on: a TIE");
p = await gridPt(G, free, row);
const up2 = await gridPt(G, free, row + 2);
await page.mouse.move(p.x, p.y);
await page.mouse.down();
for (let i = 1; i <= 6; i++) await page.mouse.move(p.x, p.y + (up2.y - p.y) * i / 6);
await page.mouse.up();
await page.waitForTimeout(300);
c = await cur(glacier);
check((c[ST.seq + free] & 127) === row + 2 - 12 * goct && c[ST.seq + free + 1] === SCHEMA.seq.tie, "dragged up two rows: another note, its TIE kept");
p = await gridPt(G, free, row + 2);
await page.keyboard.down("Shift");
await page.mouse.click(p.x, p.y);
await page.keyboard.up("Shift");
await page.waitForTimeout(300);
c = await cur(glacier);
check((c[ST.seq + free] & SCHEMA.seq.accent) !== 0, "Shift-click: its ACCENT");
await page.mouse.click(p.x, p.y);
await page.waitForTimeout(300);
c = await cur(glacier);
check(!c || (seqOf(c).every((b, i) => b === g0[i])), "clicked: it goes, its TIE too (the sequence as it was)");
const lenP = await page.evaluate((art) => {                              // LENGTH's grip, dragged two steps back
  const b = document.querySelector(`${art} .oc-keys .k-board`), a = b._draw._at, svg = b._draw.querySelector("svg"), r = svg.getBoundingClientRect();
  const X = (i) => r.left + (72 + i * a.cw) * r.width / 1200, y = r.top + 7 * r.height / svg.viewBox.baseVal.height;
  return { x: X(a.len), y, to: X(a.len - 2) };
}, G);
await page.mouse.move(lenP.x, lenP.y);
await page.mouse.down();
for (let i = 1; i <= 8; i++) await page.mouse.move(lenP.x + (lenP.to - lenP.x) * i / 8, lenP.y);
await page.mouse.up();
await page.waitForTimeout(300);
c = await cur(glacier);
check(!!c && c[ST.v + ID.P_SQ_LEN] === gb[ST.v + ID.P_SQ_LEN] - 2, `LENGTH's grip dragged: SEQ LENGTH ${gb[ST.v + ID.P_SQ_LEN] + 1} → ${c && c[ST.v + ID.P_SQ_LEN] + 1}`);
{                                                                        // LENGTH past 16: the grid's 32 steps
  const grip = () => page.evaluate((art) => {
    const b = document.querySelector(`${art} .oc-keys .k-board`), d = b._draw, a = d._at, svg = d.querySelector("svg"), r = svg.getBoundingClientRect(), bb = b.getBoundingClientRect();
    const X = (i) => r.left + (72 + i * a.cw) * r.width / 1200, y = r.top + 7 * r.height / svg.viewBox.baseVal.height;
    return { x: X(a.len), y, past: bb.right + 24, X19: X(19) };
  }, G);
  let q = await grip();
  await page.mouse.move(q.x, q.y);
  await page.mouse.down();
  for (let i = 1; i <= 6; i++) await page.mouse.move(q.x + (q.past - q.x) * i / 6, q.y);
  await page.mouse.up();
  await page.waitForTimeout(300);
  const mid = await gridAt(G);
  q = await grip();
  await page.mouse.move(q.x, q.y);
  await page.mouse.down();
  for (let i = 1; i <= 6; i++) await page.mouse.move(q.x + (q.X19 - q.x) * i / 6, q.y);
  await page.mouse.up();
  await page.waitForTimeout(300);
  c = await cur(glacier);
  check(mid.n === 32 && mid.len === 17 && (await gridAt(G)).n === 32 && c[ST.v + ID.P_SQ_LEN] === 18,
        `LENGTH's grip past the grid's end: 17, the grid's 32 steps; dragged on: ${c[ST.v + ID.P_SQ_LEN] + 1}`);
}
await page.click(`${G} [data-act="revert"]`);
await page.waitForSelector("dialog[open]");
check(/The changes go \(one setting\)/.test(await page.textContent("dialog[open]")), "Revert: asked (one setting)");
await page.click("dialog[open] .btn-ice");
await page.waitForTimeout(300);

// ---------------------------------------------------------------- the bass's steps (AURORA)
const A = `#m-${aurora.id}`;
{                                                                        // a key clicked: the keys stay where they were
  await track(N, "drone");
  await page.evaluate((a) => { const k = document.querySelector(`${a} .oc-keys .k-board`); scrollTo(0, k.getBoundingClientRect().top + scrollY - 300); }, N);
  await page.waitForTimeout(300);
  const t0 = await page.$eval(`${N} .oc-keys .k-board`, (b) => b.getBoundingClientRect().top);
  const kp = await page.evaluate((a) => { const r = document.querySelector(`${a} .oc-keys .k-key[data-n="67"]`).getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height * 0.86 }; }, N);
  await page.mouse.click(kp.x, kp.y);
  await page.waitForTimeout(400);
  const t1 = await page.$eval(`${N} .oc-keys .k-board`, (b) => b.getBoundingClientRect().top);
  await page.mouse.click(kp.x, kp.y);
  await page.waitForTimeout(400);
  const t2 = await page.$eval(`${N} .oc-keys .k-board`, (b) => b.getBoundingClientRect().top);
  check(Math.abs(t1 - t0) < 1 && Math.abs(t2 - t0) < 1 && !(await cur(north)), `a key clicked, then again: the keys don't move on the screen (${[t0, t1, t2].map(Math.round).join(" → ")})`);
}
await open(A);
await track(A, "bass");
const ab = b0(aurora), boct = 12 * (ab[ST.v + ID.P_BS_OCTAVE] - 2), steps = ab[ST.bq + 1];
check((await page.$$(`${A} .oc-keys .k-steps .k-step`)).length === 16 && await page.$eval(`${A} .oc-kset .k-set .k-l`, (e) => e.textContent) === "BASS SEQ",
      "AURORA, BASS: its sixteen steps over the keys, BASS SEQ's settings under them");
check(await page.$$eval(`${A} .oc-keys .k-steps .k-step.out`, (e) => e.length) === 15 - steps, `...the steps past SEQ STEPS (${steps + 1}) dim`);
await page.click(`${A} .oc-keys [data-k="step:2"]`);
await page.waitForTimeout(150);
check(await page.$eval(`${A} .oc-keys [data-k="step:2"]`, (b) => b.classList.contains("sel")), "step 3 clicked: selected");
const k3 = [41, 43, 48].find((n) => n - boct !== ab[ST.bnote + 2]);
await clickKey(A, k3);
c = await cur(aurora);
check(!!c && c[ST.bnote + 2] === k3 - boct && await page.$eval(`${A} .oc-keys [data-k="step:3"]`, (b) => b.classList.contains("sel")),
      `a key clicked (${k3}): into step 3 (${k3 - boct} as kept), AUTO NEXT STEP: step 4 selected`);
await page.click(`${A} .oc-kset [data-k="next:OFF"]`);
await page.waitForTimeout(150);
await clickKey(A, 47);
c = await cur(aurora);
check(c[ST.bnote + 3] === 47 - boct && await page.$eval(`${A} .oc-keys [data-k="step:3"]`, (b) => b.classList.contains("sel")), "AUTO NEXT STEP OFF: B2 into step 4, step 4 stays selected");
await page.click(`${A} .oc-kset [data-k="cut"]`);
await page.waitForTimeout(250);
check((await cur(aurora))[ST.bnote + 3] === SCHEMA.bseq.cut, "CUT: into step 4");
await page.click(`${A} .oc-kset [data-k="hold"]`);
await page.waitForTimeout(250);
check((await cur(aurora))[ST.bnote + 3] === SCHEMA.bseq.hold, "HOLD: into step 4");
await page.click(`${A} .oc-keys [data-k="step:12"]`);
await page.waitForTimeout(150);
await clickKey(A, 40);
c = await cur(aurora);
check(c[ST.bnote + 12] === 40 - boct && c[ST.bq + 1] === 12, `step 13, past SEQ STEPS: written, SEQ STEPS taking it in (${c[ST.bq + 1] + 1})`);
await page.click(`${A} .oc-kset [data-k="next:ON"]`);
await page.click(`${A} .oc-keys [data-k="step:12"]`);
await page.waitForTimeout(150);
await clickKey(A, 41);
check(await page.$eval(`${A} .oc-keys [data-k="step:0"]`, (b) => b.classList.contains("sel")), "AUTO NEXT STEP on the last step: round to step 1, as the FM-1's STEP record goes");
await page.click(`${A} .oc-keys [data-k="step:12"]`);
await page.waitForTimeout(150);
const lenCell = `${A} .oc-kset .cell[data-p="${ID.P_BQ_LEN}"]`;
const lb = await page.$eval(lenCell, (e) => { e.scrollIntoView({ block: "center" }); const r = e.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
await page.mouse.move(lb.x, lb.y);
await page.mouse.down();
for (let i = 1; i <= 8; i++) await page.mouse.move(lb.x + i * 9, lb.y + 1);
await page.mouse.up();
await page.waitForTimeout(300);
c = await cur(aurora);
check(c[ST.blen + 12] > ab[ST.blen + 12] && c.slice(ST.blen, ST.blen + 12).every((x, i) => x === ab[ST.blen + i]), `STEP LENGTH dragged: step 13's alone (${c[ST.blen + 12]})`);

// ---------------------------------------------------------------- SAVE AS keeps what it plays
await page.click(`${A} [data-act="save-as"]`);
await page.waitForSelector("dialog.ed-sheet[open]");
await page.click("dialog.ed-sheet .btn-ice");
await page.waitForFunction(() => !document.querySelector("dialog.ed-sheet[open]"));
await page.waitForTimeout(400);
const ii = (await api(page, "get", "/api/shards")).items.find((x) => x.name === "AURORA II");
const iib = ii ? Array.from(Buffer.from(ii.data, "base64")) : [];
check(!!ii && iib[ST.bnote + 2] === k3 - boct && iib[ST.bnote + 3] === SCHEMA.bseq.hold && iib[ST.bnote + 12] === 41 - boct && iib[ST.bq + 1] === 12,
      "SAVE AS: AURORA II has its steps as written");

// ---------------------------------------------------------------- a phone
const { page: ph } = await person("phone", null, { width: 390, height: 844 }, { deviceScaleFactor: 2, isMobile: true, hasTouch: true });
await ph.context().addCookies(await page.context().cookies());
await ph.goto(BASE + "/yours/");
await ph.waitForSelector(N);
await ph.click(`${N} .sr-open`);
await ph.waitForSelector(`${N}.open .oc-keys`);
await ph.waitForTimeout(500);
const w = await ph.evaluate(() => document.documentElement.scrollWidth);
const sw = await ph.$eval(`${N} .oc-keys .k-board`, (b) => [b.scrollWidth, b.clientWidth, b.scrollLeft]);
check(w <= 390 && sw[0] > sw[1] && sw[2] > 0, `phone: no horizontal page scroll (${w}), the keys scroll inside, opened where the notes are (${sw.join(" ")})`);
{                                                                        // a finger: a swipe scrolls, a tap latches
  const cdp = await ph.context().newCDPSession(ph);
  const swipe = async (x, y, dx, dy) => {
    await cdp.send("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [{ x, y }] });
    for (let i = 1; i <= 10; i++) { await cdp.send("Input.dispatchTouchEvent", { type: "touchMove", touchPoints: [{ x: x + dx * i / 10, y: y + dy * i / 10 }] }); await sleep(16); }
    await cdp.send("Input.dispatchTouchEvent", { type: "touchEnd", touchPoints: [] });
    await ph.waitForTimeout(400);
  };
  const ndrafts = () => ph.evaluate(() => RiverEdit.drafts.size);
  await ph.tap(`${N} .oc-keys [data-k="KEYS:drone"]`);
  await ph.waitForTimeout(300);
  const onKey = () => ph.evaluate((art) => {
    const b = document.querySelector(`${art} .oc-keys .k-board`);
    b.scrollIntoView({ block: "center" });
    const k = [...b.querySelectorAll(".k-key")].find((e) => { const r = e.getBoundingClientRect(); return +e.getAttribute("height") > 100 && r.left > 80 && r.right < 300; });
    const r = k.getBoundingClientRect();
    return { x: r.left + r.width / 2, y: r.top + r.height * 0.86, n: +k.dataset.n, sl: b.scrollLeft };
  }, N);
  let k = await onKey();
  await swipe(k.x, k.y, -180, 0);
  const sl = await ph.$eval(`${N} .oc-keys .k-board`, (b) => b.scrollLeft);
  check(await ndrafts() === 0 && sl !== k.sl, `phone: a swipe across the keys scrolls them (${k.sl} → ${sl}), nothing latched`);
  k = await onKey();
  await ph.touchscreen.tap(k.x, k.y);
  await ph.waitForTimeout(500);
  const sl2 = await ph.$eval(`${N} .oc-keys .k-board`, (b) => b.scrollLeft);
  check(await ndrafts() === 1 && Math.abs(sl2 - k.sl) < 2, `...a tap latches its chord, the keys stay where they were (${k.sl} → ${sl2})`);
  check(await ph.$eval(`${N} .oc-keys`, (e) => getComputedStyle(e).position) !== "sticky", "phone: THE KEYS stay in their place (not held at the bottom)");
  {                                                                      // two fingers spread on the keys: wider, nothing latched
    await ph.evaluate(() => { RiverEdit.drafts.clear(); sessionStorage.clear(); });
    await ph.goto(BASE + "/yours/");
    await ph.waitForSelector(N);
    await ph.tap(`${N} .sr-open`);
    await ph.waitForSelector(`${N}.open .oc-keys`);
    await ph.tap(`${N} .oc-keys [data-k="KEYS:drone"]`);
    await ph.evaluate(() => RiverEdit.ready());
    await ph.waitForTimeout(400);
    const r = await ph.$eval(`${N} .oc-keys .k-board`, (b) => { b.scrollIntoView({ block: "center" }); const q = b.getBoundingClientRect(); return { x: q.left + q.width / 2, y: q.top + q.height * 0.75 }; });
    const z0 = await ph.$eval(`${N} .oc-keys .k-board`, (b) => +b.style.getPropertyValue("--kz") || 1), n0 = await ndrafts();
    await cdp.send("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [{ x: r.x - 30, y: r.y, id: 1 }, { x: r.x + 30, y: r.y, id: 2 }] });
    for (let i = 1; i <= 10; i++) {
      await cdp.send("Input.dispatchTouchEvent", { type: "touchMove", touchPoints: [{ x: r.x - 30 - 6 * i, y: r.y, id: 1 }, { x: r.x + 30 + 6 * i, y: r.y, id: 2 }] });
      await sleep(20);
    }
    await cdp.send("Input.dispatchTouchEvent", { type: "touchEnd", touchPoints: [] });
    await ph.waitForTimeout(400);
    const z1 = await ph.$eval(`${N} .oc-keys .k-board`, (b) => +b.style.getPropertyValue("--kz"));
    check(z1 > z0 * 1.5 && await ndrafts() === n0, `...two fingers spread on the keys: wider (${z0} → ${z1.toFixed(2)}), nothing latched`);
    const q = await ph.$eval(`${N} .oc-keys .k-board`, (b) => { const k = [...b.querySelectorAll(".k-key")].find((e) => { const r = e.getBoundingClientRect(); return +e.getAttribute("height") > 100 && r.left > 60 && r.right < 330; });
      const r = k.getBoundingClientRect(), rb = b.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height * 0.86, top: rb.top + 12 }; });
    await cdp.send("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [{ x: q.x, y: q.y, id: 1 }] });
    await cdp.send("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [{ x: q.x, y: q.y, id: 1 }, { x: q.x + 120, y: q.top, id: 2 }] });
    for (let i = 1; i <= 8; i++) { await cdp.send("Input.dispatchTouchEvent", { type: "touchMove", touchPoints: [{ x: q.x, y: q.y, id: 1 }, { x: q.x + 120 - 10 * i, y: q.top, id: 2 }] }); await sleep(20); }
    await cdp.send("Input.dispatchTouchEvent", { type: "touchEnd", touchPoints: [{ x: q.x, y: q.y, id: 1 }] });
    await cdp.send("Input.dispatchTouchEvent", { type: "touchEnd", touchPoints: [] });
    await ph.waitForTimeout(400);
    check(await ndrafts() === n0, "...a pinch, one finger still on a key, the other above the keys: nothing latched");
    k = await onKey();
    await ph.touchscreen.tap(k.x, k.y);
    await ph.waitForTimeout(500);
    check(await ndrafts() === n0 + 1, "...then a tap: latched (the keys answer after a pinch)");
    await ph.evaluate(() => { RiverEdit.drafts.clear(); sessionStorage.clear(); });
  }
  await ph.evaluate(() => { RiverEdit.drafts.clear(); sessionStorage.clear(); });
  const GP = `#m-${glacier.id}`;
  await ph.goto(BASE + "/yours/");
  await ph.waitForSelector(GP);
  await ph.tap(`${GP} .sr-open`);
  await ph.waitForSelector(`${GP}.open .oc-keys`);
  await ph.tap(`${GP} .oc-keys [data-k="KEYS:glint"]`);
  await ph.waitForSelector(`${GP}.open .oc-keys .k-grid`);
  await ph.evaluate(() => RiverEdit.ready());
  await ph.waitForTimeout(300);
  const cellPt = (end) => ph.evaluate(([art, end]) => {
    const b = document.querySelector(`${art} .oc-keys .k-board`), a = b._draw._at, svg = b._draw.querySelector("svg");
    b.scrollIntoView({ block: "center" });
    const r = svg.getBoundingClientRect(), H = svg.viewBox.baseVal.height, bar = a.bars.find((x) => x.note >= a.lo && x.note <= a.hi && x.i < 3 && x.j < 4);
    const free = end ? null : a.n - 1;
    const i = end ? bar.j - 0.15 : 0.5, note = end ? bar.note : a.lo + 1;
    return { x: r.left + (72 + i * a.cw) * r.width / 1200, y: r.top + ((a.hi - note) * 13 + 24.5) * r.height / H, cw: a.cw * r.width / 1200, bar };
  }, [GP, end]);
  let gp = await cellPt(false);
  await swipe(gp.x, gp.y, 0, -200);
  check(await ndrafts() === 0, "phone, GLINT's grid: a swipe from an empty cell scrolls the page, nothing placed");
  gp = await cellPt(true);
  if (gp.bar) {
    const body = { x: gp.x - gp.cw * 0.6, y: gp.y };
    const g0 = await ph.$eval(`${GP} .oc-keys .k-board`, (b) => b.scrollLeft);
    await swipe(body.x, body.y, -160, 0);
    const g1 = await ph.$eval(`${GP} .oc-keys .k-board`, (b) => b.scrollLeft);
    check(await ndrafts() === 0 && g1 > g0, `...a finger across a note's body: the grid scrolls (${g0} → ${g1}), the note left as it was`);
    await ph.$eval(`${GP} .oc-keys .k-board`, (b, x) => { b.scrollLeft = x; }, g0);
    await ph.waitForTimeout(250);
    await swipe(body.x, body.y, 0, -40);
    const up = await ph.evaluate((id) => { const x = RiverEdit.drafts.get("shard:" + id); return x ? Array.from(x.cur) : null; }, glacier.id);
    check(!!up && (up[ST.seq + gp.bar.i] & 127) > (b0(glacier)[ST.seq + gp.bar.i] & 127), `...a finger dragging a note up: another note (${b0(glacier)[ST.seq + gp.bar.i] & 127} → ${up && up[ST.seq + gp.bar.i] & 127})`);
    await ph.evaluate(() => { RiverEdit.drafts.clear(); sessionStorage.clear(); });
    await ph.goto(BASE + "/yours/");
    await ph.waitForSelector(GP);
    await ph.tap(`${GP} .sr-open`);
    await ph.waitForSelector(`${GP}.open .oc-keys`);
    await ph.tap(`${GP} .oc-keys [data-k="KEYS:glint"]`);
    await ph.waitForSelector(`${GP}.open .oc-keys .k-grid`);
    await ph.evaluate(() => RiverEdit.ready());
    await ph.waitForTimeout(300);
    await ph.evaluate((art) => document.querySelector(`${art} .oc-keys .k-board`).scrollIntoView({ block: "center" }), GP);
    gp = await cellPt(true);
    await swipe(gp.x, gp.y, gp.cw * 1.1, 0);
    const d = await ph.evaluate((id) => { const x = RiverEdit.drafts.get("shard:" + id); return x ? Array.from(x.cur) : null; }, glacier.id);
    check(!!d && d[ST.seq + gp.bar.j] === SCHEMA.seq.tie, `...a finger dragging a note's end: longer (a TIE on step ${gp.bar.j + 1})`);
  } else check(false, "...GLACIER: a note in its first steps, to drag");
}
await ph.screenshot({ path: path.join(OUT, "keys-390.png") });
await page.screenshot({ path: path.join(OUT, "keys-1440.png") });

check(!errors.length, "no console errors" + (errors.length ? ": " + errors.slice(0, 4).join(" / ") : ""));
await browser.close();
console.log(fails ? `river_keys: ${fails} FAILED` : "river_keys: all ok");
process.exit(fails ? 1 : 0);
