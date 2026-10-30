// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The editor's mockups, on the River itself: the service (a fresh database, port 8059, the River as tools/website.sh
// built it), Mira (who shares KATABATIC) and Styves (ABYSS and three others), a card opened, the prototype
// (proto/editor.js, editor.css) put in, its states staged with edits the engine itself makes (web_param), and, where it
// plays, the edited sound playing for real: the PNGs in png/.
//   node design/river-editor/scripts/shoot.mjs [only ...]
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
const [kat] = await add(mira, [["KATABATIC", "LUMIERE NOCTURNE", "The wind coming down the glacier, at night."]]);
await mira.evaluate((id) => RiverAPI.post(`/api/shards/${id}/share`), kat.id);
const { page } = await person("Styves", "styves@river.test");
await add(page, [["NORTH RIVER", "NORTH RIVER", ""], ["GLACIER", "GLACIER", "A sequence in seven over a slow D major: the ice knocks, the river carries it."],
                 ["CATHEDRAL", "CATHEDRAL", ""], ["ABYSS", "ABYSS", "Two fifths in D phrygian, a bass far below. Let it rise slowly, then FREEZE the choir."]]);

// ---------------------------------------------------------------- a card, opened, edited
const CSS = fs.readFileSync(path.join(DIR, "proto/editor.css"), "utf8"), JS = fs.readFileSync(path.join(DIR, "proto/editor.js"), "utf8");
const T = (name) => `FB.S.lfo.targets.findIndex((t) => t && t.name === ${JSON.stringify(name)})`;
async function open(pg, url, list, name, edits, o = {}) {
  await pg.goto("about:blank");
  await pg.goto(BASE + url);
  await pg.waitForSelector(`${list} .sr-item`);
  await pg.addStyleTag({ content: CSS });
  await pg.addScriptTag({ content: JS });
  return pg.evaluate(async ([list, name, edits, o]) => {
    const art = [...document.querySelectorAll(`${list} .sr-item`)].find((a) => a.querySelector(".nm-n").textContent === name);
    RiverCards.openRow(art, true, true);
    art.id = "ed-card";
    const scs = await (async () => {                           // (the shard's own bytes: the service's)
      const all = [...(await RiverAPI.get("/api/shards").catch(() => ({ items: [] }))).items, ...(await RiverAPI.get("/api/public/shards").catch(() => ({ items: [] }))).items];
      return all.map((x) => RiverAPI.scene({ ...x, data: x.data }));
    })();
    const sc0 = scs.find((x) => x && x.name === name);
    const ed = (edits || []).map(([p, x]) => [typeof p === "string" ? FB.S.ids[p] : p, typeof x === "string" ? eval(x) : x]);
    const r = await Editor.stage(art, sc0, ed, o);
    if (o.play) { RiverPlayer.follow = true; art._lv = null; await RiverPlayer.play(r.bytes, art._key); }
    window.__ed = { sc0, bytes: r.bytes };
    return true;
  }, [list, name, edits, o]);
}
async function shot(pg, file, { pad = 16, full = false, from = "#pt" } = {}) {
  await pg.mouse.move(2, 2);
  await pg.waitForTimeout(o_wait);
  if (full) { await pg.screenshot({ path: path.join(PNG, file) }); console.log("  " + file); return; }
  const box = await pg.$eval("#ed-card", (e) => {
    const r = e.getBoundingClientRect(), p = document.querySelector(".ed-pop"), q = p && p.getBoundingClientRect();
    return { y: r.top + scrollY, h: Math.max(r.bottom, q ? q.bottom + 8 : 0) - r.top };
  });
  const top = await pg.$eval(from, (e) => e.getBoundingClientRect().top + scrollY).catch(() => box.y - 140);
  const vw = await pg.evaluate(() => innerWidth), y0 = Math.max(0, Math.min(top - 30, box.y - pad));
  await pg.screenshot({ path: path.join(PNG, file), fullPage: true, clip: { x: 0, y: y0, width: vw, height: box.y + box.h + pad - y0 } });
  console.log("  " + file);
}
let o_wait = 600;
/* ABYSS: LFO 1 woken (DEPTH 46, TARGET 1 on the CHOIR's level), the cloud's DECAY longer */
const ABYSS = [[96, 46], [98, T("CHOIR")], [21, 104]];
if (want("30")) { await open(page, "/yours/", "#list", "ABYSS", ABYSS, { open: ["LFO 1", "FX"], active: 96, play: true }); o_wait = 2600; await shot(page, "30-edit-1440.png"); o_wait = 600; }
if (want("31")) { await open(page, "/yours/", "#list", "ABYSS", ABYSS, { open: ["LFO 1"], pop: 99, popHover: 0 }); await shot(page, "31-target-1440.png"); }
if (want("32")) { await open(page, "/yours/", "#list", "ABYSS", ABYSS, { open: ["HARMONY"], pop: 34, popHover: 4, hover: 34 }); await shot(page, "32-choice-1440.png"); }
if (want("33")) {
  await open(page, "/yours/", "#list", "ABYSS", ABYSS, { open: ["LFO 1", "FX"] });
  await page.evaluate(() => Editor.sheet({ source: "ABYSS", name: "ABYSS II", description: "Two fifths in D phrygian, a bass far below. Let it rise slowly, then FREEZE the choir.", ownName: "ABYSS" }));
  await shot(page, "33-save-as-own-1440.png", { full: true });
}
if (want("34") || want("35")) {
  await open(page, "/", "#gal", "KATABATIC", [[95, 70], [34, 2]], { open: ["LFO 1", "HARMONY"], hover: 95 });
  if (want("34")) await shot(page, "34-community-edit-1440.png", { from: "#pt" });
  if (want("35")) {
    await page.evaluate(() => Editor.sheet({ source: "KATABATIC", name: "KATABATIC II", description: "", from: { name: "KATABATIC", author: "Mira K." } }));
    await shot(page, "35-save-as-other-1440.png", { full: true });
  }
}
if (want("36")) {
  const { page: anon } = await person("anon", null);
  await open(anon, "/", "#gal", "KATABATIC", [[95, 70], [34, 2]], { open: ["LFO 1"] });
  await anon.evaluate(() => Editor.sheet({ source: "KATABATIC", signedOut: true, name: "" }));
  await shot(anon, "36-save-as-signed-out-1440.png", { full: true });
}
if (want("37")) {                                            // Your Private River afterwards: KATABATIC II (Mira credited), ABYSS still edited
  const copy = await page.evaluate((id) => RiverAPI.post(`/api/public/shards/${id}/save`), kat.id);
  await page.evaluate((id) => RiverAPI.patch(`/api/shards/${id}`, { name: "KATABATIC II", description: "" }), copy.id);
  await open(page, "/yours/", "#list", "ABYSS", ABYSS, { open: [] });
  await page.evaluate(() => {
    const art = document.querySelector("#ed-card");
    RiverCards.openRow(art, false, true);
    const nm = art.querySelector(".nm-t");
    nm.append(RiverCards.h("span", { class: "nm-mk ed" }, "EDITED · NOT SAVED"));
    const k = [...document.querySelectorAll("#list .sr-item")].find((a) => a.querySelector(".nm-n").textContent === "KATABATIC II");
    k.id = "ed-new";
    RiverCards.toast("Saved: KATABATIC II, in Your Private River.");
  });
  o_wait = 900;
  await shot(page, "37-list-after-1440.png", { full: true });
  o_wait = 600;
}
if (want("38")) {
  const { page: ph } = await person("Styves", null, { width: 390, height: 844 }, { deviceScaleFactor: 2, isMobile: true, hasTouch: true });
  await ph.context().addCookies(await page.context().cookies());
  await open(ph, "/yours/", "#list", "ABYSS", ABYSS, { open: ["LFO 1"], active: 96 });
  await ph.evaluate(() => { const c = document.querySelector("#ed-card .cell.is-active"); scrollTo(0, c.getBoundingClientRect().top + scrollY - 420); });
  await ph.mouse.move(2, 2);
  await ph.waitForTimeout(600);
  await ph.screenshot({ path: path.join(PNG, "38-edit-390.png") });
  console.log("  38-edit-390.png");
}
await browser.close();
srv.kill();
process.exit(0);
