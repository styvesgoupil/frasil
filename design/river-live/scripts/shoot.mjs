// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The live view's mockups, on the River itself: the service (server/.venv's uvicorn, a fresh database, port 8059, the
// River as tools/website.sh built it), Styves' library (shards of the example backup), a card opened in Your Private
// River, the prototype (proto/live.js, live.css) put in, then fed what the engine played (data/live.json:
// capture.mjs), frame by frame: PNGs at 1440, 1100 and 390, and short films (ffmpeg) of a card playing.
//   node design/river-live/scripts/shoot.mjs [only ...]
import fs from "fs";
import os from "os";
import path from "path";
import { spawn, execFileSync } from "child_process";
import { fileURLToPath } from "url";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");

const HERE = path.dirname(fileURLToPath(import.meta.url)), DIR = path.join(HERE, ".."), REPO = path.join(DIR, "../..");
const PNG = path.join(DIR, "png");
fs.mkdirSync(PNG, { recursive: true });
const LIVE = JSON.parse(fs.readFileSync(path.join(DIR, "data/live.json"), "utf8"));
const only = process.argv.slice(2);
const want = (name) => !only.length || only.some((x) => name.startsWith(x));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// ---------------------------------------------------------------- the service, Styves, his shards
const port = "8059", BASE = `http://localhost:${port}`;
const db = path.join(fs.mkdtempSync(path.join(os.tmpdir(), "river-live-db-")), "river.sqlite");
let log = "";
const srv = spawn(path.join(REPO, "server/.venv/bin/uvicorn"), ["app:create_app", "--factory", "--port", port], {
  cwd: path.join(REPO, "server"),
  env: { ...process.env, RIVER_URL: BASE, COOKIE_SECURE: "0", RIVER_STATIC: path.join(REPO, "build/website/river"), RIVER_DB: db },
});
srv.stdout.on("data", (d) => { log += d; });
srv.stderr.on("data", (d) => { log += d; });
process.on("exit", () => srv.kill());
for (let i = 0; i < 60; i++) { try { if ((await fetch(BASE + "/api/health")).ok) break; } catch (_) { /* not yet */ } await sleep(250); }
const browser = await chromium.launch({ channel: "chrome" });
const ctx = await browser.newContext({ viewport: { width: 1440, height: 900 } });
const page = await ctx.newPage();
page.on("pageerror", (e) => console.log("  pageerror: " + e.message));
page.on("console", (m) => { if (m.type() === "error") console.log("  console: " + m.text()); });
await page.goto(BASE + "/yours/#register");
await page.waitForSelector("#register:not([hidden])");
await page.fill('#register [name="display_name"]', "Styves");
await page.fill('#register [name="email"]', "styves@river.test");
await page.fill('#register [name="password"]', "drone in the fog");
await page.click('#register [type="submit"]');
await page.waitForSelector("#mine:not([hidden])");
let tok = null;
for (let i = 0; i < 40 && !tok; i++) { const m = /#verify=([\w-]+)/.exec(log.slice(log.lastIndexOf("the email to styves@river.test"))); tok = m && m[1]; await sleep(150); }
await page.evaluate((t) => RiverAPI.post("/api/auth/verify", { token: t }), tok);
const SHARDS = [
  ["AURORA", "AURORA", ""],
  ["LUMIERE NOCTURNE", "LUMIERE NOCTURNE", ""],
  ["GLACIER", "GLACIER", "A sequence in seven over a slow D major: the ice knocks, the river carries it."],
  ["NORTH RIVER", "NORTH RIVER", ""],
];
await page.evaluate(async (list) => {
  const img = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer());
  const ex = FB.read(PB.parse(img)).scenes.filter((s) => s.bytes && s.state !== "empty");
  for (const [name, scene, description] of list) {
    const sc = ex.find((s) => s.name === scene && s.state === "saved") || ex.find((s) => s.name === scene);
    await RiverAPI.post("/api/shards", { name, description, layout: FB.S.magic.shard, firmware: FRASIL_VERSION, data: RiverAPI.b64(sc.bytes), from_slot: sc.slot });
  }
}, SHARDS);

// ---------------------------------------------------------------- a card, opened, with the prototype
const CSS = fs.readFileSync(path.join(DIR, "proto/live.css"), "utf8"), JS = fs.readFileSync(path.join(DIR, "proto/live.js"), "utf8");
async function card(name, { width = 1440, sections = [] } = {}) {
  await page.setViewportSize({ width, height: 900 });
  await page.goto("about:blank");
  await page.goto(BASE + "/yours/");
  await page.waitForSelector("#list .sr-item");
  await page.addStyleTag({ content: CSS });
  await page.addScriptTag({ content: JS });
  await page.evaluate(async ([name, sections]) => {
    const art = [...document.querySelectorAll("#list .sr-item")].find((a) => a.querySelector(".nm-n").textContent === name);
    RiverCards.openRow(art, true, true);
    art.querySelectorAll("details.sec").forEach((d) => { if (sections.includes(d.querySelector(".sec-name").textContent)) d.open = true; });
    const img = new Uint8Array(await (await fetch("/example.frasil")).arrayBuffer());
    const sc = FB.read(PB.parse(img)).scenes.find((s) => s.name === name && s.bytes);
    Live.attach(art, sc);
    art.id = "live-card";
  }, [name, sections]);
  await page.evaluate(() => document.fonts.ready);
  await page.waitForTimeout(900);
}
/* frames 0 .. upto of a shard's capture, fed as they came (30 a second) */
const feed = (name, upto, from = 0) => page.evaluate(([fr, from]) => { fr.forEach((v, i) => Live.feed(v, (from + i) / 30)); }, [LIVE[name].slice(from, upto + 1), from]);
async function cardShot(file, pad = 16) {
  await page.mouse.move(2, 2);
  await page.waitForTimeout(500);
  const box = await page.$eval("#live-card", (e) => { const r = e.getBoundingClientRect(); return { x: r.left + scrollX, y: r.top + scrollY, w: r.width, h: r.height }; });
  const head = await page.$eval("#pt", (e) => { const r = e.getBoundingClientRect(); return r.top + scrollY; }).catch(() => box.y - 140);
  const vw = await page.evaluate(() => innerWidth);
  const y0 = Math.max(0, Math.min(head - 30, box.y - pad));
  await page.screenshot({ path: path.join(PNG, file), fullPage: true, clip: { x: 0, y: y0, width: vw, height: box.y + box.h + pad - y0 } });
  console.log("  " + file);
}
async function film(name, file, { from, secs, clip }) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "river-live-film-"));
  await page.mouse.move(2, 2);
  const box = await page.$eval(clip, (e) => { const r = e.getBoundingClientRect(); return { x: r.left + scrollX, y: r.top + scrollY, w: r.width, h: r.height }; });
  await feed(name, from - 1);
  const c = { x: Math.max(0, Math.floor(box.x) - 1), y: Math.floor(box.y) - 1, width: Math.ceil(box.w) + 2, height: Math.ceil(box.h) + 2 };
  c.width -= c.width % 2; c.height -= c.height % 2;
  for (let i = 0; i < secs * 30; i++) {
    await feed(name, from + i, from + i);
    await page.screenshot({ path: path.join(dir, `f${String(i).padStart(4, "0")}.png`), fullPage: true, clip: c });
  }
  execFileSync("ffmpeg", ["-y", "-loglevel", "error", "-framerate", "30", "-i", path.join(dir, "f%04d.png"), "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "18", path.join(PNG, file + ".mp4")]);
  execFileSync("ffmpeg", ["-y", "-loglevel", "error", "-i", path.join(PNG, file + ".mp4"), "-vf", "fps=20,scale=iw:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer:bayer_scale=3", path.join(PNG, file + ".gif")]);
  fs.rmSync(dir, { recursive: true, force: true });
  console.log("  " + file + ".mp4, .gif");
}

// ---------------------------------------------------------------- the shots
if (want("01")) { await card("NORTH RIVER", { sections: ["BASS"] }); await feed("NORTH RIVER", 290); await cardShot("01-north-river-live-1440.png"); }
if (want("02")) { await card("NORTH RIVER", { sections: ["BASS"] }); await page.evaluate(() => Live.rest()); await cardShot("02-north-river-rest-1440.png"); }
if (want("03")) { await card("GLACIER"); await feed("GLACIER", 200); await cardShot("03-glacier-live-1440.png"); }
if (want("04")) { await card("AURORA", { sections: ["BASS"] }); await feed("AURORA", 260); await cardShot("04-aurora-live-1440.png"); }
if (want("05")) { await card("LUMIERE NOCTURNE"); await feed("LUMIERE NOCTURNE", 330); await cardShot("05-lumiere-live-1440.png"); }
if (want("06")) { await card("NORTH RIVER", { width: 1100 }); await feed("NORTH RIVER", 290); await cardShot("06-north-river-live-1100.png"); }
if (want("07")) {
  await page.setViewportSize({ width: 390, height: 844 });
  await card("NORTH RIVER", { width: 390 }); await feed("NORTH RIVER", 290); await cardShot("07-north-river-live-390.png", 0);
}
if (want("10")) { await card("NORTH RIVER", { sections: ["BASS"] }); await film("NORTH RIVER", "10-north-river-film", { from: 120, secs: 10, clip: "#live-card .oc-body" }); }
if (want("11")) { await card("GLACIER"); await film("GLACIER", "11-glacier-film", { from: 60, secs: 8, clip: "#live-card .oc-body" }); }
await browser.close();
srv.kill();
process.exit(0);
