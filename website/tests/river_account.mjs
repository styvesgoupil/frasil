// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The River with its service, in Chrome: two people and a simulated FM-1 (fake_fm1.mjs). Ana makes an account,
// confirms her email (the link from the service's log), signs out and in, gets two shards from her FM-1 (the dock's
// GET FROM FM-1: the FM-1 read by itself; its sixteen slots as a checklist, the new ones ticked, the factory's not, those
// in her River marked and not tickable, the empty ones dim) and stores the whole backup, renames a shard, gives it a
// description, shares it; it is in the
// Community River, where it plays. Bea, signed out, is invited to sign in to favorite it, reports it, opens its link
// (#s=), sends it to her FM-1 (counted), makes an account and favorites it (by Ana, in her FAVORITES), forgets her
// password (#reset=). Then the phone's width, the header (its part lit, the name to the profile), signing out on the
// profile; no console errors. (website/tests/river_lists.mjs: the lists themselves.)
//   node website/tests/river_account.mjs [OUT_DIR]       (SHOTS: Get from FM-1's screenshots, at 1440 and 390; default OUT_DIR)
// It starts the service itself (server/.venv's uvicorn, a fresh database, port RIVER_TEST_PORT or 8057, the River as
// tools/website.sh built it); RIVER_URL: a service already running instead (its log not read: no verification).
// PLAYWRIGHT: Playwright's index.mjs (default: the global one).
import fs from "fs";
import os from "os";
import path from "path";
import { spawn } from "child_process";
import { fileURLToPath } from "url";
import { FAKE } from "./fake_fm1.mjs";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");

const REPO = path.join(path.dirname(fileURLToPath(import.meta.url)), "../..");
const OUT = process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), "river-account-"));
const SHOTS = process.env.SHOTS || OUT;
fs.mkdirSync(OUT, { recursive: true });
fs.mkdirSync(SHOTS, { recursive: true });
let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "  ok" : "FAIL"} ${msg}`); if (!ok) fails++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// ---------------------------------------------------------------- the service
let BASE = process.env.RIVER_URL, srv = null, log = "";
if (!BASE) {
  const port = process.env.RIVER_TEST_PORT || "8057";
  BASE = `http://localhost:${port}`;
  try { await fetch(BASE + "/api/health"); console.log(`FAIL port ${port} is taken: another service answers there (RIVER_TEST_PORT)`); process.exit(1); }
  catch (_) { /* free: good */ }
  const db = path.join(OUT, "river-test.sqlite");
  for (const f of [db, db + "-wal", db + "-shm"]) fs.rmSync(f, { force: true });
  srv = spawn(path.join(REPO, "server/.venv/bin/uvicorn"), ["app:create_app", "--factory", "--port", port], {
    cwd: path.join(REPO, "server"),
    env: { ...process.env, RIVER_URL: BASE, COOKIE_SECURE: "0", RIVER_STATIC: path.join(REPO, "build/website/river"), RIVER_DB: db },
  });
  srv.stdout.on("data", (d) => { log += d; });
  srv.stderr.on("data", (d) => { log += d; });
  for (let i = 0; i < 60; i++) {
    try { if ((await fetch(BASE + "/api/health")).ok) break; } catch (_) { /* not yet */ }
    await sleep(250);
  }
}
const stop = () => { if (srv) srv.kill(); };
process.on("exit", stop);
/* the token of the last email of a kind to someone, from the service's log */
async function token(kind, email) {
  for (let i = 0; i < 40; i++) {
    const at = log.lastIndexOf(`the email to ${email}`);
    const m = at >= 0 && new RegExp(`#${kind}=([\\w-]+)`).exec(log.slice(at));
    if (m) return m[1];
    await sleep(150);
  }
  return null;
}

// ---------------------------------------------------------------- two people, each their own browser (and address)
const browser = await chromium.launch({ channel: "chrome" });
const errors = [];
async function person(name, ip, size = { width: 1280, height: 900 }, more = {}) {
  const ctx = await browser.newContext({ viewport: size, acceptDownloads: true, ...more });
  await ctx.route(BASE + "/**", (r) => r.continue({ headers: { ...r.request().headers(), "cf-connecting-ip": ip } }));   // (the River's only)
  await ctx.addInitScript(FAKE);
  const page = await ctx.newPage();
  page.on("console", (m) => { if (m.type() === "error" || m.type() === "warning") errors.push(`${name}: ${m.type()}: ${m.text()}`); });
  page.on("pageerror", (e) => { console.log(`    ${name} pageerror: ${e.message}`); errors.push(`${name}: pageerror: ${e.message}`); });
  return { ctx, page };
}
const shot = async (page, file) => { await page.waitForTimeout(700); await page.screenshot({ path: path.join(OUT, file) }); };
const shot2 = async (page, file) => { await page.mouse.move(2, 2); await page.waitForTimeout(600); await page.screenshot({ path: path.join(SHOTS, file) }); };
const example = async (page) => page.evaluate(async () => { window.__fm1.image = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()); });
const PASSWORD = "drone in the fog";

// ---------------------------------------------------------------- Ana: an account, confirmed
const { ctx: A, page: a } = await person("Ana", "10.9.0.1", { width: 1440, height: 900 });
await a.goto(BASE + "/yours/");
await a.waitForSelector("#auth:not([hidden])");
check(await a.textContent("[data-who]") === "Sign in", "signed out: the header says Sign in");
await a.click('#auth-tabs [data-tab="register"]');
await a.fill('#register [name="display_name"]', "Ana");
await a.fill('#register [name="email"]', "ana@river.test");
await a.fill('#register [name="password"]', PASSWORD);
await shot(a, "acc-1-register.png");
await a.click('#register [type="submit"]');
await a.waitForSelector("#mine:not([hidden])");
check(await a.isVisible("#vb") && (await a.textContent("[data-who]")) === "Ana", "registered: signed in as Ana, asked to confirm the email");
const vt = await token("verify", "ana@river.test");
check(!!vt, "the verification link is in the service's log");
if (vt) {
  await a.goto(`${BASE}/#verify=${vt}`);                              // (as from the email: the front page, then Your River)
  await a.waitForURL(/\/yours\//);
  await a.waitForFunction(() => /confirmed/.test(document.querySelector("#voice .voice-say").textContent));
  check(await a.isHidden("#vb"), "#verify=: the email confirmed (from the front page, through Your River)");
}
await shot(a, "acc-2-verified.png");
await a.goto(BASE + "/profile/");
await a.waitForSelector("#account:not([hidden])");
await a.click("#out-here");
await a.waitForSelector("#pf-out:not([hidden])");
await a.goto(BASE + "/yours/");
await a.waitForSelector("#auth:not([hidden])");
await a.fill('#signin [name="email"]', "ana@river.test");
await a.fill('#signin [name="password"]', PASSWORD);
await a.click('#signin [type="submit"]');
await a.waitForSelector("#mine:not([hidden])");
check(/Welcome back, Ana/.test(await a.textContent("#voice .voice-say")), "signed out, signed in again");

// ---------------------------------------------------------------- Ana gets shards from her FM-1
const pk = (page) => page.$$eval("#get .pk", (e) => e.map((x) => ({ slot: +x.dataset.slot, kind: x.className.match(/is-(\w+)/)[1], box: !!x.querySelector("input"),
  on: !!x.querySelector("input:checked"), tag: x.querySelector(".tag").textContent })));
await a.evaluate(() => Object.assign(window.__fm1, { connected: false }));
await a.click("#dock-get");
await a.waitForSelector("#get[open]");
await a.waitForFunction(() => /NOT CONNECTED/.test(document.querySelector("#get .dlg-say h3")?.textContent || ""));
check(/^GET FROM FM-1$/.test((await a.textContent("#get-h")).trim()) && await a.isVisible("#get-again"), "GET FROM FM-1: it reads the FM-1 by itself; not connected: Try again");
await shot2(a, "20b-get-not-connected-1440.png");
await example(a);
await a.evaluate(() => Object.assign(window.__fm1, { connected: true }));
await a.click("#get-again");
await a.waitForSelector("#get .pk");
let slots = await pk(a);
check(slots.length === 16 && slots.filter((x) => x.kind === "new").every((x) => x.box && x.on && x.tag === "NEW") && slots.filter((x) => x.kind === "new").length === 4,
      "the checklist: the 4 new scenes ticked (NEW)");
check(slots.filter((x) => x.kind === "factory").length === 7 && slots.filter((x) => x.kind === "factory").every((x) => x.box && !x.on && x.tag === "FACTORY"), "the factory's: tickable, unticked");
check(slots.filter((x) => x.kind === "empty").every((x) => !x.box) && slots.filter((x) => x.kind === "empty").length === 5, "the empty slots: dim, nothing to tick");
check(await a.isChecked("#get-whole") && (await a.getAttribute("#get-save", "aria-label")) === "Save 4 shards and the backup" && /GLACIER and AURORA, and the backup\./.test(await a.textContent("#get-say")),
      "Also store the whole backup; the button says what happens: " + await a.getAttribute("#get-save", "aria-label"));
await a.uncheck('#get .pk[data-slot="9"] input');
await a.uncheck('#get .pk[data-slot="10"] input');
await a.fill("#get-note", "Before the show");
check((await a.getAttribute("#get-save", "aria-label")) === "Save 2 shards and the backup" && /^Save 2 shards\s*and the backup$/.test((await a.textContent("#get-save")).trim()), "two ticked: Save 2 shards and the backup");
await shot2(a, "20-get-ready-1440.png");
await a.click("#get-save");
await a.waitForFunction(() => /SAVED/.test(document.querySelector("#get .dlg-say h3")?.textContent || ""));
check(/ABYSS and CATHEDRAL are in your shards, and the backup of \d\d:\d\d is stored\./.test(await a.textContent("#get .dlg-say")), "saved: " + (await a.textContent("#get .dlg-say p")));
await a.waitForFunction(() => document.querySelectorAll("#list .sr-item").length === 2 && /BACKUPS\s*1/i.test(document.querySelector("#pt").textContent));
await shot2(a, "21-get-done-1440.png");
await a.click("#get-see");
// again: those in her River now marked, not tickable
await a.click("#dock-get");
await a.waitForSelector("#get .pk");
slots = await pk(a);
check(slots[7].kind === "had" && !slots[7].box && slots[7].tag === "IN YOUR RIVER" && slots[8].kind === "had" && slots[9].kind === "new" && slots[9].on,
      "read again: ABYSS and CATHEDRAL IN YOUR RIVER (nothing to tick), GLACIER still NEW");
await a.click("#get .sheet-x");
await a.click('#pt a[href="#backups"]');
await a.waitForSelector("#my-backups .bu");
check(/ABYSS/.test(await a.textContent("#my-backups .bu")) && /Before the show/.test(await a.textContent("#my-backups .bu")), "the whole backup stored: BACKUPS 1, its slots' names and its note");
await a.click('#pt a[href="#shards"]');
await a.waitForSelector("#list .sr-item");

// ---------------------------------------------------------------- Ana names a shard, gives it a description, shares it
const rowNamed = (name) => a.evaluate((name) => [...document.querySelectorAll("#list .sr-item")].find((x) => x.querySelector(".nm-n").textContent === name).id, name);
const abyss = await rowNamed("ABYSS");
await a.click(`#${abyss} .sr-open`);
await a.click(`#${abyss} [aria-label="Rename: ABYSS"]`);
check(!(await a.$('#f-edit [name="title"]')) && !(await a.$('#f-edit [name="tags"]')), "the edit form: a name and a description only");
await a.fill('#f-edit [name="name"]', "deep one");
await a.fill('#f-edit [name="description"]', "Two fifths in D phrygian, a bass far below.\nLet it rise.");
await a.click('#f-edit [type="submit"]');
await a.waitForFunction(() => [...document.querySelectorAll("#list .nm-n")].some((x) => x.textContent === "DEEP ONE"));
check(await a.textContent(`#${abyss} .nm-d`) === "Two fifths in D phrygian, a bass far below." && await a.isVisible(`#${abyss}.open`),
      "renamed: DEEP ONE (upper-cased), its description's first line in its row, still opened");
check(await a.evaluate((id) => document.activeElement === document.querySelector(`#${id} [data-act="edit"]`), abyss), "saved: the focus back on its Rename (M4)");
const cath = await rowNamed("CATHEDRAL");
await a.click(`#${cath} .sr-open`);
await a.click(`#${cath} [aria-label="Rename: CATHEDRAL"]`);
await a.fill('#f-edit [name="name"]', "cathedral of ice");
await a.click('#f-edit [type="submit"]');
await a.waitForFunction(() => [...document.querySelectorAll("#list .nm-n")].some((x) => x.textContent === "CATHEDRAL OF ICE"));
await a.click(`#${cath} .sr-open`);
check(await a.evaluate((id) => { const r = document.getElementById(id), n = r.querySelector(".nm-n").getBoundingClientRect(), m = r.querySelector(".ms").getBoundingClientRect();
                                  return n.right <= m.left; }, cath), "sixteen characters: CATHEDRAL OF ICE, its row holds it whole");
const deep = abyss;
await a.click(`#${deep} [aria-label="Share: DEEP ONE"]`);
await a.click("dialog[open] .sheet-foot .btn-ice");
await a.waitForSelector(`#${deep} .sr .st:not(.dot)`, { state: "attached" });   // (in its row, closed; the card is open)
check(/SHARED/.test(await a.textContent(`#${deep} .sr .st:not(.dot)`)) && await a.getAttribute(`#${deep} [data-act="share"]`, "aria-pressed") === "true" &&
      await a.getAttribute(`#${deep} [data-act="share"]`, "aria-label") === "Shared: DEEP ONE", "shared: SHARED in its row, the toggle pressed, named as it reads (Shared: DEEP ONE)");
check(await a.evaluate((id) => document.activeElement === document.querySelector(`#${id} [data-act="share"]`), deep), "... the focus on it still (M4)");
await a.waitForSelector(`#${deep}.open .oc-desc`);
await shot(a, "acc-4-shards.png");
const sid = abyss.slice(2);

// ---------------------------------------------------------------- the River: it is there; it plays
await a.goto(BASE + "/");
await a.waitForSelector(`#s-${sid}`);
check(/Ana/.test(await a.textContent(`#s-${sid} .by`)) && await a.textContent(`#s-${sid} .nm-n`) === "DEEP ONE", "the Community River: DEEP ONE, by Ana");
await a.click(`#s-${sid} .play`);
await a.waitForFunction((id) => document.querySelector(`#s-${id}`).dataset.sound === "playing", sid, { timeout: 30000 });
check(true, "▶ in the River: it sounds");
await a.evaluate((id) => document.querySelector(`#s-${id}`).scrollIntoView({ block: "center" }), sid);
await shot(a, "acc-5-navigate.png");
await a.keyboard.press("Escape");
await a.click(`#s-${sid} .sr-open`);
await a.waitForSelector(`#s-${sid}.open .oc-meta`);
check(/#s=/.test(a.url()), "an opened card: its link in the address (#s=)");
await shot(a, "acc-6-open.png");

// ---------------------------------------------------------------- Bea: invited to favorite it, reports it, sends it, favorites it
const { ctx: B, page: b } = await person("Bea", "10.9.0.2");
await b.goto(BASE + "/");
await b.waitForSelector(`#s-${sid}`);
await b.click(`#s-${sid} .sr-open`);
await b.click(`#s-${sid} [aria-label="Favorite: DEEP ONE"]`);
await b.waitForSelector("#invite[open]");
check(true, "signed out: Favorite invites to sign in");
await shot(b, "acc-7-invite.png");
await b.click("#invite [data-close]");
await b.click(`#s-${sid} [aria-label="Report: DEEP ONE"]`);
await b.waitForSelector("#report[open]");
await b.fill("#report-why", "Just checking the reports.");
await b.click("#report-send");
await b.waitForFunction(() => /moderators will look/.test((document.querySelector(".toast") || {}).textContent || ""));
check(true, "reported");
await b.click(`#s-${sid} .sr-open`);                                   // (closed: its #s= leaves the address)
check(!/#s=/.test(b.url()), "closed: its link leaves the address");
await b.goto(`${BASE}/#s=${sid}`);
await b.waitForSelector(`#one-${sid}.open`);
check(await b.isVisible("#one"), "#s=: the shard from its link, first, opened");
await example(b);
await b.click(`#one-${sid} [aria-label="Send to FM-1: DEEP ONE"]`);
await b.waitForSelector("#place[open]");
await b.waitForSelector("#place-list", { timeout: 30000 });               // (it reads the FM-1 by itself)
await b.selectOption("#place-list li:nth-child(1) select", "13");
await b.click("#place-send");
await b.click("#confirm-send");
await b.waitForFunction(() => /^DONE$/.test(document.querySelector("#place .dlg-say h3")?.textContent || ""), null, { timeout: 30000 });
const onFM1 = await b.evaluate(() => FB.read(PB.parse(window.__fm1.image)).scenes[13].name);
check(onFM1 === "DEEP ONE", "sent to Bea's FM-1, slot 14: " + onFM1);
await shot(b, "acc-8-sent.png");
await b.click("#place-close");
await sleep(400);
const counted = await (await fetch(`${BASE}/api/public/shards/${sid}`)).json();
check(counted.sends === 1, `counted: sent once (${counted.sends})`);
await b.goto(BASE + "/yours/#register");
await b.waitForSelector("#register:not([hidden])");
await b.fill('#register [name="display_name"]', "Bea");
await b.fill('#register [name="email"]', "bea@river.test");
await b.fill('#register [name="password"]', PASSWORD);
await b.click('#register [type="submit"]');
await b.waitForSelector("#mine:not([hidden])");
await b.goto(BASE + "/");
await b.waitForSelector(`#s-${sid}`);
await b.click(`#s-${sid} .sr-open`);
await b.click(`#s-${sid} [aria-label="Favorite: DEEP ONE"]`);
await b.waitForFunction(() => /is in your favorites/.test((document.querySelector(".toast") || {}).textContent || ""));
check(await b.getAttribute(`#s-${sid} [aria-label="Favorite: DEEP ONE"]`, "aria-pressed") === "true", "FAVORITE: pressed (a bookmark, not a copy)");
await b.goto(BASE + "/yours/#favorites");
await b.waitForFunction(() => document.querySelectorAll("#list.fav .sr-item").length === 1);
check(/Ana/.test(await b.textContent("#list .sr-item .by")) && /SHARDS 0/i.test((await b.textContent("#pt")).replace(/\s+/g, " ")),
      "Bea's FAVORITES: DEEP ONE, by Ana (her own shards: none)");

// ---------------------------------------------------------------- Bea forgets her password
await b.goto(BASE + "/profile/");
await b.waitForSelector("#account:not([hidden])");
await b.click("#out-here");
await b.waitForSelector("#pf-out:not([hidden])");
await b.goto(BASE + "/yours/");
await b.waitForSelector("#auth:not([hidden])");
await b.click('#signin [data-go="forgot"]');
await b.fill('#f-forgot [name="email"]', "bea@river.test");
await b.click('#f-forgot [type="submit"]');
await b.waitForFunction(() => /on its way/.test(document.querySelector("#f-forgot .form-say").textContent));
const rt = await token("reset", "bea@river.test");
check(!!rt, "forgot: the reset link is in the service's log");
await b.goto(`${BASE}/#reset=${rt}`);
await b.waitForSelector("#f-reset:not([hidden])");
await b.fill('#f-reset [name="password"]', "a new drone at dawn");
await b.click('#f-reset [type="submit"]');
await b.waitForSelector("#mine:not([hidden])");
check(/new password is saved/.test(await b.textContent("#voice .voice-say")), "#reset=: a new password, signed in");

// ---------------------------------------------------------------- the phone; the header
const { page: p } = await person("Phone", "10.9.0.3", { width: 390, height: 844 }, { deviceScaleFactor: 2, isMobile: true, hasTouch: true, storageState: await A.storageState() });
await p.goto(BASE + "/yours/");
await p.waitForSelector("#list .sr-item");
await example(p);
await p.click("#dock-get");
await p.waitForSelector("#get .pk");
check(await p.$$eval("#get .pk.is-empty", (e) => e.every((x) => getComputedStyle(x).display === "none")) && await p.evaluate(() => document.documentElement.scrollWidth <= 390),
      "phone: Get from FM-1's checklist, the empty slots hidden, no horizontal scroll");
await shot2(p, "20-get-ready-390.png");
await p.click("#get-save");
await p.waitForFunction(() => /SAVED/.test(document.querySelector("#get .dlg-say h3")?.textContent || ""));
await shot2(p, "21-get-done-390.png");
await p.click("#get .sheet-x");
await p.evaluate(() => Object.assign(window.__fm1, { connected: false }));
await p.click("#dock-get");
await p.waitForSelector("#get-again");
await shot2(p, "20b-get-not-connected-390.png");
await p.click("#get .sheet-x");
for (const [url, file] of [["/", "acc-9-phone-navigate.png"], ["/yours/", "acc-10-phone-yours.png"], ["/backup/#example", "acc-11-phone-backup.png"],
                           ["/tools/", "acc-11b-phone-tools.png"], ["/profile/", "acc-11c-phone-profile.png"]]) {
  await p.goto(BASE + url);
  await p.waitForTimeout(1200);
  const sw = await p.evaluate(() => document.documentElement.scrollWidth);
  check(sw <= 390, `phone ${url}: no horizontal scroll (${sw})`);
  await shot(p, file);
}
await a.addInitScript(() => document.addEventListener("DOMContentLoaded", () => {   // (the header as the page is first drawn)
  window.whoFirst = document.querySelector("[data-who]").textContent;
  window.fontsFirst = document.fonts.check('700 17px "Barlow Semi Condensed"');
}));
await a.goto(BASE + "/yours/");
await a.waitForSelector("#mine:not([hidden])");
check(await a.textContent(".rvh-nav a[aria-current]") === "Your Private River", "Your Private River: lit in the header");
check(await a.evaluate(() => window.whoFirst) === "Ana", "signed in: the header's name there from the start (not Sign in, then the name)");
check(await a.getAttribute("[data-who]", "href") === "/profile/", "signed in: the header's name leads to the profile");
await a.goto(BASE + "/tools/");
check(await a.textContent(".rvh-nav a[aria-current]") === "Tools", "Tools: lit in the header");
check(await a.evaluate(() => window.whoFirst) === "Ana", "the next page too: the name from the start");
await shot(a, "acc-12-tools.png");

// ---------------------------------------------------------------- Ana's profile: signs out there
await a.click("[data-who]");
await a.waitForSelector("#account:not([hidden])");
check(await a.inputValue('#f-name [name="display_name"]') === "Ana", "the profile: her display name");
await shot(a, "acc-13-profile.png");
await a.click("#out-here");
await a.waitForSelector("#pf-out:not([hidden])");
check(await a.textContent("[data-who]") === "Sign in", "signed out: the header says so");
check(await a.getAttribute("[data-who]", "href") === "/yours/#signin", "signed out: Sign in leads to the form");

check(!errors.length, errors.length ? "console:\n  " + errors.join("\n  ") : "no console errors");
await browser.close();
if (srv) { const gone = new Promise((r) => srv.once("exit", r)); stop(); await gone; }
console.log(fails ? `${fails} FAILED` : "all ok");
process.exit(fails ? 1 : 0);
