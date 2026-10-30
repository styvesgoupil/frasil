// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The card laid out again (THE KEYS at the bottom, one column of what moves at the left), on the River itself: the
// service (a fresh database, port 8059, the River as tools/website.sh built it), Styves' NORTH RIVER, GLACIER and
// AURORA, a card opened and playing for real, the prototype (proto/keys.js, keys.css) put in, its states staged: the
// PNGs in png/.
//   node design/river-keys/scripts/shoot.mjs [only ...]
import fs from "fs";
import os from "os";
import path from "path";
import { spawn } from "child_process";
import { fileURLToPath } from "url";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");

const HERE = path.dirname(fileURLToPath(import.meta.url)), DIR = path.join(HERE, ".."), REPO = path.join(DIR, "../..");
const PNG = path.join(DIR, "png");
fs.mkdirSync(PNG, { recursive: true });
const only = process.argv.slice(2), want = (n) => !only.length || only.some((x) => n.startsWith(x));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// ---------------------------------------------------------------- the service, Mira, Styves
const port = "8059", BASE = `http://localhost:${port}`;
const db = path.join(fs.mkdtempSync(path.join(os.tmpdir(), "river-editor-db-")), "river.sqlite");
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
async function person(name, email, size = { width: 1440, height: 900 }, more = {}) {
  const ctx = await browser.newContext({ viewport: size, ...more });
  const page = await ctx.newPage();
  page.on("pageerror", (e) => console.log(`  ${name} pageerror: ${e.message}`));
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
  const img = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer());
  const ex = FB.read(PB.parse(img)).scenes.filter((s) => s.bytes && s.state !== "empty");
  const out = [];
  for (const [name, scene, description] of list) {
    const sc = ex.find((s) => s.name === scene && s.state === "saved") || ex.find((s) => s.name === scene);
    out.push(await RiverAPI.post("/api/shards", { name, description, layout: FB.S.magic.shard, firmware: FRASIL_VERSION, data: RiverAPI.b64(sc.bytes), from_slot: sc.slot }));
  }
  return out;
}, list);
const { page: mira } = await person("Mira K.", "mira@river.test");
const { page } = await person("Styves", "styves@river.test");
await add(page, [["AURORA", "AURORA", ""], ["GLACIER", "GLACIER", "A sequence in seven over a slow D major: the ice knocks, the river carries it."],
                 ["NORTH RIVER", "NORTH RIVER", "Where the river turns north: an arpeggio of ice over a slow bass."]]);

// ---------------------------------------------------------------- a card, opened, laid out again, playing
const CSS = fs.readFileSync(path.join(DIR, "proto/keys.css"), "utf8"), JS = fs.readFileSync(path.join(DIR, "proto/keys.js"), "utf8");
async function card(pg, name, o, width = 1440) {
  await pg.setViewportSize({ width, height: 900 });
  await pg.goto("about:blank");
  await pg.goto(BASE + "/yours/");
  await pg.waitForSelector("#list .sr-item");
  await pg.addStyleTag({ content: CSS });
  await pg.addScriptTag({ content: JS });
  await pg.evaluate(async ([name, o]) => {
    const art = [...document.querySelectorAll("#list .sr-item")].find((a) => a.querySelector(".nm-n").textContent === name);
    RiverCards.openRow(art, true, true);
    art.id = "k-card";
    const sc = RiverCards.card(art._key).sc;
    const keys = Keys.lay(art, sc, o);
    RiverPlayer.follow = true;
    await RiverPlayer.play(sc.bytes, art._key);
    Keys.run(keys);
  }, [name, o]);
  await pg.waitForTimeout(3200);
}
async function shot(pg, file) {
  await pg.mouse.move(2, 2);
  const box = await pg.$eval("#k-card", (e) => { const r = e.getBoundingClientRect(); return { y: r.top + scrollY, h: r.height }; });
  const vw = await pg.evaluate(() => innerWidth);
  await pg.screenshot({ path: path.join(PNG, file), fullPage: true, clip: { x: 0, y: Math.max(0, box.y - 12), width: vw, height: box.h + 24 } });
  console.log("  " + file);
}
if (want("40")) { await card(page, "NORTH RIVER", { track: "glint", mode: "latch" }); await shot(page, "40-glint-arp-1440.png"); }
if (want("41")) { await card(page, "NORTH RIVER", { track: "drone", mode: "latch", hover: 50, openLfo: [1] }); await shot(page, "41-drone-1440.png"); }
if (want("42")) { await card(page, "GLACIER", { track: "glint", mode: "latch", step: 4 }); await shot(page, "42-glint-seq-1440.png"); }
if (want("45")) { await card(page, "GLACIER", { track: "glint", mode: "latch", grid: true, hoverCell: [5, 67], selected: 7 }); await shot(page, "45-glint-grid-1440.png"); }
if (want("43")) { await card(page, "AURORA", { track: "bass", mode: "latch", step: 2 }); await shot(page, "43-bass-seq-1440.png"); }
if (want("44")) {
  const { page: ph } = await person("Styves", null, { width: 390, height: 844 }, { deviceScaleFactor: 2, isMobile: true, hasTouch: true });
  await ph.context().addCookies(await page.context().cookies());
  await card(ph, "NORTH RIVER", { track: "glint", mode: "latch" }, 390);
  await ph.evaluate(() => { const k = document.querySelector(".oc-keys"), b = k.querySelector(".k-board"); b.scrollLeft = b.scrollWidth * 0.42; scrollTo(0, k.getBoundingClientRect().top + scrollY - 120); });
  await ph.waitForTimeout(600);
  await ph.screenshot({ path: path.join(PNG, "44-keys-390.png") });
  console.log("  44-keys-390.png");
}
await browser.close();
srv.kill();
process.exit(0);
