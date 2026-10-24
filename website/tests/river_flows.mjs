// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The River's flows in Chrome against a simulated FM-1 (fake_fm1.mjs: a fake Web MIDI injected before the page):
//   Tools is Quick Backup (at /tools/ and /backup/): its dock (BACK UP THE FM-1 | OPEN A FILE), a backup, its source line
//   and its own actions (Download the backup, Restore it all…), the sixteen slots as numbered rows, the settings last,
//   the terminal note; the selection (Send to FM-1, Download);
//   SEND TO FM-1, every state: reading (by itself), not connected, FRASIL not installed, an FM-1 without the bass's
//   record (refused), ready (the cards, their slot pickers, a drag onto a slot, Back where they came from, the Send
//   button counting what is placed), a slot changed since read, the confirmation (the slots replaced), sending (CC 120
//   before the restore), the FM-1's question (FX and SEL only lit), done, not sent (FX), something held (STOP FIRST);
//   Restore it all (held, FX, yes); a package, a bad file, an earlier FRASIL's backup; a row opened in place and its ▶
//   with FRASIL's engine (emu/, the page's js/river-player.js), and with no engine; phones with no horizontal scroll.
//   the River at RIVER_URL (default its service at localhost:8050: server/README.md, RIVER_STATIC=build/website/river;
//   the assembled site served by website/serve.py is enough), then
//   PLAYWRIGHT=/path/to/playwright/index.mjs node website/tests/river_flows.mjs [OUT_DIR]
// (Playwright with its Chromium; OUT_DIR gets the files, default a temporary folder; SHOTS the screenshots, at 1440
// and 390, named as design/river-v2's mockups, default OUT_DIR)
import fs from "fs";
import os from "os";
import path from "path";
import { fileURLToPath } from "url";
import { FAKE } from "./fake_fm1.mjs";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");

const OUT = process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), "river-"));
const SHOTS = process.env.SHOTS || OUT;
fs.mkdirSync(`${OUT}/rv`, { recursive: true });
fs.mkdirSync(SHOTS, { recursive: true });
const URL = process.env.RIVER_URL || "http://localhost:8050/";
const QB = URL + "tools/";
const REPO = path.join(path.dirname(fileURLToPath(import.meta.url)), "../..");
/* the player's script: the site's, or the repository's while the site doesn't carry it */
const withPlayer = (pg) => pg.route("**/assets/js/river-player.js*", async (route) => {
  const r = await route.fetch().catch(() => null);
  if (r && r.ok()) return route.fulfill({ response: r });
  return route.fulfill({ status: 200, contentType: "text/javascript", body: fs.readFileSync(path.join(REPO, "website/assets/js/river-player.js")) });
});
let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "  ok" : "FAIL"} ${msg}`); if (!ok) fails++; };

const browser = await chromium.launch({ channel: "chrome" });
const errors = [];
const DESK = { viewport: { width: 1440, height: 900 } }, PHONE = { viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, hasTouch: true, isMobile: true };
async function open(o, name) {
  const ctx = await browser.newContext({ ...o, acceptDownloads: true });
  await ctx.addInitScript(FAKE);
  const page = await ctx.newPage();
  page.on("console", (m) => { if ((m.type() === "error" || m.type() === "warning") && !/\/api\/auth\/session|status of 404/.test(m.text() + m.location().url)) errors.push(`${name}: ${m.type()}: ${m.text()}`); });
  page.on("pageerror", (e) => { console.log(`    ${name} pageerror: ${e.message}`); errors.push(`${name} pageerror: ${e.message}`); });
  return { ctx, page };
}
const shot = async (page, name, w, full = false) => {
  await page.mouse.move(2, 2);
  await page.waitForTimeout(500);
  await page.screenshot({ path: `${SHOTS}/${name}-${w}.png`, fullPage: full });
};
const example = (page) => page.evaluate(async () => { window.__fm1.image = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()); });
const fm1 = (page, o) => page.evaluate((o) => Object.assign(window.__fm1, o), o);
const sayTitle = (page, dlg = "#place") => page.$eval(`${dlg} .dlg-say h3`, (e) => e.textContent).catch(() => "");
const waitTitle = async (page, re, dlg = "#place") => {
  try { await page.waitForFunction(([dlg, re]) => new RegExp(re).test(document.querySelector(`${dlg} .dlg-say h3`)?.textContent || ""), [dlg, re.source], { timeout: 30000 }); }
  catch (e) { console.log(`    (waiting for ${re} in ${dlg}: ${await sayTitle(page, dlg)}; the FM-1: ${await page.evaluate(() => window.__fm1.events.join(","))})`); throw e; }
};
const lit = (page, dlg = "#place") => page.$$eval(`${dlg} .ill-close rect[fill="#2b3644"]`, (e) => e.length);
const noScroll = async (page, what) => { const w = await page.evaluate(() => document.documentElement.scrollWidth); check(w <= 390, `phone ${what}: no horizontal scroll (${w})`); };
/* the dialog opened on shards of the example (by slot), read by itself */
const sendExample = (page, slots) => page.evaluate((slots) => { const ex = FB.read(PB.parse(window.__fm1.image)); RiverDevice.place(slots.map((i) => ex.scenes[i])); }, slots);

// ================================================================ Tools is Quick Backup
const { ctx, page } = await open(DESK, "desk");
await page.goto(URL + "backup/");
check(await page.isVisible("#act-backup") && /^QUICK BACKUP$/i.test((await page.textContent("h1")).trim()), "/backup/: Quick Backup, as before");
await page.goto(QB);
await example(page);
check(/^QUICK BACKUP$/i.test((await page.textContent("h1")).trim()) && (await page.textContent(".rvh-nav a[aria-current]")) === "Tools", "/tools/: Quick Backup itself, Tools lit in the header");
check((await page.$$eval(".qb .dock .dock-b", (e) => e.map((x) => x.textContent.trim().toUpperCase()))).join(" | ") === "BACK UP THE FM-1 | OPEN A FILE", "the dock: BACK UP THE FM-1 | OPEN A FILE");
check(!(await page.$(".tool")) && !(await page.$(".rv-how")) && !(await page.$(".map")), "no hop to Quick Backup, no How it works, no waterline of slots");
check((await page.$$(".qb-end p")).length === 1 && /fm1_backup\.py/.test(await page.textContent(".qb-end")), "the terminal note: one line at the bottom");
await shot(page, "40-tools-empty", 1440);

// ---- back up
await page.click("#act-backup");
await page.waitForFunction(() => !document.querySelector("#river").hidden && !document.body.classList.contains("is-busy"));
check(/Read: 4 saved, 7 factory and 5 empty, and the settings/.test(await page.textContent("#voice .voice-say")), "back up: " + (await page.textContent("#voice .voice-say")).slice(0, 70));
const src = (await page.textContent("#river-src")).replace(/\s+/g, " ").trim();
check(/^FROM YOUR FM-1 · \d\d:\d\d · FRASIL 1\.1 · 4 saved, 7 factory, 5 empty, and the settings$/.test(src), "the source line: " + src);
check((await page.textContent("#act-keep-all")).trim() === "Download the backup" && await page.$eval("#act-keep-all", (e) => e.classList.contains("a-ice")) &&
      /^Restore it all/.test((await page.textContent("#act-restore-all")).trim()), "its actions: Download the backup (primary), Restore it all…");
check((await page.$$("#cards .sr-item.is-slot")).length === 11 && (await page.$$("#cards .sr-item.is-empty")).length === 5 && (await page.$$eval("#cards .sr-item .no", (e) => e.map((x) => x.textContent.replace("Slot ", "")))).join() ===
      "01,02,03,04,05,06,07,08,09,10,11,12,13,14,15,16", "the sixteen slots: numbered rows, the 5 empty ones dim");
check((await page.textContent("#shard-0 .nm-s")) === "FACTORY SCENE" && !(await page.$("#shard-2 .nm-s")) && (await page.textContent("#shard-10 .bq")).includes("SEQ"),
      "a factory scene says so; AURORA's BASS: SEQ");
check(await page.$eval("#cards > :last-child", (e) => e.id === "settings" && /Last scene \d\d/.test(e.textContent)), "the settings, last: " + (await page.textContent("#settings .nm-d")));
await shot(page, "40-tools", 1440, true);
const [dl1] = await Promise.all([page.waitForEvent("download"), page.click("#act-keep-all")]);
const full = fs.readFileSync(await dl1.path());
check(/^frasil-backup-\d{4}-\d\d-\d\d\.frasil$/.test(dl1.suggestedFilename()) && await page.evaluate((b) => { const a = window.__fm1.image; return a.length === b.length && a.every((x, i) => x === b[i]); }, [...full]),
      "Download the backup: " + dl1.suggestedFilename() + ", the FM-1's, byte for byte");

// ---- the selection
for (const i of [2, 8, 9]) await page.check(`#shard-${i} .ck input`);
check((await page.$$eval("#cards .selbar > *", (e) => e.map((x) => x.textContent.trim().toUpperCase()).filter(Boolean))).join(",") === "3 SELECTED,SEND TO FM-1,DOWNLOAD,CLEAR",
      "three selected: Send to FM-1, Download, Clear");
await shot(page, "40-tools-selected", 1440);
const [dl2] = await Promise.all([page.waitForEvent("download"), page.click('#cards .selbar [data-act="download"]')]);
const packText = fs.readFileSync(await dl2.path(), "utf8"), pack = JSON.parse(packText);
check(pack.format === "frasil-scenes" && pack.scenes.map((x) => x.name).join() === "ABYSS,CATHEDRAL,GLACIER", "Download: a package of the 3");
fs.writeFileSync(`${OUT}/rv/three.frasilscenes`, packText);

// ================================================================ SEND TO FM-1: reading by itself, then ready
await fm1(page, { slow: 90 });
await page.click('#cards .selbar [data-act="send"]');
await page.waitForSelector("#place[open] .wl.flow");
check(/^SEND TO FM-1\s*3 SHARDS$/.test((await page.textContent("#place-h")).trim()) && !(await page.$("#place .sheet-h")) && (await sayTitle(page)) === "READING THE FM-1",
      "SEND TO FM-1 · 3 SHARDS, titled as the FM-1 titles a page (no serif headline); it reads the FM-1 by itself");
check(!(await page.$("#place-read")) || !(await page.isVisible("#place-read")), "no Read button while it reads");
await shot(page, "10-send-reading", 1440);
await page.waitForSelector("#place-list", { timeout: 30000 });
await fm1(page, { slow: 0 });
check(/FRASIL 1\.1 · read just now/.test(await page.textContent("#place .p-fm .devl")) && await page.isVisible("#place-read"), "ready: FRASIL 1.1 · read just now, a quiet READ AGAIN");
const names = await page.$$eval("#slots .tile .t-n", (e) => e.map((x) => x.textContent));
check(names[2] === "ABYSS" && names[0] === "MIST" && names[15] === "EMPTY", "the FM-1's sixteen slots: " + names.slice(0, 4).join(","));
check(await page.isDisabled("#place-send") && Number(await page.$eval("#place-send", (e) => getComputedStyle(e).opacity)) < 0.5, "nothing placed: Send disabled, and looks it (G6)");
await page.selectOption("#place-list li:nth-child(1) select", "11");      // ABYSS -> 12
await page.dragAndDrop("#place-list li:nth-child(2) .cd-top", "#slots .tile:nth-child(13)");   // CATHEDRAL -> 13
check((await page.textContent("#place-send")).trim() === "Send 2 to FM-1", "the Send button counts what is placed: Send 2 to FM-1");
await shot(page, "14-send-ready-3", 1440);
await page.selectOption("#place-list li:nth-child(3) select", "0");       // GLACIER -> 1
const sum = await page.textContent("#place-sum");
check(/GLACIER goes into slot 01, in place of the factory’s MIST/.test(sum) && /ABYSS goes into slot 12, empty now/.test(sum) && /CATHEDRAL goes into slot 13/.test(sum), "a line per shard: " + sum.slice(0, 80));
check((await page.textContent("#place-send")).trim() === "Send 3 to FM-1" && (await page.textContent("#slots .tile:nth-child(12) .t-was")) === "INTO AN EMPTY SLOT" &&
      (await page.textContent("#slots .tile:nth-child(1) .t-was")) === "REPLACES MIST", "the slots say what happens: REPLACES MIST, INTO AN EMPTY SLOT");
await page.click("#place-origin");
check((await page.$$eval("#place-list select", (e) => e.map((x) => x.value))).join() === "2,8,9", "Back where they came from: 03, 09, 10");
await page.selectOption("#place-list li:nth-child(1) select", "11");
await page.selectOption("#place-list li:nth-child(2) select", "12");
await page.selectOption("#place-list li:nth-child(3) select", "0");

// ---- the confirmation (then Cancel: nothing sent)
await page.click("#place-send");
await page.waitForSelector("#confirm[open]");
const conf = (await page.textContent("#confirm")).replace(/\s+/g, " ");
check(/SEND 3 SHARDS\s*NOW\?/.test(conf) && /What plays on the FM-1 stops\./.test(conf) && /Slots 01, 12 and 13 are replaced: GLACIER in place of the factory’s MIST; ABYSS and CATHEDRAL into empty slots\./.test(conf) &&
      /The other slots and the settings stay\./.test(conf) && /answer YES with SEL/.test(conf), "the confirmation lists the slots: " + conf.slice(0, 150));
await shot(page, "16-send-confirm", 1440);
await page.click("#confirm .confirm-f .a:not(#confirm-send)");
check(await page.isVisible("#place-list") && await page.evaluate(() => !window.__fm1.events.includes("restore")), "Cancel: back to the slots, nothing sent");

// ---- a slot changed on the FM-1 since it was read: nothing sent
await page.evaluate(() => { window.__fm1.mutate = (img) => { const sc = FB.read(PB.parse(img)).scenes; return FB.patch(img, [{ slot: 11, bytes: sc[10].bytes }]); }; });
await page.click("#place-send");
await page.waitForSelector("#confirm[open]");
await page.click("#confirm-send");
await page.waitForFunction(() => /Slot 12 has changed/.test(document.querySelector("#place-alarm")?.textContent || ""), null, { timeout: 30000 });
check(await page.evaluate(() => window.__fm1.restores === 0 && !window.__fm1.events.includes("restore")) && /ABYSS goes into slot 12, in place of AURORA/.test(await page.textContent("#place-sum")),
      "changed since read: " + (await page.textContent("#place-alarm")).slice(0, 60) + " (nothing sent; the slots as they are now)");
await page.click("#place-send");
await page.waitForSelector("#confirm[open]");

// ---- sending: CC 120 first (playing), the question (FX and SEL only lit), then yes
await fm1(page, { playing: true, soundOff: 0, events: [], slow: 12, answer: null });
await page.click("#confirm-send");
await waitTitle(page, /^SENDING · [1-9]/);
check(true, "sending: progress on a waterline (" + await sayTitle(page) + ")");
await shot(page, "17-send-sending", 1440);
await waitTitle(page, /^PRESS SEL ON THE FM-1$/);
check(await page.evaluate(() => { const e = window.__fm1.events; return window.__fm1.ccBeforeRestore === 16 && e.indexOf("cc120") < e.indexOf("restore") && !window.__fm1.playing; }),
      "CC 120 on the 16 channels before the restore (" + await page.evaluate(() => window.__fm1.events.slice(0, 3).join(",") + "…") + ")");
check(await lit(page) === 2 && /NO/.test(await page.textContent("#place .ill-close")) && /YES/.test(await page.textContent("#place .ill-close")) && await page.isDisabled("#place .sheet-x"),
      "press SEL: the drawn FM-1 asks NO [FX] / YES [SEL], only FX and SEL lit; the dialog stays");
await shot(page, "18-send-press-sel", 1440);
await fm1(page, { answer: true, slow: 0 });
await waitTitle(page, /^DONE$/);
check(await lit(page) === 1 && /RESTORED/.test(await page.textContent("#place .ill-close")) && /OK/.test(await page.textContent("#place .ill-close")) &&
      /GLACIER in 01, ABYSS in 12 and CATHEDRAL in 13\. The other slots and the settings are as they were/.test(await page.textContent("#place .dlg-say")), "done: RESTORED with OK [SEL], only SEL lit; " + (await page.textContent("#place .dlg-say p")).slice(0, 60));
const placedOk = await page.evaluate(async () => {
  const ex = FB.read(PB.parse(new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()))), a = FB.read(PB.parse(window.__fm1.image));
  const eq = (x, y) => x && y && x.length === y.length && x.every((v, i) => v === y[i]);
  return eq(a.scenes[11].bytes, ex.scenes[2].bytes) && eq(a.scenes[12].bytes, ex.scenes[8].bytes) && eq(a.scenes[0].bytes, ex.scenes[9].bytes) &&
    a.scenes.every((s, i) => [0, 11, 12].includes(i) || s.state === ex.scenes[i].state) && eq(a.settings.raw_v, ex.settings.raw_v);
});
check(placedOk, "the FM-1 holds ABYSS in 12, CATHEDRAL in 13, GLACIER in 1; the rest and the settings as they were");
await shot(page, "19-send-done", 1440);
await page.click("#place-close");
check(/changed since this backup/.test(await page.textContent("#river-flag")), "Quick Backup says the FM-1 changed since its backup");

// ---- the cable's other states, one shard
await example(page);
await fm1(page, { connected: false });
await sendExample(page, [2]);
await waitTitle(page, /^NOT CONNECTED$/);
check(/USB-C data cable/.test(await page.textContent("#place .dlg-say")) && await page.isVisible("#place-again") && await page.$$eval("#place .ill.is-off", (e) => e.length) === 1,
      "not connected: the cable unplugged, one sentence, Try again");
await shot(page, "11-send-not-connected", 1440);
await fm1(page, { connected: true, id: "FM-1_0103" });
await page.click("#place-again");
await waitTitle(page, /^FRASIL NOT INSTALLED$/);
check(/guide\/install\/$/.test(await page.getAttribute("#place .dlg-f a.a", "href")) && /Install FRASIL/i.test(await page.textContent("#place .dlg-f a.a")) && await page.isVisible("#place-again"),
      "FRASIL not installed (another firmware): Install FRASIL, Try again");
await shot(page, "12-send-no-frasil", 1440);
await fm1(page, { id: "FM-1_47010100" });
await page.click("#place-again");
await page.waitForSelector("#place-list");
check(/Drag the shard onto a slot/.test(await page.textContent("#place .p-lead")) && /^SEND TO FM-1\s*1 SHARD$/.test((await page.textContent("#place-h")).trim()), "one shard: SEND TO FM-1 · 1 SHARD");
await page.selectOption("#place-list li:nth-child(1) select", "13");
check((await page.textContent("#place-send")).trim() === "Send to FM-1", "one placed of one: Send to FM-1");
await shot(page, "13-send-ready-1", 1440);
await fm1(page, { answer: false, asked: 0 });
await page.click("#place-send");
await page.click("#confirm-send");
await waitTitle(page, /^NOT SENT$/);
check(await lit(page) === 1 && /NOT RESTORED/.test(await page.textContent("#place .ill-close")) && /FX was pressed/.test(await page.textContent("#place .dlg-say")) && await page.isVisible("#place-again"),
      "FX: NOT RESTORED with OK [SEL], Send again");
await shot(page, "19b-send-failed", 1440);
await fm1(page, { held: true, answer: true, soundOff: 0 });
await page.click("#place-again");
await waitTitle(page, /^SOMETHING IS STILL HELD$/);
check(/STOP FIRST/.test(await page.textContent("#place .ill-close")) && /Let go of it, or hold PLAY/.test((await page.textContent("#place .dlg-say")).replace(/\s+/g, " ")) &&
      await page.evaluate(() => window.__fm1.ccBeforeRestore === 16), "a key held: STOP FIRST after CC 120; let go, or hold PLAY");
await shot(page, "19c-send-held", 1440);
await fm1(page, { held: false });
await page.click("#place-again");
await waitTitle(page, /^DONE$/);
check(await page.evaluate(() => FB.read(PB.parse(window.__fm1.image)).scenes[13].name === "ABYSS"), "Send again: done, ABYSS in 14");
await page.click("#place-close");

// ---- an FM-1 whose backup has no bass's record (OBJ_BSEQ): refused, nothing sent
await page.evaluate(async () => {
  const img = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()), objs = PB.parse(img).objs.filter((o) => o.obj !== FB.S.objs.bseqs);
  const le = (v, n) => Array.from({ length: n }, (_, i) => (v >>> (8 * i)) & 255);
  const head = [...PB.MAGIC, ...le(PB.VERSION, 2), ...le(objs.length, 2), ...le(0, 4)], out = [...head, ...le(PB.crc32(Uint8Array.from(head)), 4)];
  for (const { obj, data } of objs) out.push(...le(obj, 2), ...le(data.length, 2), ...le(PB.crc32(data), 4), ...data);
  window.__fm1.image = Uint8Array.from([...out, ...le(PB.crc32(Uint8Array.from(out)), 4)]);
  window.__fm1.restores = 0;
});
await sendExample(page, [2]);
await waitTitle(page, /^UPDATE FRASIL$/);
check(/another version of FRASIL/.test(await page.textContent("#place .dlg-say")) && await page.evaluate(() => window.__fm1.restores === 0), "no OBJ_BSEQ on the FM-1: refused (Update FRASIL), nothing sent");
await page.click("#place .sheet-x");
await example(page);

// ================================================================ Restore it all
await page.click("#act-restore-all");
await page.waitForSelector("#whole[open]");
check(/^RESTORE IT ALL/.test((await page.textContent("#whole-h")).trim()) && /What plays on the FM-1 stops/.test(await page.textContent("#whole")) && !/hold\s+PLAY/i.test(await page.textContent("#whole")),
      "Restore it all: what plays stops, all sixteen slots and the settings (no hold PLAY asked)");
await shot(page, "41-restore-all", 1440);
await fm1(page, { held: true, soundOff: 0, asked: 0 });
await page.click("#whole-send");
await waitTitle(page, /^SOMETHING IS STILL HELD$/, "#whole");
check(await page.evaluate(() => window.__fm1.soundOff === 16), "a key held: STOP FIRST, after CC 120");
await fm1(page, { held: false, playing: true, answer: false });
await page.click("#whole-again");
await waitTitle(page, /^NOT SENT$/, "#whole");
check(await page.evaluate(() => !window.__fm1.playing && window.__fm1.asked === 1), "playing: let go by CC 120, then asked; FX: not restored");
await fm1(page, { answer: true });
await page.click("#whole-again");
await waitTitle(page, /^DONE$/, "#whole");
check(await page.evaluate(async () => { const a = window.__fm1.image, b = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()); return a.length === b.length && a.every((x, i) => x === b[i]); }),
      "restored: the FM-1 holds the backup again, byte for byte");
await page.click("#whole-close");

// ================================================================ files
await page.setInputFiles("#file", `${OUT}/rv/three.frasilscenes`);
await page.waitForFunction(() => /^A SCENE PACKAGE/.test(document.querySelector("#river-src").textContent));
check((await page.$$("#cards .sr-item")).length === 3 && !(await page.$("#settings")) && await page.isHidden("#act-restore-all"), "a package: three rows, no settings, nothing to restore whole");
fs.writeFileSync(`${OUT}/rv/bad.frasil`, "not a backup at all, just words");
await page.setInputFiles("#file", `${OUT}/rv/bad.frasil`);
await page.waitForTimeout(200);
check(/not a FRASIL backup/.test(await page.textContent("#voice .voice-alarm")), "a bad file: " + (await page.textContent("#voice .voice-alarm")).slice(0, 60));
const old = await page.evaluate(async () => {     // an earlier FRASIL's backup (FRSf scenes)
  const S = FB.S, N = S.sizes, l = S.old_scenes.find((x) => x.magic === "FRSf");
  const rs = l.name + l.nv + 2 + N.SC_CHORDS * 5 + N.SC_ARP + 1 + N.SC_SEQ, p = new Uint8Array(8 + 16 * rs);
  [..."FRSf"].forEach((c, i) => { p[i] = c.charCodeAt(0); });
  p[4] = 1 << 2;
  const o = 8 + 2 * rs;
  [..."OLDIE"].forEach((c, i) => { p[o + i] = c.charCodeAt(0); });
  p[o + l.name + l.nv] = 1;
  p.set([50, 50, 57, 255, 255], o + l.name + 2 + l.nv);
  p[o + l.name + 2 + l.nv + N.SC_CHORDS * 5 + N.SC_ARP] = 33;
  const ex = PB.parse(new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()));
  return [...FB.build(ex.objs.map((x) => (x.obj === S.objs.scenes ? { obj: x.obj, data: p } : x)))];
});
fs.writeFileSync(`${OUT}/rv/old.frasil`, Buffer.from(old));
await page.setInputFiles("#file", `${OUT}/rv/old.frasil`);
await page.waitForFunction(() => /old\.frasil/.test(document.querySelector("#river-src").textContent));
check(await page.isVisible("#shard-2 .badge.is-old") && await page.isDisabled("#shard-2 .ck input") && /brought up to date/.test(await page.textContent("#river-flag")),
      "an earlier FRASIL's scene: its badge, not selectable, and the source says so");

// ================================================================ a row opened in place; ▶ with FRASIL's engine
const p3 = (await open(DESK, "rows")).page;
await withPlayer(p3);
await p3.goto(QB + "#example");
await p3.waitForSelector("#shard-8");
check(await p3.getAttribute("#shard-8 .sr-open", "aria-expanded") === "false" && !(await p3.$("#shard-8 .oc-body")), "a row starts closed (its card not built)");
check(/D MAJOR/.test(await p3.textContent("#shard-8 .kv")) && (await p3.textContent("#shard-8 .gt")).includes("ARP"), "closed: its key, GLINT's type");
await p3.click("#shard-8 .sr-open");
await p3.waitForSelector("#shard-8.open .oc-body .kb");
check((await p3.$$("#shard-8 .sec")).length >= 8, "opened in place: the keyboard, GLINT, the sections");
check((await p3.$$("#shard-10 .ms-bq")).length >= 3 && /bass's sequence/.test(await p3.getAttribute("#shard-10 .ms", "aria-label")), "AURORA closed: its bass's sequence on the line of what plays");
await p3.click("#settings .sr-open");
await p3.waitForSelector("#settings.open .set-grid");
check(/LAST SCENE/.test(await p3.textContent("#settings .set-grid")), "the settings, opened: what comes back at power-on");
const engine = !!(await p3.evaluate(async () => (await fetch("/emu/app.wasm", { method: "HEAD" })).ok));
await p3.click("#shard-2 .play");
if (engine) {
  await p3.waitForFunction(() => document.querySelector("#shard-2").dataset.sound === "playing", null, { timeout: 30000 });
  check(await p3.getAttribute("#shard-2 .play", "aria-label") === "Stop ABYSS", "▶ ABYSS: it sounds (■)");
  await p3.click("#shard-9 .play");
  await p3.waitForFunction(() => document.querySelector("#shard-9").dataset.sound === "playing", null, { timeout: 15000 });
  check(!(await p3.getAttribute("#shard-2", "data-sound")), "▶ GLACIER: one shard at a time");
  await p3.keyboard.press("Escape");
  await p3.waitForTimeout(300);
  check(!(await p3.getAttribute("#shard-9", "data-sound")) && await p3.evaluate(() => window.RiverPlayer.state) === "idle", "Escape: silence");
} else console.log("    (no emu/ on this site: the engine's own states not checked)");
const p4 = (await open(DESK, "no engine")).page;
await withPlayer(p4);
await p4.route("**/emu/**", (route) => route.fulfill({ status: 404, body: "" }));
await p4.goto(QB + "#example");
await p4.waitForSelector("#shard-2");
await p4.click("#shard-2 .play");
await p4.waitForFunction(() => /Sound needs a moment/.test(document.querySelector("#shard-2 .sh-say").textContent), null, { timeout: 15000 });
check(!(await p4.getAttribute("#shard-2", "data-sound")), "no engine on the site: ▶ says sound needs a moment, nothing stuck");

// ================================================================ phones: Quick Backup, every state of the Send dialog
const { page: p2 } = await open(PHONE, "phone");
await p2.goto(QB + "#example");
await p2.waitForSelector("#shard-2");
await example(p2);
await noScroll(p2, "Quick Backup");
await shot(p2, "40-tools", 390);
await p2.check("#shard-2 .ck input");
await p2.check("#shard-8 .ck input");
await p2.check("#shard-9 .ck input");
await fm1(p2, { slow: 90 });
await p2.click('#cards .selbar [data-act="send"]');
await p2.waitForSelector("#place[open] .wl.flow");
await shot(p2, "10-send-reading", 390);
await p2.waitForSelector("#place-list", { timeout: 30000 });
await fm1(p2, { slow: 0 });
check(/Choose a slot for each shard/.test(await p2.textContent("#place .p-lead")) && await p2.evaluate(() => document.activeElement.tagName === "SELECT"),
      "phone: Choose a slot for each shard; the first card's slot picker has the focus");
await p2.selectOption("#place-list li:nth-child(1) select", "2");
await p2.selectOption("#place-list li:nth-child(2) select", "11");
await noScroll(p2, "the Send dialog");
check(await p2.evaluate(() => { const d = document.querySelector("#place .dlg-b"); return d.scrollWidth <= d.clientWidth; }), "phone: the dialog itself has no horizontal scroll");
await shot(p2, "14-send-ready-3", 390);
await p2.selectOption("#place-list li:nth-child(3) select", "12");
await p2.click("#place-send");
await p2.waitForSelector("#confirm[open]");
await shot(p2, "16-send-confirm", 390);
await fm1(p2, { slow: 12, answer: null });
await p2.click("#confirm-send");
await waitTitle(p2, /^SENDING · [1-9]/);
await shot(p2, "17-send-sending", 390);
await waitTitle(p2, /^PRESS SEL/);
await shot(p2, "18-send-press-sel", 390);
await fm1(p2, { answer: true, slow: 0 });
await waitTitle(p2, /^DONE$/);
await shot(p2, "19-send-done", 390);
await p2.click("#place-close");
await example(p2);
await fm1(p2, { connected: false });
await sendExample(p2, [2]);
await waitTitle(p2, /^NOT CONNECTED$/);
await shot(p2, "11-send-not-connected", 390);
await fm1(p2, { connected: true, id: "FM-1_0103" });
await p2.click("#place-again");
await waitTitle(p2, /^FRASIL NOT INSTALLED$/);
await shot(p2, "12-send-no-frasil", 390);
await fm1(p2, { id: "FM-1_47010100" });
await p2.click("#place-again");
await p2.waitForSelector("#place-list");
await p2.selectOption("#place-list li:nth-child(1) select", "13");
await shot(p2, "13-send-ready-1", 390);
await fm1(p2, { answer: false });
await p2.click("#place-send");
await p2.click("#confirm-send");
await waitTitle(p2, /^NOT SENT$/);
await shot(p2, "19b-send-failed", 390);
await fm1(p2, { held: true, answer: true });
await p2.click("#place-again");
await waitTitle(p2, /^SOMETHING IS STILL HELD$/);
await shot(p2, "19c-send-held", 390);
await noScroll(p2, "STOP FIRST");
await fm1(p2, { held: false });
await p2.click("#place-close");
await p2.click("#act-restore-all");
await p2.waitForSelector("#whole[open]");
await shot(p2, "41-restore-all", 390);

check(!errors.length, errors.length ? "console:\n  " + errors.join("\n  ") : "no console errors");
await browser.close();
console.log(fails ? `${fails} FAILED` : "all ok");
process.exit(fails ? 1 : 0);
