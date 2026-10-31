// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The Play page's EDITOR (js/play-editor.js), in Chrome: EDITOR first (the switch, NEW SHARD and the factory scenes as
// the River's rows), a card opened and played, a setting turned (the engine playing gets it: app_worklet.js op "param",
// web_param), THE KEYS (DRONE: a chord latched, op "latch"), DOWNLOAD (a scene package Quick Backup opens), EMULATOR
// and back (one engine at a time: the emulator's audio asleep in EDITOR, a shard stopped in EMULATOR), #emulator, OPEN
// IN THE RIVER (Your Private River: the draft, EDITED, the changes marked; signed out, SAVE AS says sign in; with the
// service, signed in: SAVE AS makes it one's shard); 390 px: no horizontal scroll. No console errors.
//   node website/tests/play_editor.mjs [OUT_DIR]
// SITE_URL: the main site served (default http://localhost:8091/: website/serve.py build/website/frasil 8091);
// RIVER_URL: the River served as files (default http://localhost:8092/); RIVER_STATIC (default build/website/river):
// the River the service is started with (server/.venv's uvicorn, a fresh database, port RIVER_TEST_PORT or 8058) for
// SAVE AS signed in (NO_SERVICE=1: not that part). PLAYWRIGHT: Playwright's index.mjs (default: the global one).
import fs from "fs";
import os from "os";
import path from "path";
import { spawn } from "child_process";
import { fileURLToPath } from "url";
import { FAKE } from "./fake_fm1.mjs";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");

const REPO = path.join(path.dirname(fileURLToPath(import.meta.url)), "../..");
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), "play-editor-")));
fs.mkdirSync(OUT, { recursive: true });
const SITE = (process.env.SITE_URL || "http://localhost:8091/").replace(/\/?$/, "/");
const RIVER = (process.env.RIVER_URL || "http://localhost:8092/").replace(/\/?$/, "/");
const SCHEMA = JSON.parse(fs.readFileSync(path.join(REPO, "server/schema.json"), "utf8"));
const ID = SCHEMA.ids, ST = SCHEMA.scene_t;
let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "  ok" : "FAIL"} ${msg}`); if (!ok) fails++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const browser = await chromium.launch({ channel: "chrome", args: ["--autoplay-policy=no-user-gesture-required"] });
const errors = [];
const watch = (page, where) => {
  page.on("pageerror", (e) => errors.push(`${where}: ${e.message}`));
  page.on("console", (m) => { if (m.type() === "error" && !/reader\/words\.js|status of 404|status of 503|status of 502|ERR_CONNECTION/.test(m.text() + m.location().url)) errors.push(`${where}: ${m.text()}`); });
};
/* what the page sends the engines' worklets ({type: "app", op}: param, latch, scene...) */
const SPY = () => {
  window.__sent = [];
  const pm = MessagePort.prototype.postMessage;
  MessagePort.prototype.postMessage = function (m, ...r) { if (m && m.type === "app") window.__sent.push(m); return pm.call(this, m, ...r); };
};
const ctx = await browser.newContext({ viewport: { width: 1440, height: 900 }, acceptDownloads: true });
const page = await ctx.newPage();
watch(page, "play");
await page.addInitScript(SPY);

// ---------------------------------------------------------------- EDITOR first
await page.goto(SITE + "play/");
await page.waitForSelector("#ed-list .sr-item");
check(await page.evaluate(() => document.documentElement.dataset.play) === "editor", "a first visit: EDITOR");
check(await page.$eval('#mode-switch [data-mode="editor"]', (b) => b.getAttribute("aria-pressed")) === "true" &&
      await page.$eval('#mode-switch [data-mode="emulator"]', (b) => b.getAttribute("aria-pressed")) === "false", "the switch: EMULATOR | EDITOR, EDITOR pressed");
const names = await page.$$eval("#ed-list .sr-item .nm-n", (n) => n.map((x) => x.textContent));
check(names[0] === "NEW SHARD" && names.length === 1 + SCHEMA.sizes.NFACTORY && names[1] === "NORTH RIVER", `NEW SHARD and the factory scenes: ${names.join(", ")}`);
check(!(await page.isVisible(".play-device")) && !(await page.isVisible(".play-notes")), "the emulator hidden");
await sleep(800);
await page.screenshot({ path: path.join(OUT, "play-editor-1440.png") });

// ---------------------------------------------------------------- a card opened, played, a setting turned
const N = "#ed-1";
await page.click(`${N} .sr-open`);
await page.waitForSelector(`${N}.open .oc-keys`);
await sleep(700);
const acts = await page.$$eval(`${N} > .sr .acts [data-act]`, (b) => b.map((x) => x.dataset.act));
check(acts.join() === "download,send,river", `its head: DOWNLOAD, SEND TO FM-1, OPEN IN THE RIVER (${acts.join()})`);
await page.click(`${N} .play`);
await page.waitForFunction(() => window.RiverPlayer && RiverPlayer.state === "playing", null, { timeout: 20000 });
await sleep(1500);
check(await page.evaluate(() => RiverPlayer.level()) > 0.0005, "▶: it sounds (FRASIL's engine, the emulator's app.wasm)");
await page.evaluate(() => { window.__sent.length = 0; });
await page.evaluate((art) => { const d = [...document.querySelectorAll(`${art} details.sec`)].find((x) => x.querySelector(".sec-name").firstChild.textContent === "FX"); d.open = true; }, N);
await page.waitForSelector(`${N} details.sec[open] .cell[data-p]`);
const p0 = await page.$eval(`${N} details.sec[open] .cell[data-p]`, (c) => { c.scrollIntoView({ block: "center" }); return +c.dataset.p; });
const pc = `${N} .oc-body .cell[data-p="${p0}"]`;
const v0 = await page.$eval(pc, (c) => +c.getAttribute("aria-valuenow"));
await page.focus(pc);
const step = v0 > 0 ? "ArrowLeft" : "ArrowRight";
for (let i = 0; i < 3; i++) await page.keyboard.press(step);
await sleep(900);
const v1 = await page.$eval(pc, (c) => +c.getAttribute("aria-valuenow"));
const sent = await page.evaluate(() => window.__sent.filter((m) => m.op === "param"));
check(v1 !== v0 && sent.some((m) => m.p === p0 && m.x === v1), `a setting turned (${SCHEMA.params[p0].name} ${v0} -> ${v1}): the engine playing gets it (web_param ${JSON.stringify(sent.at(-1))})`);
check(/EDITED/.test(await page.textContent(`${N} .ed-band`)) && /changed\. The sound follows\.$/.test((await page.textContent(`${N} .ed-band`)).trim()), "EDITED: the line, no SAVE");
const eacts = await page.$$eval(`${N} > .sr .acts [data-act]`, (b) => b.map((x) => x.dataset.act));
check(eacts.join() === "undo,revert,download,send,river", `edited, its head: UNDO, REVERT, then the three (${eacts.join()})`);

// ---------------------------------------------------------------- THE KEYS: DRONE, a chord latched
await page.click(`${N} .oc-keys [data-k="KEYS:drone"]`);
await sleep(200);
await page.evaluate(() => { window.__sent.length = 0; });
const kp = await page.evaluate((art) => {
  const ks = [...document.querySelectorAll(`${art} .oc-keys .k-key[data-n]`)].filter((k) => +k.dataset.n >= 60 && +k.dataset.n <= 72 && !k.classList.contains("lat"));
  const k = ks.find((x) => +x.getAttribute("height") > 100) || ks[0];
  k.scrollIntoView({ block: "center" });
  const r = k.getBoundingClientRect();
  return { x: r.left + r.width / 2, y: r.top + r.height * 0.86, n: +k.dataset.n };
}, N);
const nch0 = await page.evaluate((o) => { const d = RiverEdit.drafts.get("play:1"); return d ? d.cur[o] : -1; }, ST.nch);
await page.mouse.click(kp.x, kp.y);
await sleep(500);
const latched = await page.evaluate(() => window.__sent.filter((m) => m.op === "latch"));
const nch1 = await page.evaluate((o) => { const d = RiverEdit.drafts.get("play:1"); return d ? d.cur[o] : -1; }, ST.nch);
check(latched.length > 0 && nch1 === nch0 + 1, `THE KEYS: key ${kp.n} latches its chord (the engine playing: ${JSON.stringify(latched[0])}; chords ${nch0} -> ${nch1})`);
await page.screenshot({ path: path.join(OUT, "play-editor-open-1440.png") });

// ---------------------------------------------------------------- DOWNLOAD: a scene package
const [dl] = await Promise.all([page.waitForEvent("download"), page.click(`${N} > .sr [data-act="download"]`)]);
const file = path.join(OUT, dl.suggestedFilename());
await dl.saveAs(file);
const pack = JSON.parse(fs.readFileSync(file, "utf8"));
const draft = await page.evaluate(() => Array.from(RiverEdit.drafts.get("play:1").cur));
check(/\.frasilscenes$/.test(file) && pack.format === "frasil-scenes" && pack.layout === SCHEMA.magic.shard && pack.scenes.length === 1 &&
      Buffer.from(pack.scenes[0].data, "base64").equals(Buffer.from(draft)), `DOWNLOAD: ${path.basename(file)}, a scene package, the shard as edited`);

// ---------------------------------------------------------------- EMULATOR and back: one engine at a time
await page.click('#mode-switch [data-mode="emulator"]');
await sleep(300);
check(await page.isVisible(".play-device") && !(await page.isVisible("#editor")), "EMULATOR: the panel, the editor hidden");
await page.waitForFunction(() => RiverPlayer.state === "idle", null, { timeout: 3000 }).catch(() => {});
check(await page.evaluate(() => RiverPlayer.state) === "idle", "...the shard playing stopped");
check(new URL(page.url()).hash === "#emulator", "...the address says #emulator");
await page.click("#go");
await page.waitForFunction(() => /^On/.test(document.getElementById("status").textContent), null, { timeout: 20000 });
await sleep(800);
check(await page.evaluate(() => node && node.context.state) === "running", "the emulator switched on: its audio runs");
await page.screenshot({ path: path.join(OUT, "play-emulator-1440.png") });
await page.click('#mode-switch [data-mode="editor"]');
await sleep(400);
check(await page.evaluate(() => node.context.state) === "suspended" && await page.isVisible("#editor"), "EDITOR again: the emulator's audio asleep, the cards back");
check(await page.$eval(N, (a) => a.classList.contains("is-edited")), "...the card as it was left (edited)");
await page.click('#mode-switch [data-mode="emulator"]');
await sleep(400);
check(await page.evaluate(() => node.context.state) === "running", "EMULATOR again: its audio back");
await page.reload();
await page.waitForSelector("#mode-switch");
check(await page.evaluate(() => document.documentElement.dataset.play) === "emulator", "reloaded: EMULATOR kept (#emulator)");
await page.goto(SITE + "play/#editor");
await page.waitForSelector("#ed-list .sr-item");
check(await page.evaluate(() => document.documentElement.dataset.play) === "editor", "#editor: EDITOR");
await page.goto(SITE + "play/");
await page.waitForSelector("#mode-switch");
check(await page.evaluate(() => document.documentElement.dataset.play) === "emulator", "no address: the last chosen (EMULATOR)");
await page.click('#mode-switch [data-mode="editor"]');
await page.goto(SITE + "play/");
await page.waitForSelector("#ed-list .sr-item");
check(await page.evaluate(() => document.documentElement.dataset.play) === "editor", "...EDITOR chosen: EDITOR");

// ---------------------------------------------------------------- OPEN IN THE RIVER
await ctx.route("https://river.frasilsynth.com/**", (r) => r.fulfill({ status: 200, contentType: "text/html", body: "" }));   // (only its address read)
await page.click(`${N} .sr-open`).catch(() => {});
await page.waitForSelector(`${N}.open`);
const [pop] = await Promise.all([ctx.waitForEvent("page"), page.click(`${N} > .sr [data-act="river"]`)]);
await pop.waitForURL(/river\.frasilsynth\.com/, { timeout: 10000 }).catch(() => {});
const target = pop.url();
await pop.close();
const m = /^https:\/\/river\.frasilsynth\.com\/yours\/#play=([A-Za-z0-9]{4})\.1\.([\w-]+)$/.exec(target);
check(!!m && m[1] === SCHEMA.magic.shard, `OPEN IN THE RIVER: ${target.slice(0, 80)}…`);
const bytesOf = (d) => Buffer.from(d.replace(/-/g, "+").replace(/_/g, "/"), "base64");
check(!!m && bytesOf(m[2]).equals(Buffer.from(draft)), "...carrying the shard as edited");
const there = (base) => base + "yours/" + target.slice(target.indexOf("#"));

async function riverSide(base, signedIn, shots) {
  const rp = await ctx.newPage();
  watch(rp, "river");
  await rp.goto(there(base));
  await rp.waitForSelector("#from-play:not([hidden]) #from-play-card.open .oc-keys");
  await sleep(900);
  check(!/#play=/.test(rp.url()), "the River: the address made clean");
  check(await rp.$eval("#from-play-card", (a) => a.classList.contains("is-edited")) && /EDITED · NOT SAVED/.test(await rp.textContent("#from-play-card .nm-t")),
        "the River: NORTH RIVER as a draft (EDITED · NOT SAVED)");
  check(await rp.$eval("#from-play-card", (a) => [...a.querySelectorAll("details.sec .sec-name")].some((n) => n.firstChild.textContent === "FX" && n.querySelector(".ed-dot"))),
        "...the section of the setting turned on the Play page marked (FX)");
  await rp.evaluate(() => { const d = [...document.querySelectorAll("#from-play-card details.sec")].find((x) => x.querySelector(".sec-name").firstChild.textContent === "FX"); d.open = true; });
  await rp.waitForSelector("#from-play-card details.sec[open] .cell.is-edited", { timeout: 3000 }).catch(() => {});
  check((await rp.$$("#from-play-card .oc-body .cell.is-edited")).length >= 1, "...opened: the setting marked, where it was ticked");
  const ra = await rp.$$eval("#from-play-card > .sr .acts [data-act]", (b) => b.map((x) => x.dataset.act));
  check(ra.join() === "undo,revert,save-as", `...its head: UNDO, REVERT, SAVE AS (${ra.join()})`);
  if (shots) await rp.screenshot({ path: path.join(OUT, shots) });
  await rp.click('#from-play-card [data-act="save-as"]');
  await rp.waitForSelector("dialog.ed-sheet[open]");
  if (!signedIn) {
    check(/Sign in/.test(await rp.textContent("dialog.ed-sheet")), "signed out: SAVE AS says sign in");
    await rp.click("dialog.ed-sheet .ed-sign a");
    await sleep(300);
    check(!(await rp.$("dialog[open]")) && await rp.isVisible("#signin"), "...Sign in: the sheet closed, the form there");
    await rp.reload();
    await rp.waitForSelector("#from-play:not([hidden]) #from-play-card");
    check(await rp.$eval("#from-play-card", (a) => a.classList.contains("is-edited")), "...reloaded (as a sign-in's way back): the draft still there");
    await rp.close();
    return;
  }
  await rp.fill("dialog.ed-sheet .name-in", "NORTH RIVER PLAY");
  await rp.click('dialog.ed-sheet [type="submit"]');
  await rp.waitForFunction(() => !document.querySelector("dialog[open]") && document.getElementById("from-play").hidden, null, { timeout: 8000 });
  await sleep(500);
  const mine = await rp.evaluate(() => fetch("/api/shards").then((r) => r.json()));
  const y = mine.items.find((x) => x.name === "NORTH RIVER PLAY");
  check(!!y && Buffer.from(y.data, "base64").subarray(16).equals(Buffer.from(draft).subarray(16)) && !y.origin_id,
        "signed in: SAVE AS makes it one's shard, its sound as edited on the Play page");
  check(await rp.$(`#m-${y && y.id}.open`) !== null, "...opened in Your shards; the Play page's card gone");
  await rp.reload();
  await sleep(800);
  check(await rp.$eval("#from-play", (s) => s.hidden), "...reloaded: no draft from the Play page any more");
  await rp.close();
}
await riverSide(RIVER, false, "river-from-play-1440.png");

// ---------------------------------------------------------------- the same, signed in (the River's service)
let srv = null;
if (!process.env.NO_SERVICE) {
  const port = process.env.RIVER_TEST_PORT || "8059", BASE = `http://localhost:${port}/`;
  const db = path.join(fs.mkdtempSync(path.join(os.tmpdir(), "play-editor-db-")), "river.sqlite");
  let log = "";
  srv = spawn(path.join(REPO, "server/.venv/bin/uvicorn"), ["app:create_app", "--factory", "--port", port], {
    cwd: path.join(REPO, "server"),
    env: { ...process.env, RIVER_URL: BASE.slice(0, -1), COOKIE_SECURE: "0", RIVER_STATIC: process.env.RIVER_STATIC || path.join(REPO, "build/website/river"), RIVER_DB: db },
  });
  srv.stdout.on("data", (d) => { log += d; });
  srv.stderr.on("data", (d) => { log += d; });
  process.on("exit", () => srv.kill());
  let up = false;
  for (let i = 0; i < 60 && !up; i++) { try { up = (await fetch(BASE + "api/health")).ok; } catch (_) { await sleep(250); } }
  if (!up) { console.log("FAIL the River's service didn't start\n" + log); fails++; }
  else {
    const rp = await ctx.newPage();
    await rp.goto(BASE + "yours/#register");
    await rp.fill('#register [name="display_name"]', "Player");
    await rp.fill('#register [name="email"]', "player@example.com");
    await rp.fill('#register [name="password"]', "a long password");
    await rp.click('#register [type="submit"]');
    await rp.waitForSelector("#mine:not([hidden])");
    await rp.close();
    await riverSide(BASE, true, null);
  }
}

// ---------------------------------------------------------------- DOWNLOAD's file: the River's Quick Backup opens it
{
  const qp = await ctx.newPage();
  watch(qp, "quick backup");
  await qp.goto(RIVER + "tools/");
  await qp.setInputFiles("#file", file);
  await qp.waitForSelector("#cards .sr-item");
  check((await qp.$$eval("#cards .sr-item .nm-n", (n) => n.map((x) => x.textContent))).includes("NORTH RIVER"), "the River's Quick Backup opens DOWNLOAD's file");
  await qp.close();
}

// ---------------------------------------------------------------- SEND TO FM-1 (the River's simulated FM-1: fake_fm1.mjs)
{
  const sc = await browser.newContext({ viewport: { width: 1440, height: 900 } });
  await sc.addInitScript(FAKE);
  const sp = await sc.newPage();
  watch(sp, "send");
  await sp.goto(SITE + "play/#editor");
  await sp.waitForSelector("#ed-n");
  const ex = [...new Uint8Array(await (await fetch(RIVER + "example.frasil")).arrayBuffer())];
  await sp.evaluate((a) => { window.__fm1.image = Uint8Array.from(a); }, ex);
  await sp.click("#ed-n .sr-open");
  await sp.waitForSelector("#ed-n.open .oc-keys");
  await sp.click('#ed-n > .sr [data-act="send"]');
  await sp.waitForSelector("#place-list", { timeout: 30000 });
  await sp.selectOption("#place-list li:nth-child(1) select", "11");
  await sp.click("#place-send");
  await sp.waitForSelector("#confirm[open]");
  await sp.click("#confirm-send");
  await sp.waitForFunction(() => /^DONE$/.test(document.querySelector("#place .dlg-say h3")?.textContent || ""), null, { timeout: 30000 });
  const ok = await sp.evaluate(() => { const a = FB.read(PB.parse(window.__fm1.image)).scenes[11]; return a.name === "NEW SHARD" && a.bytes.every((v, i) => v === FB.NEW_SHARD.bytes[i]); });
  check(ok, "SEND TO FM-1: NEW SHARD into slot 12 of the (simulated) FM-1, as the River sends");
  await sp.screenshot({ path: path.join(OUT, "play-send-done-1440.png") });
  await sc.close();
}

// ---------------------------------------------------------------- a phone
{
  const ph = await browser.newPage({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true });
  watch(ph, "phone");
  await ph.goto(SITE + "play/");
  await ph.waitForSelector("#ed-list .sr-item");
  await sleep(800);
  const wide = () => ph.evaluate(() => document.documentElement.scrollWidth);
  check(await wide() <= 390, `390 px, EDITOR: no horizontal scroll (${await wide()})`);
  await ph.screenshot({ path: path.join(OUT, "play-editor-390.png") });
  await ph.tap("#ed-1 .sr-open");
  await ph.waitForSelector("#ed-1.open .oc-keys");
  await sleep(900);
  check(await wide() <= 390, `...a card opened: no horizontal scroll (${await wide()})`);
  await ph.$eval("#ed-1 .oc-keys", (k) => k.scrollIntoView({ block: "center" }));
  await sleep(500);
  await ph.screenshot({ path: path.join(OUT, "play-editor-open-390.png") });
  await ph.evaluate(() => scrollTo(0, 0));
  await ph.screenshot({ path: path.join(OUT, "play-editor-head-390.png") });
  await ph.tap('#mode-switch [data-mode="emulator"]');
  await sleep(500);
  check(await ph.isVisible(".play-device"), "...EMULATOR on a phone");
  await ph.screenshot({ path: path.join(OUT, "play-emulator-390.png") });
  await ph.close();
}

check(!errors.length, "no console errors" + (errors.length ? ":\n    " + errors.join("\n    ") : ""));
await browser.close();
if (srv) srv.kill();
console.log(fails ? `${fails} failed` : "all ok", "· screenshots in", OUT);
process.exit(fails ? 1 : 0);
