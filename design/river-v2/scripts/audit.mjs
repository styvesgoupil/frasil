// FRASIL River v2: the audit's screenshots. The River as it is today (build/website/river, the service at RIVER_URL
// or localhost:8050), used like a player with a simulated FM-1 (website/tests/fake_fm1.mjs): every page and flow at
// 1440 and 390 wide. Not a test: it only takes pictures.
//   node design/river-v2/scripts/audit.mjs   (the service's log at RIVER_LOG for the verification link)
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { FAKE } from "../../../website/tests/fake_fm1.mjs";
const { chromium } = await import("/opt/homebrew/lib/node_modules/playwright/index.mjs");

const HERE = path.dirname(fileURLToPath(import.meta.url));
const OUT = path.join(HERE, "../audit");
fs.mkdirSync(OUT, { recursive: true });
const BASE = process.env.RIVER_URL || "http://localhost:8050";
const LOG = process.env.RIVER_LOG || "/tmp/river-audit/server.log";
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const PASSWORD = "drone in the fog";

const browser = await chromium.launch({ channel: "chrome" });
const DESK = { viewport: { width: 1440, height: 900 } };
const PHONE = { viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true };
async function person(ip, size, more = {}, fake = FAKE) {
  const ctx = await browser.newContext({ ...size, acceptDownloads: true, ...more });
  await ctx.route(BASE + "/**", (r) => r.continue({ headers: { ...r.request().headers(), "cf-connecting-ip": ip } }));
  if (fake) await ctx.addInitScript(fake);
  const page = await ctx.newPage();
  page.on("pageerror", (e) => console.log("  pageerror: " + e.message));
  return { ctx, page };
}
const shot = async (page, file, full = true) => {
  await page.waitForTimeout(800);
  await page.screenshot({ path: path.join(OUT, file), fullPage: full });
  console.log("  " + file);
};
const el = async (page, sel, file) => { await page.waitForTimeout(700); await page.locator(sel).first().screenshot({ path: path.join(OUT, file) }); console.log("  " + file); };
const example = async (page) => page.evaluate(async () => { window.__fm1.image = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer()); });
async function token(kind, email) {
  for (let i = 0; i < 40; i++) {
    const log = fs.readFileSync(LOG, "utf8"), at = log.lastIndexOf(`the email to ${email}`);
    const m = at >= 0 && new RegExp(`#${kind}=([\\w-]+)`).exec(log.slice(at));
    if (m) return m[1];
    await sleep(150);
  }
  return null;
}

// ================================================================ signed out, an empty River
{
  const { page } = await person("10.8.0.1", DESK);
  await page.goto(BASE + "/"); await shot(page, "a01-community-empty-1440.png");
  await page.goto(BASE + "/yours/"); await page.waitForSelector("#auth:not([hidden])"); await shot(page, "a02-yours-signedout-1440.png");
  await page.goto(BASE + "/tools/"); await shot(page, "a03-tools-1440.png");
  await page.goto(BASE + "/profile/"); await page.waitForTimeout(600); await shot(page, "a04-profile-signedout-1440.png");
  await page.goto(BASE + "/backup/"); await shot(page, "a05-quickbackup-empty-1440.png");
  await example(page);
  await page.click("#act-backup");
  await page.waitForFunction(() => document.querySelector("#river-name").textContent === "Your FM-1" && !document.body.classList.contains("is-busy"));
  await page.evaluate(() => scrollTo(0, 0));
  await shot(page, "a06-quickbackup-read-1440.png");
  for (const i of [2, 8]) await page.check(`#shard-${i} .keep input`);
  await page.waitForSelector("#bar.in");
  await shot(page, "a07-quickbackup-selected-1440.png", false);
  await page.click("#bar-place");
  await page.waitForSelector("#place[open]");
  await shot(page, "a08-quickbackup-place-1440.png", false);
  await page.click("#place [data-close]");
  await page.click("#settings .sh-open");
  await page.waitForTimeout(800);
  await page.evaluate(() => document.querySelector("#settings").scrollIntoView());
  await shot(page, "a09-quickbackup-settings-open-1440.png", false);
  await page.click("#act-restore-all");
  await page.waitForSelector("#whole[open]");
  await shot(page, "a10-quickbackup-restore-all-1440.png", false);
  await page.context().close();
}
{
  const { page } = await person("10.8.0.2", PHONE);
  await page.goto(BASE + "/"); await shot(page, "a01-community-empty-390.png");
  await page.goto(BASE + "/yours/"); await page.waitForSelector("#auth:not([hidden])"); await shot(page, "a02-yours-signedout-390.png");
  await page.goto(BASE + "/tools/"); await shot(page, "a03-tools-390.png");
  await page.goto(BASE + "/backup/#example"); await page.waitForTimeout(1500); await shot(page, "a06-quickbackup-example-390.png");
  await page.context().close();
}

// ================================================================ Ana: an account, her FM-1, her shards
const { ctx: A, page: a } = await person("10.8.0.3", DESK);
await a.goto(BASE + "/yours/#register");
await a.waitForSelector("#register:not([hidden])");
await a.fill('#register [name="display_name"]', "Styves");
await a.fill('#register [name="email"]', "styves@river.test");
await a.fill('#register [name="password"]', PASSWORD);
await a.click('#register [type="submit"]');
await a.waitForSelector("#mine:not([hidden])");
await shot(a, "b01-yours-new-unverified-1440.png");
const vt = await token("verify", "styves@river.test");
if (vt) { await a.goto(`${BASE}/#verify=${vt}`); await a.waitForURL(/\/yours\//); await a.waitForTimeout(1200); }
await shot(a, "b02-yours-new-verified-1440.png");
await example(a);
await a.click("#grab-go");
await a.waitForSelector("#g-2");
await a.evaluate(() => document.querySelector("#grab").scrollIntoView());
await shot(a, "b03-yours-grab-read-1440.png");
await a.check("#g-2 .keep input");
await a.check("#g-8 .keep input");
await a.click("#grab-keep");
await a.waitForFunction(() => document.querySelectorAll("#my-shards .shard").length === 2);
await a.fill("#grab-note", "Before the show");
await a.click("#grab-store");
await a.waitForFunction(() => document.querySelectorAll("#my-backups .bk").length === 1);
await a.evaluate(() => scrollTo(0, 0));
await shot(a, "b04-yours-2-shards-1440.png");
// a name, words, shared
const abyss = await a.evaluate(() => [...document.querySelectorAll("#my-shards .shard")].find((x) => x.querySelector(".sh-name").textContent === "ABYSS").id);
await a.click(`#${abyss} [aria-label="Name ABYSS, give it words"]`);
await a.waitForSelector("#edit[open]");
await a.fill('#f-edit [name="title"]', "Under the ice");
await a.fill('#f-edit [name="description"]', "Two fifths in D phrygian, a bass far below. Let it rise slowly, then FREEZE the choir.");
await a.fill('#f-edit [name="tags"]', "dark, slow");
await shot(a, "b05-edit-sheet-1440.png", false);
await a.click('#f-edit [type="submit"]');
await a.waitForTimeout(600);
await a.click(`#${abyss} [aria-label="Share ABYSS in the River"]`);
await a.waitForSelector(`#${abyss} .sh-flag`);
await a.evaluate(() => scrollTo(0, 0));
await shot(a, "b06-yours-shared-toast-1440.png", false);
await el(a, `#${abyss}`, "b07-own-card-closed.png");
await a.click(`#${abyss} .sh-open`);
await a.waitForSelector(`#${abyss}.open .ab-title`);
await a.evaluate((id) => document.getElementById(id).scrollIntoView(), abyss);
await shot(a, "b08-own-card-open-1440.png", false);
await el(a, `#${abyss}`, "b08b-own-card-open-whole.png");
await a.evaluate((id) => { document.querySelectorAll(`#${id} details.sec`).forEach((d, i) => { if (i < 2) d.open = true; }); }, abyss);
await a.waitForTimeout(600);
await el(a, `#${abyss}`, "b09-own-card-open-sections.png");

// many shards: the library at scale (the example's four saved scenes again, renamed), a few shared
const NAMES = ["GLACIER", "NORTHERN LIGHT", "FRAZIL DRIFT", "SLOW THAW", "BLACK ICE", "ST LAWRENCE", "FOG HORN", "WHITEOUT",
  "POLAR NIGHT", "SNOW BLIND", "ICE FOG", "BOREAL", "TUNDRA", "CALVING", "PACK ICE", "SILT", "UNDERTOW", "EDDY",
  "DEEP CURRENT", "LOW TIDE", "HOARFROST", "RIME", "LAKE EFFECT", "FIRST SNOW", "BRASH ICE", "SEA SMOKE"];
await a.evaluate(async (names) => {
  const A = RiverAPI, img = window.__fm1.image, sc = FB.read(PB.parse(img)).scenes.filter((x) => x.state !== "empty" && x.current);
  for (let i = 0; i < names.length; i++) {
    const s = sc[i % sc.length];
    const x = await A.post("/api/shards", { layout: FB.S.magic.scenes, firmware: FRASIL_VERSION, data: A.b64(s.bytes), from_slot: s.slot });
    await A.patch(`/api/shards/${x.id}`, { name: names[i], title: "", description: i % 3 ? "" : "A slow one for the end of a set.", tags: [] });
    if (i < 9) await A.post(`/api/shards/${x.id}/share`);
  }
}, NAMES);
await a.goto(BASE + "/yours/");
await a.waitForFunction(() => document.querySelectorAll("#my-shards .shard").length > 20);
await shot(a, "b10-yours-28-shards-1440.png");
await a.click("#send-several");
await a.waitForSelector("#place[open]");
await shot(a, "b11-send-several-28-1440.png", false);
await a.click("#place [data-close]");
await a.goto(BASE + "/profile/");
await a.waitForSelector("#account:not([hidden])");
await shot(a, "b12-profile-1440.png");
const stor = await A.storageState();

{
  const { page } = await person("10.8.0.4", PHONE, { storageState: stor });
  await page.goto(BASE + "/yours/");
  await page.waitForFunction(() => document.querySelectorAll("#my-shards .shard").length > 20);
  await shot(page, "b10-yours-28-shards-390.png", false);
  await page.evaluate(() => document.querySelector("#grab").scrollIntoView());
  await shot(page, "b13-yours-grab-390.png", false);
  await page.goto(BASE + "/profile/"); await page.waitForSelector("#account:not([hidden])"); await shot(page, "b12-profile-390.png");
  await page.goto(BASE + "/yours/");
  await page.waitForFunction(() => document.querySelectorAll("#my-shards .shard").length > 20);
  const id = await page.evaluate(() => document.querySelector("#my-shards .shard").id);
  await page.click(`#${id} .sh-open`);
  await page.waitForTimeout(900);
  await page.evaluate((x) => document.getElementById(x).scrollIntoView(), id);
  await shot(page, "b14-own-card-open-390.png", false);
  await page.context().close();
}

// ================================================================ Bea, signed out: the Community River, someone else's shard
const { ctx: B, page: b } = await person("10.8.0.5", DESK);
await b.goto(BASE + "/");
await b.waitForSelector(".shard");
await shot(b, "c01-community-1440.png");
await shot(b, "c01b-community-fold-1440.png", false);
const sid = abyss.slice(2);
await el(b, `#s-${sid}`, "c02-other-card-closed.png");
await b.click(`#s-${sid} .sh-open`);
await b.waitForSelector(`#s-${sid}.open .ab-by`);
await b.evaluate((id) => document.getElementById(id).scrollIntoView(), `s-${sid}`);
await shot(b, "c03-other-card-open-1440.png", false);
await el(b, `#s-${sid}`, "c03b-other-card-open-whole.png");
await b.click(`#s-${sid} [aria-label="Save ABYSS to my River"]`);
await b.waitForSelector("#invite[open]");
await shot(b, "c04-invite-sheet-1440.png", false);
await b.click("#invite [data-close]");
await b.click(`#s-${sid} [aria-label="Report ABYSS"]`);
await b.waitForSelector("#report[open]");
await shot(b, "c05-report-sheet-1440.png", false);
await b.click("#report [data-close]");
// the Send dialog, its states
await example(b);
await b.click(`#s-${sid} [aria-label="Send ABYSS to my FM-1"]`);
await b.waitForSelector("#place[open]");
await shot(b, "d01-send-opened-1440.png", false);
await b.click("#place-read");
await b.waitForFunction(() => !document.body.classList.contains("is-busy") && document.querySelector("#place-when").textContent.startsWith("as read"));
await shot(b, "d02-send-read-1440.png", false);
await b.selectOption("#place-list li:nth-child(1) select", "13");
await shot(b, "d03-send-placed-1440.png", false);
await b.evaluate(() => { window.__fm1.playing = true; });
await b.click("#place-send");
await b.waitForFunction(() => !document.body.classList.contains("is-busy"));
await shot(b, "d04-send-stopfirst-1440.png", false);
await b.evaluate(() => { window.__fm1.playing = false; });
await b.click("#place-send");
await b.waitForFunction(() => /^Done/.test(document.querySelector("#place-voice .voice-say").textContent), null, { timeout: 30000 });
await shot(b, "d05-send-done-1440.png", false);
await b.click("#place [data-close]");
await B.close();

// no FM-1 plugged in: Web MIDI with nothing on it
{
  const NONE = `Object.defineProperty(navigator, "requestMIDIAccess", { configurable: true, value: async () => ({ inputs: new Map(), outputs: new Map() }) });`;
  const { page } = await person("10.8.0.6", DESK, {}, NONE);
  await page.goto(BASE + "/");
  await page.waitForSelector(`#s-${sid}`);
  await page.click(`#s-${sid} [aria-label="Send ABYSS to my FM-1"]`);
  await page.waitForSelector("#place[open]");
  await page.click("#place-read");
  await page.waitForFunction(() => !document.body.classList.contains("is-busy"));
  await page.waitForTimeout(1500);
  await shot(page, "d06-send-no-fm1-1440.png", false);
  await page.context().close();
}
{
  const { page } = await person("10.8.0.7", PHONE);
  await page.goto(BASE + "/");
  await page.waitForSelector(".shard");
  await shot(page, "c01-community-390.png", false);
  await page.click(`#s-${sid} .sh-open`);
  await page.waitForTimeout(1000);
  await page.evaluate((id) => document.getElementById(id).scrollIntoView(), `s-${sid}`);
  await shot(page, "c03-other-card-open-390.png", false);
  await example(page);
  await page.click(`#s-${sid} [aria-label="Send ABYSS to my FM-1"]`);
  await page.waitForTimeout(800);
  await shot(page, "d01-send-opened-390.png", false);
  await page.context().close();
}
await A.close();
await browser.close();
console.log("done");
