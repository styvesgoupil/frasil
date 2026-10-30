// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// FRASIL River v2's lists, in Chrome, with the service: three people. Mira shares 27 shards; Styves keeps two, then a
// hundred, a backup and four favorites (one Mira takes back: "no longer shared"); Bea favorites and plays. Checked:
//   the title (SHARDS n · FAVORITES n · BACKUPS n, the switch), the dock (SEND ③ TO FM-1 once rows are ticked);
//   the rows' columns (KEY, GLINT, BASS: SEQ for AURORA's sequence), the toolbar only once a list is long, the
//   filters, the search, the sorts (one's own in the page; the Community River's NEWEST, MOST PLAYED, FAVORITES from
//   the service), paging by 24; the selection (Download, Share, Delete, Clear); an opened card's one action zone, one's
//   own (Send, Rename, Share/Shared, Copy link, Delete; "No description. Add one") and someone else's (Send, Favorite,
//   Copy link, Report); favorites (added, removed, the list, no longer shared); the backups; the profile's rows; a
//   disabled button in a dialog looks disabled (G6); phones with no horizontal scroll; no console errors. And phase A's
//   review: the tablet's column labels (M1), the 25th favorite (M2), the focus kept (M4), HIDDEN and "no longer shared"
//   where their column is gone (M5), names that hold their visible word (LOW 1), the selection cleared by a filter
//   (LOW 2), the Community River drawn before one's own ids come (LOW 3), the address naming the shard open (LOW 4),
//   the list's name (LOW 5), SEND TO FM-1 with nothing ticked: choose first (LOW 6), and a favorite no longer shared
//   counted neither played nor sent (item 8), a shared one counted sent.
// Its screenshots (OUT_DIR): each page at 1440 and at 390, for design/river-v2's mockups.
//   node website/tests/river_lists.mjs [OUT_DIR]       (SHOTS: the choose step's screenshots; default OUT_DIR)
// It starts the service itself (server/.venv's uvicorn, a fresh database, port RIVER_TEST_PORT or 8058, the River as
// tools/website.sh built it). PLAYWRIGHT: Playwright's index.mjs (default: the global one).
import fs from "fs";
import os from "os";
import path from "path";
import { spawn } from "child_process";
import { fileURLToPath } from "url";
import { FAKE } from "./fake_fm1.mjs";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");

const REPO = path.join(path.dirname(fileURLToPath(import.meta.url)), "../..");
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), "river-lists-")));
fs.mkdirSync(OUT, { recursive: true });
const SHOTS = process.env.SHOTS || OUT;
fs.mkdirSync(SHOTS, { recursive: true });
let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "  ok" : "FAIL"} ${msg}`); if (!ok) fails++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// ---------------------------------------------------------------- the service: its own, fresh
const port = process.env.RIVER_TEST_PORT || "8058", BASE = `http://localhost:${port}`;
try { await fetch(BASE + "/api/health"); console.log(`FAIL port ${port} is taken (RIVER_TEST_PORT)`); process.exit(1); } catch (_) { /* free */ }
const db = path.join(fs.mkdtempSync(path.join(os.tmpdir(), "river-lists-db-")), "river.sqlite");
let log = "";
const srv = spawn(path.join(REPO, "server/.venv/bin/uvicorn"), ["app:create_app", "--factory", "--port", port], {
  cwd: path.join(REPO, "server"),
  env: { ...process.env, RIVER_URL: BASE, COOKIE_SECURE: "0", RIVER_STATIC: path.join(REPO, "build/website/river"), RIVER_DB: db },
});
srv.stdout.on("data", (d) => { log += d; });
srv.stderr.on("data", (d) => { log += d; });
for (let i = 0; i < 60; i++) { try { if ((await fetch(BASE + "/api/health")).ok) break; } catch (_) { /* not yet */ } await sleep(250); }
process.on("exit", () => srv.kill());
async function token(kind, email) {
  for (let i = 0; i < 40; i++) {
    const at = log.lastIndexOf(`the email to ${email}`), m = at >= 0 && new RegExp(`#${kind}=([\\w-]+)`).exec(log.slice(at));
    if (m) return m[1];
    await sleep(150);
  }
  return null;
}

// ---------------------------------------------------------------- people, their browsers
const browser = await chromium.launch({ channel: "chrome" });
const errors = [];
const DESK = { width: 1440, height: 900 }, PHONE = { viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true };
async function person(name, ip, size = DESK, more = {}) {
  const ctx = await browser.newContext({ viewport: size, acceptDownloads: true, ...more });
  await ctx.route(BASE + "/**", (r) => r.continue({ headers: { ...r.request().headers(), "cf-connecting-ip": ip } }));
  await ctx.addInitScript(FAKE);
  const page = await ctx.newPage();
  page.on("console", (m) => { if (m.type() === "error" || m.type() === "warning") errors.push(`${name}: ${m.type()}: ${m.text()}`); });
  page.on("pageerror", (e) => { console.log(`    ${name} pageerror: ${e.message}`); errors.push(`${name}: pageerror: ${e.message}`); });
  return { ctx, page };
}
const shot = async (page, file, full = false) => {           // (a word for a moment left out of the picture)
  await page.waitForTimeout(700);
  await page.evaluate(() => document.querySelectorAll(".toast").forEach((t) => t.classList.remove("in")));
  await page.waitForTimeout(450);
  await page.screenshot({ path: path.join(OUT, file), fullPage: full });
};
const api = (page, m, p, b) => page.evaluate(([m, p, b]) => RiverAPI[m](p, b), [m, p, b]);
const PASSWORD = "drone in the fog";
async function signUp(page, name, email) {
  await page.goto(BASE + "/yours/#register");
  await page.waitForSelector("#register:not([hidden])");
  await page.fill('#register [name="display_name"]', name);
  await page.fill('#register [name="email"]', email);
  await page.fill('#register [name="password"]', PASSWORD);
  await page.click('#register [type="submit"]');
  await page.waitForSelector("#mine:not([hidden])");
  const t = await token("verify", email);
  await api(page, "post", "/api/auth/verify", { token: t });
}
/* shards from the example backup's scenes, named and described: it.scene (a scene's name there, saved first), it.bseq
 * (one whose bass plays its sequence), else the scenes in turn (it.i); whatever slots the example fills */
const addShards = (page, list) => page.evaluate(async (list) => {
  const img = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer());
  const ex = FB.read(PB.parse(img)).scenes.filter((s) => s.bytes && s.state !== "empty");
  const out = [];
  for (const [i, it] of list.entries()) {
    const named = it.scene && (ex.find((s) => s.name === it.scene && s.state === "saved") || ex.find((s) => s.name === it.scene));
    const sc = (it.bseq && ex.find((s) => s.bseq && s.bseq.on)) || named || ex[(it.i ?? i) % ex.length];
    out.push(await RiverAPI.post("/api/shards", { name: it.name, description: it.description || "", layout: FB.S.magic.shard,
                                                  firmware: FRASIL_VERSION, data: RiverAPI.b64(sc.bytes), from_slot: sc.slot }));
  }
  return out;
}, list);
const texts = (page, sel) => page.$$eval(sel, (e) => e.map((x) => x.textContent.trim()));
const words = (page, sel) => page.$$eval(sel, (e) => e.map((x) => x.textContent.replace(/\s+/g, " ").trim().toUpperCase()));
const fresh = async (page, url) => { await page.goto("about:blank"); await page.goto(url); };   // (a new page, not a new #hash)
/* every row of the Community River's list (24 a page; shards shared in the same second come in their ids' order) */
const allRows = async (page) => { while (await page.$("#more")) { const n = (await page.$$("#gal .sr-item")).length; await page.click("#more");
  await page.waitForFunction((n) => document.querySelectorAll("#gal .sr-item").length > n, n); } };
const noScroll = async (page, what) => { const w = await page.evaluate(() => document.documentElement.scrollWidth); check(w <= 390, `phone ${what}: no horizontal scroll (${w})`); };

// ---------------------------------------------------------------- Mira shares 27 shards
const COMM = [
  ["TADOUSSAC 4AM", "Belugas somewhere under the fog. GLINT on CHIME, very far."], ["NILAS", "Thirty-two steps, half of them rests."],
  ["GREASE ICE", "Warm, close voicing; turn BODY down for a thinner one."], ["KATABATIC", "The wind coming down the glacier. LFO 2 on BREATH."],
  ["POLYNYA", ""], ["BERGY BIT", ""], ["PANCAKE ICE", "Soft, round, a little out of tune on purpose."], ["ANCHOR ICE", ""],
  ["DIAMOND DUST", "An arpeggio like snow on a tin roof."], ["FJORD", ""], ["SASTRUGI", ""], ["MORAINE", "Church organ, far away, through the snow."],
  ["GROWLER", ""], ["ICE SHELF", ""], ["LEAD", ""], ["CREVASSE", ""], ["SERAC", ""], ["FIRN", "Old snow, slowly turning to ice."], ["NUNATAK", ""],
  ["ICEFALL", ""], ["CORNICE", ""], ["SPINDRIFT", ""], ["HUMMOCK", ""], ["FRAZIL", "Needles in the current."], ["SHUGA", ""], ["BRASH", ""],
  ["CALVING FRONT", ""], ["WHITE NIGHT", ""], ["LAST LIGHT", ""], ["NORTH WIND", ""],
];
const { page: m } = await person("Mira", "10.8.0.1");
await signUp(m, "Mira K.", "mira@river.test");
const mira = await addShards(m, COMM.map(([name, description], i) => ({ name, description, i: i + 2 })));
for (const x of mira.slice(0, 27).reverse()) await api(m, "post", `/api/shards/${x.id}/share`);   // (the first, the newest shared)
check(mira.length === 30, "Mira: 30 shards, 27 shared");

// ---------------------------------------------------------------- Styves: two shards and a backup
const { ctx: SC, page: s } = await person("Styves", "10.8.0.2");
await signUp(s, "Styves", "styves@river.test");
const two = await addShards(s, [{ name: "ABYSS", scene: "ABYSS", description: "Two fifths in D phrygian, a bass far below. Let it rise slowly, then FREEZE the choir." },
                                { name: "CATHEDRAL", scene: "CATHEDRAL", i: 1 }]);
await s.evaluate(async () => { const img = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer());
  await RiverAPI.post("/api/backups", { image: RiverAPI.b64(img), note: "Before the show", firmware: FRASIL_VERSION }); });
await s.goto(BASE + "/yours/");
await s.waitForSelector("#list .sr-item");
check((await words(s, "#pt .pt-t")).join(" | ") === "SHARDS 2 | BACKUPS 1", "the title: SHARDS 2 · BACKUPS 1 (no FAVORITES yet): " + (await words(s, "#pt .pt-t")).join(" | "));
check(await s.isVisible("#dock-get") && /^send to fm-1$/i.test((await s.textContent("#dock-send")).trim()), "the dock: GET FROM FM-1 | SEND TO FM-1");
check(!(await s.$("#tb .tb")) && /Get more from your FM-1/.test(await s.textContent("#list-end")), "two shards: no toolbar, one quiet line under the list");
check((await words(s, "#list .lh span")).join(",") === ",,NAME,WHAT PLAYS,KEY,GLINT,BASS,,ADDED ↓", "the columns: NAME, WHAT PLAYS, KEY, GLINT, BASS, ADDED (no DRONE)");
const abyssId = two[0].id, cathId = two[1].id;
check(/^Key [A-G]#? [A-Z ]+ · [A-Z0-9 #+-]+$/.test((await s.textContent(`#m-${abyssId} .kv`)).trim()) && /Two fifths in D phrygian/.test(await s.textContent(`#m-${abyssId} .nm-d`)),
      "a row: its name, its description's first line, KEY · chord");
await shot(s, "01-private-2-1440.png");

// ---------------------------------------------------------------- one's own card: its one action zone
await s.click(`#m-${abyssId} .sr-open`);
await s.waitForSelector(`#m-${abyssId}.open .oc-keys .k-board`);
const ownActs = async (id) => words(s, `#m-${id} .acts > *`);
check((await ownActs(abyssId)).join(",") === "SEND TO FM-1,RENAME,SHARE,DELETE", "one's own, opened: Send to FM-1, Rename, Share, Delete: " + (await ownActs(abyssId)).join(","));
check(/^Added \d+ \w+ \d{4} from slot \d\d of your FM-1 · FRASIL /.test(await s.textContent(`#m-${abyssId} .oc-meta`)), "... one quiet line: " + await s.textContent(`#m-${abyssId} .oc-meta`));
check(await s.$$eval(`#m-${abyssId} .sec .dh-map rect.on`, (e) => e.length) >= 10 && !(await s.$(`#m-${abyssId} .sec-key`)), "the device hints: a map of the panel, lit (no button-like keys)");
check((await s.$$eval(`#m-${abyssId} .acts > button`, (e) => e.map((x) => x.getAttribute("aria-label")))).join(" | ") === "Send to FM-1: ABYSS | Rename: ABYSS | Share: ABYSS | Delete: ABYSS",
      "each action named with its visible word first (Send to FM-1: ABYSS)");
await s.click(`#m-${abyssId} [aria-label="Share: ABYSS"]`);
await s.waitForSelector("dialog[open] .sheet-foot .btn-ice");
await s.click("dialog[open] .sheet-foot .btn:not(.btn-ice)");
await s.waitForFunction(() => !document.querySelector("dialog[open]"));
check(await s.getAttribute(`#m-${abyssId} [data-act="share"]`, "aria-pressed") !== "true", "Share, then Cancel: not shared");
await s.click(`#m-${abyssId} [aria-label="Share: ABYSS"]`);
await s.waitForSelector("dialog[open] .sheet-foot .btn-ice");
check(/Share ABYSS\?/.test(await s.textContent("dialog[open] .sheet-h")) && /publicly to the Community River/.test(await s.textContent("dialog[open] .sheet-lede")),
      "Share asks first: Share ABYSS? Sharing your shard publicly to the Community River (OK, Cancel)");
await s.click("dialog[open] .sheet-foot .btn-ice");
await s.waitForFunction((id) => document.querySelector(`#m-${id} [data-act="share"]`)?.getAttribute("aria-pressed") === "true", abyssId);
check(await s.getAttribute(`#m-${abyssId} [data-act="share"]`, "aria-label") === "Shared: ABYSS" && await s.getAttribute(`#m-${abyssId} [data-act="link"]`, "aria-label") === "Copy link: ABYSS" &&
      await s.evaluate((id) => document.activeElement === document.querySelector(`#m-${id} [data-act="share"]`), abyssId), "pressed: Shared: ABYSS, Copy link: ABYSS; the focus stays on it (M4)");
check((await ownActs(abyssId)).join(",") === "SEND TO FM-1,RENAME,SHARED,COPY LINK,DELETE" && await s.isVisible(`#m-${abyssId}.open`),
      "shared: SHARED (a toggle, pressed), Copy link once shared, still opened");
check(/SHARED/.test(await s.textContent(`#m-${abyssId} .st`)) && /shared in the Community River on .* under CC BY 4\.0/.test(await s.textContent(`#m-${abyssId} .oc-meta`)),
      "... SHARED in its row, the licence in its line");
await s.click(`#m-${abyssId} .sr-open`);
await s.click(`#m-${cathId} .sr-open`);
await s.waitForSelector(`#m-${cathId}.open .oc-none`);
check(/^No description\. Add one$/.test((await s.textContent(`#m-${cathId} .oc-none`)).trim()), "no description: No description. Add one");
await s.click(`#m-${cathId} .oc-add`);
await s.waitForSelector("#edit[open]");
check(await s.evaluate(() => document.activeElement.name === "description") && !(await s.$('#f-edit [name="title"]')) && !(await s.$('#f-edit [name="tags"]')),
      "Add one: the edit dialog, a name and a description only (no title, no tags), the description focused");
await s.fill('#f-edit [name="description"]', "Stone and air.\nThe second line stays in the card.");
await s.click('#f-edit [type="submit"]');
await s.waitForFunction((id) => /Stone and air/.test(document.querySelector(`#m-${id} .oc-desc`)?.textContent || ""), cathId);
check((await s.textContent(`#m-${cathId} .nm-d`)) === "Stone and air.", "saved: its description in the card, its first line in the row");
await s.click(`#m-${cathId} .sr-open`);
await s.waitForTimeout(600);

// ---------------------------------------------------------------- the phone: two shards
const { page: sp } = await person("Styves (phone)", "10.8.0.2", PHONE.viewport, { ...PHONE, storageState: await SC.storageState() });
await sp.goto(BASE + "/yours/");
await sp.waitForSelector("#list .sr-item");
await noScroll(sp, "Your Private River, 2 shards");
await shot(sp, "01-private-2-390.png");

// ---------------------------------------------------------------- the Community River
const { ctx: BC, page: b } = await person("Bea", "10.8.0.3");
await signUp(b, "Bea", "bea@river.test");
const sharedIds = mira.slice(0, 27).map((x) => x.id);
const fav2 = sharedIds[5], played2 = sharedIds[8], played1 = sharedIds[12];
await api(b, "post", `/api/favorites/${fav2}`);
for (const id of [played2]) await api(b, "post", `/api/public/shards/${id}/played`);
await api(s, "post", `/api/public/shards/${played2}/played`);
await api(s, "post", `/api/public/shards/${played1}/played`);
await s.goto(BASE + "/");
await s.waitForSelector("#gal .sr-item");
check((await words(s, "#pt .pt-t")).join() === "SHARDS 28" && /CC BY 4\.0/.test(await s.textContent("#pt .pt-sub")), "the Community River: SHARDS 28, the licence sentence under it");
check(!(await s.$(".rv-how")) && !(await s.$(".rv-hero")) && !(await s.$("#drift")), "no hero, no How the River works, no Good to know");
check((await s.$$("#gal .sr-item")).length === 24 && /^24 of 28$/.test((await s.textContent("#list-end p")).trim()), "24 a page: 24 of 28");
check(await s.getAttribute("#tb .tb-sort [data-sort=new]", "aria-pressed") === "true" && (await s.$$eval("#gal .sr-item", (e) => e[0].id)) === `s-${abyssId}`,
      "NEWEST by default: ABYSS, shared last, first");
check((await words(s, "#gal .lh span")).join(",") === ",NAME,WHAT PLAYS,KEY,GLINT,BASS,AUTHOR,,SHARED ↓", "its columns: … AUTHOR, the favorites, SHARED");
check((await texts(s, `#s-${sharedIds[0]} .by`))[0].includes("Mira K.") && /favorites: 0/.test(await s.textContent(`#s-${sharedIds[0]} .fav-n`)),
      "a row: its author, its favorites");
await s.click("#more");
await s.waitForFunction(() => document.querySelectorAll("#gal .sr-item").length === 28);
check(!(await s.$("#more")), "Show 4 more: all 28");
// a bass that plays its sequence: SEQ in BASS (the service's bseq), empty for the others
const withSeq = mira.slice(0, 27).find((x) => x.bseq), without = mira.slice(0, 27).find((x) => !x.bseq);
check(!!withSeq && (await s.textContent(`#s-${withSeq.id} .bq`)).includes("SEQ") && !(await s.$(`#s-${without.id} .bq`)),
      `the BASS column: SEQ for ${withSeq && withSeq.name} (its bass's sequence), empty for ${without.name}`);
// the sorts, from the service
await s.click("#tb .tb-sort [data-sort=played]");
await s.waitForFunction(() => /sort=played/.test(location.search) && !document.querySelector("#gal[aria-busy]"));
const byPlays = await s.$$eval("#gal .sr-item", (e) => e.slice(0, 2).map((x) => x.id));
check(byPlays[0] === `s-${played2}` && byPlays[1] === `s-${played1}`, "MOST PLAYED: two plays, then one");
await allRows(s);
// someone else's card: Send, Favorite, Copy link, Report
const other = sharedIds[3];
await s.click(`#s-${other} .sr-open`);
await s.waitForSelector(`#s-${other}.open .oc-keys .k-board`);
check((await words(s, `#s-${other} .acts > *`)).join(",") === "SEND TO FM-1,FAVORITE,COPY LINK,REPORT", "someone else's, opened: Send to FM-1, Favorite, Copy link, Report");
check(/^by Mira K\. · shared .* under CC BY 4\.0 · FRASIL /.test(await s.textContent(`#s-${other} .oc-meta`)) && !/0 plays|0 favorites/.test(await s.textContent(`#s-${other} .oc-meta`)),
      "... by, shared, licence, firmware; no zero counts: " + await s.textContent(`#s-${other} .oc-meta`));
check(/#s=/.test(s.url()), "opened: its link in the address");
await s.click(`#s-${sharedIds[4]} .sr-open`);
await s.waitForSelector(`#s-${sharedIds[4]}.open`);
await s.click(`#s-${other} .sr-open`);
check(new URL(s.url()).hash === `#s=${sharedIds[4]}`, "open A, open B, close A: the address names B, still open (LOW 4)");
await s.click(`#s-${sharedIds[4]} .sr-open`);
check(new URL(s.url()).hash === "", "... B closed: no shard in the address");
await s.click(`#s-${other} .sr-open`);
await s.waitForSelector(`#s-${other}.open`);
await s.click(`#s-${other} [aria-label="Favorite: ${COMM[3][0]}"]`);
await s.waitForFunction((id) => document.querySelector(`#s-${id} [aria-pressed="true"][aria-label^="Favorite"]`), other);
check(/favorites: 1/.test(await s.textContent(`#s-${other} .fav-n`)) && /in your favorites/.test(await s.textContent(".toast")), "Favorite: pressed, counted (1), said");
await s.evaluate((id) => scrollTo(0, document.getElementById(id).getBoundingClientRect().top + scrollY - 140), `s-${other}`);
await shot(s, "05-card-other-1440.png");
for (const id of [fav2, sharedIds[1], sharedIds[2]]) await api(s, "post", `/api/favorites/${id}`);
await s.click(`#s-${other} .sr-open`);
// the favorites sort
await s.click("#tb .tb-sort [data-sort=favorites]");
await s.waitForFunction(() => /sort=favorites/.test(location.search) && !document.querySelector("#gal[aria-busy]"));
check((await s.$$eval("#gal .sr-item", (e) => e[0].id)) === `s-${fav2}`, "FAVORITES: the most favorited first (2)");
await allRows(s);
// filters, search: the service's
await s.click("#tb .tb-sort [data-sort=new]");
await s.selectOption('#tb select[aria-label="GLINT"]', { label: "ARP" });
await s.waitForFunction(() => /type=1/.test(location.search) && !document.querySelector("#gal[aria-busy]"));
const arps = await texts(s, "#gal .sr-item .gt");
check(arps.length > 0 && arps.every((t) => /ARP/.test(t)), `GLINT: ARP, the service's filter (${arps.length} rows)`);
await s.selectOption('#tb select[aria-label="GLINT"]', "");
await s.selectOption('#tb select[aria-label="BASS"]', "seq");
await s.waitForFunction(() => /bass=seq/.test(location.search) && !document.querySelector("#gal[aria-busy]"));
const seqs = await s.$$eval("#gal .sr-item", (e) => e.map((x) => !!x.querySelector(".bq")));
check(seqs.length > 0 && seqs.every(Boolean), `BASS: SEQ (${seqs.length} rows, each with its sequence)`);
await s.selectOption('#tb select[aria-label="BASS"]', "");
await s.fill("#tb input[type=search]", "glacier");
await s.waitForFunction(() => /q=glacier/.test(location.search) && !document.querySelector("#gal[aria-busy]"));
check((await texts(s, "#gal .sr-item .nm-n")).join() === "KATABATIC", "the search: a word of a description (KATABATIC's glacier)");
await s.fill("#tb input[type=search]", "");
await s.waitForFunction(() => !/q=/.test(location.search) && !document.querySelector("#gal[aria-busy]"));
// one's own in the Community River: Send and Copy link only
await s.click(`#s-${abyssId} .sr-open`);
check((await words(s, `#s-${abyssId} .acts > *`)).join(",") === "SEND TO FM-1,COPY LINK", "one's own there: Send to FM-1, Copy link (no Favorite, no Report)");
await s.click(`#s-${abyssId} .sr-open`);
// the row playing (the waveform), for the screenshot
const engine = !!(await s.evaluate(async () => (await fetch("/emu/app.wasm", { method: "HEAD" })).ok));
if (engine) {
  await s.click(`#s-${abyssId} .play`);
  await s.waitForFunction((id) => document.querySelector(`#s-${id}`).dataset.sound === "playing", abyssId, { timeout: 30000 });
  await s.waitForTimeout(2500);
  check(await s.evaluate((id) => document.querySelector(`#s-${id} .ms-wave polyline`).getAttribute("points") !== "0,10 100,10", abyssId), "▶ in a row: its line of what plays moves");
}
await s.evaluate(() => scrollTo(0, 0));
await shot(s, "30-community-1440.png");
await s.keyboard.press("Escape");
// the tablet (761-960): the column labels over their columns (M1): KEY and AUTHOR's labels go with their cells
await s.setViewportSize({ width: 900, height: 900 });
await s.waitForTimeout(200);
const shownLabels = await s.$$eval("#gal .lh > span", (e) => e.filter((x) => getComputedStyle(x).display !== "none").map((x) => x.textContent.trim()));
const glintAt = await s.evaluate(() => { const l = [...document.querySelectorAll("#gal .lh > span")].find((x) => x.textContent.trim() === "GLINT").getBoundingClientRect(),
  c = document.querySelector("#gal .sr-item .gt").getBoundingClientRect(); return Math.abs(l.left - c.left); });
check(shownLabels.join(",") === ",NAME,WHAT PLAYS,GLINT,BASS,,SHARED ↓" && glintAt < 2, `the Community River at 900 px: ${shownLabels.join(",")}; GLINT over its badges (M1)`);
await s.setViewportSize(DESK);
// drawn first: one's own ids come late, the rows don't wait for them (LOW 3); then one's own lose Favorite and Report
await s.route("**/api/shards", async (r) => { await sleep(2500); r.fallback(); });
await fresh(s, BASE + "/");
await s.waitForSelector(`#s-${abyssId}`, { timeout: 2000 });
check(true, "the Community River drawn before one's own ids came (LOW 3)");
await s.waitForFunction((id) => !document.querySelector(`#s-${id} [data-act="fav"]`), abyssId, { timeout: 8000 });
check(!(await s.$(`#s-${abyssId} [data-act="report"]`)) && !!(await s.$(`#s-${sharedIds[0]} [data-act="fav"]`)), "... then one's own lose Favorite and Report, the others keep them");
await s.unroute("**/api/shards");
// signed out: Favorite invites to sign in
const { page: anon } = await person("Anon", "10.8.0.9");
await anon.goto(`${BASE}/`);
await anon.waitForSelector("#gal .sr-item");
await allRows(anon);
await anon.click(`#s-${other} .sr-open`);
await anon.click(`#s-${other} [aria-label="Favorite: ${COMM[3][0]}"]`);
await anon.waitForSelector("#invite[open]");
check(true, "signed out: Favorite invites to sign in");
await anon.click("#invite [data-close]");
// the phone: the Community River, someone else's card
const { page: bp } = await person("Bea (phone)", "10.8.0.3", PHONE.viewport, { ...PHONE, storageState: await BC.storageState() });
await bp.goto(BASE + "/");
await bp.waitForSelector("#gal .sr-item");
await noScroll(bp, "the Community River");
await shot(bp, "30-community-390.png");
await fresh(bp, `${BASE}/#s=${other}`);
await bp.waitForSelector(`#one-${other}.open .oc-body`);
await noScroll(bp, "someone else's card");
await shot(bp, "05-card-other-390.png");

// ---------------------------------------------------------------- favorites: the list; one no longer shared; one removed
await api(m, "post", `/api/shards/${sharedIds[2]}/unshare`);
await s.goto(BASE + "/yours/#favorites");
await s.waitForSelector("#list.fav .sr-item");
check((await words(s, "#pt .pt-t")).join(" | ") === "SHARDS 2 | FAVORITES 4 | BACKUPS 1" && (await s.getAttribute("#pt .pt-t.is-on", "href")) === "#favorites",
      "FAVORITES 4: the subject (#favorites)");
check((await words(s, "#list .lh span")).join(",") === ",,NAME,WHAT PLAYS,KEY,GLINT,BASS,AUTHOR,ADDED ↓", "favorites: their author");
check((await s.textContent("#lib-h")) === "Your favorites", "the list's name: Your favorites (LOW 5)");
check(/no longer shared/.test(await s.textContent(`#f-${sharedIds[2]} .by`)) && !/no longer/.test(await s.textContent(`#f-${other} .by`)), "a favorite its author took back: no longer shared");
await s.click(`#f-${sharedIds[2]} .sr-open`);
check((await words(s, `#f-${sharedIds[2]} .acts > *`)).join(",") === "SEND TO FM-1,FAVORITE" && /no longer shared/.test(await s.textContent(`#f-${sharedIds[2]} .oc-meta`)),
      "... opened: Send to FM-1 and Favorite only (no link, no report), no longer shared in its line");
// a favorite no longer shared: played and sent, counted neither (item 8); one still shared: counted sent once
const counted = [];
s.on("request", (r) => { const m = /\/api\/public\/shards\/([\w-]+)\/(played|sent)$/.exec(r.url()); if (m) counted.push(`${m[2]}:${m[1]}`); });
if (engine) {
  await s.click(`#f-${sharedIds[2]} .play`);
  await s.waitForFunction((id) => document.querySelector(`#f-${id}`).dataset.sound === "playing", sharedIds[2], { timeout: 30000 });
  await s.keyboard.press("Escape");
}
await s.click(`#f-${sharedIds[2]} .sr-open`);
await s.evaluate(async () => { window.__fm1.image = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()); });
for (const id of [sharedIds[2], other]) {
  await s.click(`#f-${id} .sr-open`);
  await s.click(`#f-${id} [data-act="send"]`);
  await s.waitForSelector("#place-list", { timeout: 30000 });
  await s.selectOption("#place-list li:nth-child(1) select", "13");
  await s.click("#place-send");
  await s.click("#confirm-send");
  await s.waitForSelector("#place-close", { timeout: 30000 });
  await s.click("#place-close");
  await s.click(`#f-${id} .sr-open`);
}
await sleep(500);
check(!counted.some((c) => c.endsWith(`:${sharedIds[2]}`)) && counted.filter((c) => c === `sent:${other}`).length === 1,
      `a favorite no longer shared: neither /played nor /sent; one shared: /sent once (${counted.join(" ") || "none"}) (item 8, M3)`);
await s.click(`#f-${sharedIds[2]} .sr-open`);
await shot(s, "favorites-1440.png");
// the tablet: "no longer shared" under its name, where the author's column is gone (M5)
await s.click(`#f-${sharedIds[2]} .sr-open`);
await s.setViewportSize({ width: 900, height: 900 });
await s.waitForTimeout(200);
check(await s.isVisible(`#f-${sharedIds[2]} .nm-mk`) && /NO LONGER SHARED/.test(await s.textContent(`#f-${sharedIds[2]} .nm-mk`)) && !(await s.isVisible(`#f-${other} .nm-mk`)),
      "at 900 px: NO LONGER SHARED under its name (M5)");
await s.setViewportSize(DESK);
await s.click(`#f-${sharedIds[1]} .sr-open`);
await s.click(`#f-${sharedIds[1]} [aria-label^="Favorite"]`);
await s.waitForFunction((id) => !document.querySelector(`#f-${id}`), sharedIds[1]);
check((await words(s, "#pt .pt-t")).join(" | ") === "SHARDS 2 | FAVORITES 3 | BACKUPS 1" && (await api(s, "get", "/api/favorites")).items.length === 3,
      "unfavorited: it leaves the list; FAVORITES 3");
check(await s.evaluate(() => document.activeElement && document.activeElement.classList.contains("nm-n")), "... the focus on a row's name, not lost (M4)");
// 25 favorites: one taken out, the 25th drawn in its place (24 still shown) (M2)
const extra = sharedIds.filter((id) => ![fav2, other, sharedIds[1], sharedIds[2]].includes(id)).slice(0, 22);
for (const id of extra) await api(s, "post", `/api/favorites/${id}`);
await fresh(s, BASE + "/yours/#favorites");
await s.waitForSelector("#list.fav .sr-item");
check((await s.$$("#list .sr-item")).length === 24 && /^24 of 25$/.test((await s.textContent("#list-end p")).trim()), "25 favorites: 24 of 25");
const firstFav = await s.$eval("#list .sr-item", (e) => e.id);
await s.click(`#${firstFav} .sr-open`);
await s.click(`#${firstFav} [data-act="fav"]`);
await s.waitForFunction((id) => !document.getElementById(id), firstFav);
check((await s.$$("#list .sr-item")).length === 24 && !(await s.$("#more")), "one taken out: the 25th drawn in its place, 24 shown, none left behind (M2)");
for (const id of extra) await api(s, "del", `/api/favorites/${id}`);
await fresh(s, BASE + "/yours/#favorites");
await s.waitForSelector("#list.fav .sr-item");
await fresh(sp, BASE + "/yours/#favorites");
await sp.waitForSelector("#list.fav .sr-item");
await noScroll(sp, "favorites");
await shot(sp, "favorites-390.png");

// ---------------------------------------------------------------- the backups
await s.click('#pt a[href="#backups"]');
await s.waitForSelector("#my-backups .bu");
check(/Before the show/.test(await s.textContent("#my-backups .bu")) && /^\d\d [A-Z]/.test((await s.textContent("#my-backups .bu-names")).trim()) &&
      (await words(s, "#my-backups .bu .bu-acts > *")).join(",") === "RESTORE TO FM-1,DOWNLOAD,DELETE", "BACKUPS: a row, its note, its slots' names; Restore to FM-1, Download, Delete");
await shot(s, "02c-private-backups-1440.png");
// SEND TO FM-1 with nothing ticked (here, on BACKUPS): choose first, never all of them (LOW 6)
await s.evaluate(async () => { window.__fm1.image = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()); window.__fm1.events = []; });
await s.click("#dock-send");
await s.waitForSelector("#place[open] #choose-go");
check(/^SEND TO FM-1\s*CHOOSE UP TO 16$/.test((await s.textContent("#place-h")).trim()) && (await words(s, "#place .ch-tabs button")).join(" | ") === "SHARDS 2 | FAVORITES 3" &&
      await s.isDisabled("#choose-go") && await s.evaluate(() => !window.__fm1 || !window.__fm1.events.includes("backup")), "nothing ticked: SEND TO FM-1 · CHOOSE UP TO 16 (SHARDS 2 · FAVORITES 3), nothing read yet");
await s.click("#place .ch-r >> nth=0");
await s.click("#place .ch-tabs button >> nth=1");
await s.click("#place .ch-r >> nth=0");
check((await s.textContent("#choose-n")) === "2" && await s.isEnabled("#choose-go"), "two chosen, a shard and a favorite: Choose their slots");
await s.mouse.move(2, 2);
await s.waitForTimeout(600);
await s.screenshot({ path: path.join(SHOTS, "15-send-choose-1440.png") });
await s.click("#choose-go");
await s.waitForSelector("#place-list", { timeout: 30000 });
check((await s.$$("#place-list .cd")).length === 2, "... then their slots, the FM-1 read by itself");
await s.click("#place .sheet-x");
await fresh(sp, BASE + "/yours/#backups");
await sp.waitForSelector("#my-backups .bu");
await noScroll(sp, "backups");
await shot(sp, "02c-private-backups-390.png");

// ---------------------------------------------------------------- a hundred shards: the toolbar, filters, sorts, paging, the selection
const MORE = ["CATHEDRAL OF ICE", "GLACIER", "NORTHERN LIGHT", "FRAZIL DRIFT", "AURORA", "SLOW THAW", "BLACK ICE", "ST LAWRENCE", "FOG HORN", "WHITEOUT",
  "POLAR NIGHT", "ICE FOG", "BOREAL", "CALVING", "PACK ICE", "UNDERTOW", "EDDY", "HOARFROST", "LAKE EFFECT", "FIRST SNOW", "SEA SMOKE", "NEEDLE ICE"];
const W = ["DRIFT", "FLOE", "RIME", "SLUSH", "THAW", "GLAZE", "SHELF", "CREST"];
const names = [...MORE, ...Array.from({ length: 98 - MORE.length }, (_, i) => `${W[i % W.length]} ${String(i + 1).padStart(2, "0")}`)];
const descs = { "NORTHERN LIGHT": "Lydian shimmer, the arpeggio high and slow. OCT +1 on GLINT.", "SLOW THAW": "For the end of a set: let it ring for ten minutes.",
                "ST LAWRENCE": "The river at Tadoussac, four in the morning.", "POLAR NIGHT": "Dark, close, warm in the middle.", CALVING: "Low and broken; PUMP on the beat." };
// (AURORA: one whose bass plays its sequence, as the example's own)
const hundred = await addShards(s, names.map((name, i) => ({ name, description: descs[name] || "", bseq: name === "AURORA", scene: name === "GLACIER" ? name : null, i: i + 4 })));
for (const x of hundred.filter((x) => descs[x.name])) await api(s, "post", `/api/shards/${x.id}/share`);
await s.goto(BASE + "/yours/");
await s.waitForSelector("#tb .tb");
check((await words(s, "#pt .pt-t")).join(" | ") === "SHARDS 100 | FAVORITES 3 | BACKUPS 1", "a hundred: SHARDS 100");
check((await s.$$("#list .sr-item")).length === 24 && /^24 of 100$/.test((await s.textContent("#list-end p")).trim()) && /100 shards/i.test(await s.textContent("#tb .tb-n")),
      "the toolbar appears; 24 of 100");
check((await words(s, "#tb .f-l")).join(",") === "KEY,SCALE,GLINT,BASS,SHARED,SORT", "the filters in the FM-1's words: KEY, SCALE, GLINT, BASS, SHARED; a sort");
await s.click("#more");
await s.waitForFunction(() => document.querySelectorAll("#list .sr-item").length === 48);
check(/^48 of 100$/.test((await s.textContent("#list-end p")).trim()), "Show 24 more: 48 of 100");
await s.selectOption('#tb select[aria-label="SHARED"]', "yes");
await s.waitForFunction(() => document.querySelectorAll("#list .sr-item").length === 6);
check((await texts(s, "#list .sr-item .st:not(.dot)")).every((t) => /SHARED/.test(t)), "SHARED: the 6 shared (in the page, no request)");
await s.selectOption('#tb select[aria-label="SHARED"]', "");
await s.selectOption('#tb select[aria-label="BASS"]', "seq");
await s.waitForTimeout(150);
const ownSeq = await s.$$eval("#list .sr-item", (e) => e.map((x) => !!x.querySelector(".bq")));
const wantSeq = (await api(s, "get", "/api/shards")).items.filter((x) => x.bseq).length;
check(ownSeq.length === Math.min(24, wantSeq) && ownSeq.every(Boolean), `BASS: SEQ, in the page (${wantSeq} with the bass's sequence)`);
await s.selectOption('#tb select[aria-label="BASS"]', "");
await s.selectOption('#tb select[aria-label="Order"]', "name");
await s.waitForTimeout(150);
const byName = await texts(s, "#list .sr-item .nm-n");
check(byName[0] === "ABYSS" && byName.join() === [...byName].sort((a, c) => a.localeCompare(c)).join() && (await words(s, "#list .lh .is-sort"))[0] === "NAME ↓", "the sort: NAME, A to Z");
await s.selectOption('#tb select[aria-label="Order"]', "new");
await s.fill("#tb input[type=search]", "tadoussac");
await s.waitForFunction(() => document.querySelectorAll("#list .sr-item").length === 1);
check((await texts(s, "#list .sr-item .nm-n"))[0] === "ST LAWRENCE", "the search: names and descriptions (ST LAWRENCE's Tadoussac)");
await s.fill("#tb input[type=search]", "nothing like this");
await s.waitForFunction(() => /No shard matches/.test(document.querySelector("#list-end").textContent));
await s.click("#list-end .txt-b");
await s.waitForFunction(() => document.querySelectorAll("#list .sr-item").length === 24);
check(await s.inputValue("#tb input[type=search]") === "", "No shard matches: Clear the filters");
// the selection
const first3 = await s.$$eval("#list .sr-item", (e) => e.slice(1, 4).map((x) => x.id));
for (const id of first3) await s.check(`#${id} .ck input`);
check(/^send 3 to fm-1$/i.test((await s.textContent("#dock-send")).trim().replace(/\s+/g, " ")) && await s.evaluate(() => document.querySelector("#dock-send").classList.contains("is-hot")),
      "three ticked: SEND ③ TO FM-1, ice");
check((await words(s, "#list .selbar > *")).filter(Boolean).join(",") === "3 SELECTED,DOWNLOAD,SHARE,DELETE,CLEAR" && !(await s.$("#list .lh")),
      "the selection's bar in the column labels' place: Download, Share, Delete, Clear");
await s.selectOption('#tb select[aria-label="GLINT"]', { label: "ARP" });
await s.waitForTimeout(150);
check(/^send to fm-1$/i.test((await s.textContent("#dock-send")).trim()) && !(await s.$("#list .selbar")), "a filter changed: the selection cleared, the dock as it was (LOW 2)");
await s.selectOption('#tb select[aria-label="GLINT"]', "");
await s.waitForTimeout(150);
for (const id of first3) await s.check(`#${id} .ck input`);
const [dl] = await Promise.all([s.waitForEvent("download"), s.click("#list .selbar >> text=Download")]);
const pack = JSON.parse(fs.readFileSync(await dl.path(), "utf8"));
check(pack.format === "frasil-scenes" && pack.scenes.length === 3, `Download: a scene package of the 3 (${dl.suggestedFilename()})`);
// the playing row, the ticked ones: the screenshot
if (engine) {
  await s.click(`#list .sr-item >> nth=1 >> .play`);
  await s.waitForFunction((id) => document.getElementById(id).dataset.sound === "playing", first3[0], { timeout: 30000 });
  await s.waitForTimeout(2500);
}
await s.evaluate(() => scrollTo(0, 0));
await shot(s, "02-private-100-1440.png", true);
await s.keyboard.press("Escape");
await s.click("#list .selbar >> text=Clear");
check(/^send to fm-1$/i.test((await s.textContent("#dock-send")).trim()) && await s.isVisible("#list .lh"), "Clear: the column labels again, the dock as it was");
check(await s.evaluate(() => document.activeElement === document.querySelector("#list .sr-item .nm-n")), "... the focus on the first row's name (M4)");
await fresh(sp, BASE + "/yours/");
await sp.waitForSelector("#tb .tb");
await sp.click("#dock-send");
await sp.waitForSelector("#place[open] #choose-go");
await sp.click("#place .ch-r >> nth=1");
await noScroll(sp, "choose the shards to send");
await sp.mouse.move(2, 2);
await sp.waitForTimeout(600);
await sp.screenshot({ path: path.join(SHOTS, "15-send-choose-390.png") });
await sp.click("#place .sheet-x");
await sp.check(`#${first3[0]} .ck input`);
await sp.check(`#${first3[1]} .ck input`);
await noScroll(sp, "Your Private River, 100 shards");
await shot(sp, "02-private-100-390.png");

// ---------------------------------------------------------------- one's own card, opened: DRONE; AURORA's BASS
await s.reload();
await s.waitForSelector("#tb .tb");
await s.selectOption('#tb select[aria-label="Order"]', "old");
await s.waitForSelector(`#m-${abyssId}`);
await s.evaluate((id) => document.getElementById(id).scrollIntoView({ block: "start" }), `m-${abyssId}`);
await s.click(`#m-${abyssId} .sr-open`);
await s.waitForSelector(`#m-${abyssId}.open .oc-keys .k-board`);
await s.click(`#m-${abyssId} .sec summary >> nth=0`);
await s.evaluate((id) => scrollTo(0, document.getElementById(id).getBoundingClientRect().top + scrollY - 120), `m-${abyssId}`);
await shot(s, "04-card-own-1440.png");
await s.click(`#m-${abyssId} .sr-open`);
const aurora = hundred.find((x) => x.name === "AURORA");
await s.fill("#tb input[type=search]", "aurora");
await s.waitForSelector(`#m-${aurora.id}`);
await s.click(`#m-${aurora.id} .sr-open`);
await s.waitForSelector(`#m-${aurora.id}.open .oc-keys .k-board`);
const bassSec = s.locator(`#m-${aurora.id} .sec`).filter({ has: s.locator(".sec-name", { hasText: /^BASS$/ }) });
await bassSec.locator("summary").click();
check(/^BASS's sequence: \d+ steps/.test(await s.getAttribute(`#m-${aurora.id} .sh-bass .scr svg`, "aria-label")) && (await bassSec.locator(".pg-k").first().textContent()).includes("SEQ"),
      "AURORA opened: BASS at the left, its sequence's line of time; each page names its button");
await s.evaluate((id) => scrollTo(0, document.getElementById(id).getBoundingClientRect().top + scrollY - 120), `m-${aurora.id}`);
await shot(s, "04b-card-bass-1440.png");
await fresh(sp, BASE + "/yours/");
await sp.waitForSelector("#tb .tb");
await sp.selectOption('#tb select[aria-label="Order"]', "old");
await sp.waitForSelector(`#m-${abyssId}`);
await sp.click(`#m-${abyssId} .sr-open`);
await sp.waitForSelector(`#m-${abyssId}.open .oc-keys .k-board`);
await sp.click(`#m-${abyssId} .sec summary >> nth=0`);
await sp.evaluate((id) => scrollTo(0, document.getElementById(id).getBoundingClientRect().top + scrollY - 60), `m-${abyssId}`);
await noScroll(sp, "one's own card");
await shot(sp, "04-card-own-390.png");

// ---------------------------------------------------------------- G6: a disabled button in a dialog looks disabled
await s.evaluate(async () => { window.__fm1.image = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()); });
await s.click(`#m-${aurora.id} [aria-label="Send to FM-1: AURORA"]`);
await s.waitForSelector("#place-list", { timeout: 30000 });
check(await s.isDisabled("#place-send") && Number(await s.$eval("#place-send", (e) => getComputedStyle(e).opacity)) < 0.5, "G6: a dialog's disabled Send looks disabled");
await s.click("#place .sheet-x");

// ---------------------------------------------------------------- the profile
await s.goto(BASE + "/profile/");
await s.waitForSelector("#account:not([hidden])");
check((await words(s, ".pt-t")).join() === "ACCOUNT" && /100 shards, 6 shared/.test(await s.textContent("#pf-sub")) && (await s.$$("#account .pr")).length === 7,
      "the profile: ACCOUNT, one column of seven rows: " + await s.textContent("#pf-sub"));
await s.click("#name-go");
check(await s.isVisible("#f-name") && await s.evaluate(() => document.activeElement.name === "display_name"), "CHANGE: its field, in its row");
await s.click("#f-name [data-cancel]");
await shot(s, "41-profile-1440.png");
await sp.goto(BASE + "/profile/");
await sp.waitForSelector("#account:not([hidden])");
await noScroll(sp, "the profile");
await shot(sp, "41-profile-390.png");

check(!errors.length, errors.length ? "console:\n  " + errors.join("\n  ") : "no console errors");
await browser.close();
const gone = new Promise((r) => srv.once("exit", r));
srv.kill();
await gone;
console.log(fails ? `${fails} FAILED` : "all ok");
process.exit(fails ? 1 : 0);
