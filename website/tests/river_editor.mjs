// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The River's editor (river-edit.js), in Chrome, with the service: Mira shares KATABATIC; Styves keeps ABYSS, GLACIER
// and NORTH RIVER. Checked: an opened card's settings are sliders; a drag turns one (its value, lamp, where it was),
// the line EDITED, the head's actions the editor's (Undo, Revert, Save, Save as); the keys step one (one step back
// for a row of them); a choice's list (SCALE); a drone's setting turned on a preset makes it CUSTOM (the engine's own
// rule); the sound follows (▶, then LFO 1 woken: the engine playing has it on); the draft survives a reload (EDITED ·
// NOT SAVED under its name, closed); Undo, Revert (asked); SAVE AS (a new shard, opened; ABYSS as it was); SAVE (one's
// own: its sound replaced; shared: asked first); someone else's, from the Community River: credited (CC BY 4.0);
// HARMONY's settings in THE KEYS (DRONE), no more among the pages;
// signed out: the sheet says sign in, the draft kept in the tab; Rename (was Edit); a phone: no horizontal scroll; no
// console errors.
//   node website/tests/river_editor.mjs [OUT_DIR]
// It starts the service itself (server/.venv's uvicorn, a fresh database, port RIVER_TEST_PORT or 8056, the River as
// tools/website.sh built it). PLAYWRIGHT: Playwright's index.mjs (default: the global one).
import fs from "fs";
import os from "os";
import path from "path";
import { spawn } from "child_process";
import { fileURLToPath } from "url";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");

const REPO = path.join(path.dirname(fileURLToPath(import.meta.url)), "../..");
const OUT = path.resolve(process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), "river-editor-")));
fs.mkdirSync(OUT, { recursive: true });
let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "  ok" : "FAIL"} ${msg}`); if (!ok) fails++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const port = process.env.RIVER_TEST_PORT || "8056", BASE = `http://localhost:${port}`;
try { await fetch(BASE + "/api/health"); console.log(`FAIL port ${port} is taken (RIVER_TEST_PORT)`); process.exit(1); } catch (_) { /* free */ }
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
  for (const [name, scene] of list) {
    const sc = ex.find((s) => s.name === scene && s.state === "saved") || ex.find((s) => s.name === scene);
    out.push(await RiverAPI.post("/api/shards", { name, layout: FB.S.magic.shard, firmware: FRASIL_VERSION, data: RiverAPI.b64(sc.bytes), from_slot: sc.slot }));
  }
  return out;
}, list);
const api = (page, m, p, b) => page.evaluate(([m, p, b]) => RiverAPI[m](p, b), [m, p, b]);
const P = { DEPTH: 96, TARGET1: 98, DECAY: 21, SCALE: 34, TIMBRE: 31 };
const V0 = JSON.parse(fs.readFileSync(path.join(REPO, "server/schema.json"), "utf8")).scene_t.v - 16;   // (the sound, after the name)

// ---------------------------------------------------------------- Mira shares KATABATIC; Styves keeps three
const { page: mira } = await person("Mira K.", "mira@river.test");
const [kat] = await add(mira, [["KATABATIC", "LUMIERE NOCTURNE"]]);
await api(mira, "post", `/api/shards/${kat.id}/share`);
const { page } = await person("Styves", "styves@river.test");
const [abyss, glacier, north] = await add(page, [["ABYSS", "ABYSS"], ["GLACIER", "GLACIER"], ["NORTH RIVER", "NORTH RIVER"]]);
await page.goto(BASE + "/yours/");
await page.waitForSelector(`#m-${abyss.id}`);
const A = `#m-${abyss.id}`;
check((await page.textContent(`${A} [data-act="edit"]`)).trim() === "Rename", "one's own shard: Rename (was Edit)");
await page.click(`${A} .sr-open`);
await page.waitForSelector(`${A}.open .oc-body`);
const sec = (art, name) => page.evaluate(([art, name]) => { const d = [...document.querySelectorAll(`${art} details.sec`)].find((x) => x.querySelector(".sec-name").firstChild.textContent === name); d.open = true; }, [art, name]);
await sec(A, "LFO 1");
await sec(A, "FX");
check(!(await page.$(`${A} details.sec .sec-name:text-is("HARMONY")`)), "HARMONY: not among the pages any more (THE KEYS have it)");
await page.click(`${A} .oc-keys [data-k="KEYS:drone"]`);            // (THE KEYS on DRONE: HARMONY's settings under them)
await page.waitForTimeout(200);
check(await page.$$eval(`${A} .oc-body .cell[role="slider"]`, (e) => e.length) >= 20, "an opened card: its settings are sliders");
const cellOf = (art, p) => `${art} .oc-body .cell[data-p="${p}"]`;
const valueText = (art, p) => page.$eval(cellOf(art, p), (c) => c.querySelector(".cell-v").textContent.replace(/\s+/g, " ").trim());

// ---------------------------------------------------------------- a drag: LFO 1's DEPTH
async function drag(art, p, dx) {
  const b = await page.$eval(cellOf(art, p), (c) => { c.scrollIntoView({ block: "center" }); const r = c.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
  await page.mouse.move(b.x, b.y);
  await page.mouse.down();
  for (let i = 1; i <= 10; i++) await page.mouse.move(b.x + dx * i / 10, b.y + 1);
  await page.mouse.up();
  await page.waitForTimeout(150);
}
await page.waitForFunction(() => window.RiverEdit && true);
await page.waitForTimeout(800);                                  // (the model: app.wasm on the page)
const depth0 = await valueText(A, P.DEPTH);
await drag(A, P.DEPTH, 90);
const depth1 = await valueText(A, P.DEPTH);
check(depth0 !== depth1 && await page.$eval(cellOf(A, P.DEPTH), (c) => c.classList.contains("is-edited") && /^Was /.test(c.title) && !!c.querySelector(".gauge .was")),
      `a drag turns DEPTH (${depth0} → ${depth1}): lamp, where it was ticked`);
check(/EDITED/.test(await page.textContent(`${A} .ed-band`)) && /One setting changed/.test(await page.textContent(`${A} .ed-band`)), "the line: EDITED, one setting changed");
const heads = await page.$$eval(`${A} .acts button`, (b) => b.map((x) => x.textContent.trim()));
check(heads.join("|") === "Undo|Revert|Save|Save as…", "the head's actions, the editor's: " + heads.join(" | "));
check(await page.$eval(`${A} details.sec`, () => !!document.querySelector(`.sec-name .ed-dot`)), "its section marked (a dot)");

// ---------------------------------------------------------------- the keys: DECAY stepped
const decay0 = +(await page.getAttribute(cellOf(A, P.DECAY), "aria-valuenow"));
await page.focus(cellOf(A, P.DECAY));
for (let i = 0; i < 3; i++) await page.keyboard.press("ArrowRight");
await page.waitForTimeout(900);
const decay1 = +(await page.getAttribute(cellOf(A, P.DECAY), "aria-valuenow"));
check(decay1 === decay0 + 3 && /settings changed/.test(await page.textContent(`${A} .ed-band`)), `the arrow keys step DECAY (${decay0} → ${decay1})`);
check(await page.evaluate((a) => document.activeElement && document.activeElement.matches(`${a} .cell[data-p="21"]`), A), "the focus stays on the setting");
await page.keyboard.press("ArrowRight");
await page.keyboard.press("ArrowRight");
await page.click(`${A} [data-act="undo"]`);                       // (within the keys' 700 ms: their gesture ends first)
await page.waitForTimeout(900);
check(+(await page.getAttribute(cellOf(A, P.DECAY), "aria-valuenow")) === decay1, "Undo right after keys: those keys undone, as one step");

// ---------------------------------------------------------------- a choice's list: SCALE (the one it has: no change)
await page.click(cellOf(A, P.SCALE));
await page.waitForSelector(".ed-pop[role=listbox]");
const opts = await page.$$eval(".ed-pop button", (b) => b.map((x) => x.textContent.trim()));
check(opts.length >= 10 && opts.includes("MAJOR"), `SCALE's list: ${opts.length} scales`);
await page.keyboard.press("Escape");
check(!(await page.$(".ed-pop")), "Escape closes it");
await page.click(cellOf(A, P.SCALE));
await page.click(".ed-pop button:has-text('MAJOR')");
await page.waitForTimeout(200);
check(/MAJOR/.test(await valueText(A, P.SCALE)) && /D MAJOR/.test(await page.textContent(`${A} .kv`)), "chosen: MAJOR, its row's key too");
const n0 = await page.textContent(`${A} .ed-band`);
await page.click(cellOf(A, P.SCALE));
await page.click(".ed-pop button.is-on");
await page.waitForTimeout(200);
check(await page.textContent(`${A} .ed-band`) === n0, "the one it has, chosen again: nothing more changed");

// ---------------------------------------------------------------- undo
const before = await page.getAttribute(cellOf(A, P.SCALE), "aria-valuenow");
await page.click(`${A} [data-act="undo"]`);
await page.waitForTimeout(200);
check(await page.getAttribute(cellOf(A, P.SCALE), "aria-valuenow") !== before && !(await page.$eval(cellOf(A, P.SCALE), (c) => c.classList.contains("is-edited"))), "Undo: SCALE back");

// ---------------------------------------------------------------- the sound follows: ▶, then LFO 1 woken on its target
await page.click(`${A} [data-act="undo"]`);                        // (DECAY back)
await page.click(`${A} [data-act="undo"]`);                        // (DEPTH back: nothing changed now)
await page.waitForTimeout(200);
check(!(await page.$(`${A} .ed-band`)) && !(await page.$(`${A} [data-act="undo"]`)), "undone to the shard as it was: not edited, the page's actions back");
await page.click(`${A} .play`);
await page.waitForFunction((a) => document.querySelector(a).dataset.sound === "playing", A, { timeout: 30000 });
await page.waitForTimeout(800);
check(await page.evaluate(() => { const v = RiverPlayer.live(); return !!v && v[26] === 0; }), "playing ABYSS: LFO 1 off");
await page.click(cellOf(A, P.SCALE));
await page.waitForSelector(".ed-pop");
await page.keyboard.press("Escape");
await page.waitForTimeout(200);
check(!(await page.$(".ed-pop")) && await page.evaluate(() => RiverPlayer.state) === "playing", "Escape in a list closes it: the sound plays on");
await sec(A, "LFO 1");
await page.click(cellOf(A, P.TARGET1));
await page.click(".ed-pop button:has-text('CHOIR')");
await drag(A, P.DEPTH, 90);
await page.waitForTimeout(900);
check(await page.evaluate(() => { const v = RiverPlayer.live(); return !!v && v[26] === 1; }), "LFO 1 given a TARGET and a DEPTH: the engine playing has it on");
check(await page.$$eval(`${A} .oc-lfo .lf:not(.is-off)`, (e) => e.length) === 1, "...its LFO column too");
{                                                                  // a release redraws the card: nothing flickers
  const b = await page.$eval(cellOf(A, P.DECAY), (c) => { c.scrollIntoView({ block: "center" }); const r = c.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
  await page.mouse.move(b.x, b.y); await page.mouse.down();
  for (let i = 1; i <= 8; i++) await page.mouse.move(b.x + 6 * i, b.y + 1);
  await page.evaluate((a) => {
    window.__fr = [];
    const art = document.querySelector(a), t0 = performance.now();
    const f = () => {
      const kb = art.querySelector(".oc-keys .k-kb");
      window.__fr.push({ lit: kb ? [...kb.querySelectorAll("rect.k-on")].filter((r) => +r.getAttribute("opacity") > 0.05).length : 0,
                         lfo: [...art.querySelectorAll(".lf-sc circle")].filter((c) => +c.getAttribute("opacity") > 0.5).length });
      if (performance.now() - t0 < 600) requestAnimationFrame(f);
    };
    requestAnimationFrame(f);
  }, A);
  await page.waitForTimeout(120);
  await page.mouse.up();
  await page.waitForTimeout(700);
  const fr = await page.evaluate(() => window.__fr);
  check(fr.length > 20 && fr.every((x) => x.lit > 0 && x.lfo > 0),
        `a setting released while it plays: every frame lit as before, THE KEYS' lights and the LFO's glint there (${fr.length} frames)`);
}

// ---------------------------------------------------------------- a drone's setting on a preset: CUSTOM (GLACIER)
const G = `#m-${glacier.id}`;
await page.click(`${G} .sr-open`);
await page.waitForSelector(`${G}.open .oc-body`);
await sec(G, "DRONE");
await page.waitForTimeout(200);
check(!/CUSTOM/.test(await page.textContent(`${G} details.sec .sec-mid`)), "GLACIER's drone: a preset");
await drag(G, P.TIMBRE, 60);
check(/CUSTOM/.test(await page.textContent(`${G} details.sec .sec-mid`)), "its TIMBRE turned: CUSTOM, as on the FM-1");
await sec(G, "GLINT");
await page.click(`${G} .oc-keys [data-k="KEYS:glint"]`);           // (THE KEYS on GLINT: GLINT SEQ's settings under them)
await page.waitForTimeout(200);
const GATE = await page.evaluate(() => FB.S.ids.P_SQ_GATE);
const gate = await page.$$(`${G} .oc-body .cell[data-p="${GATE}"]`);
const gb = await gate[1].evaluate((c) => { c.scrollIntoView({ block: "center" }); const r = c.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
await page.mouse.move(gb.x, gb.y); await page.mouse.down();
for (let i = 1; i <= 8; i++) await page.mouse.move(gb.x + i * 6, gb.y + 1);
const mid = await page.$$eval(`${G} .oc-body .cell[data-p="${GATE}"]`, (c) => c.map((x) => x.querySelector(".cell-v").textContent));
await page.mouse.up();
await page.waitForTimeout(200);
check(gate.length === 2 && mid[0] === mid[1], `GATE shown twice (its SEQ page, THE KEYS): both turn together (${mid.join(" · ")})`);

// ---------------------------------------------------------------- the draft survives a reload
await page.reload();
await page.waitForSelector(A);
check(/EDITED · NOT SAVED/.test(await page.textContent(`${A} .nm-t`)) && /EDITED · NOT SAVED/.test(await page.textContent(`${G} .nm-t`)), "after a reload: ABYSS and GLACIER, EDITED · NOT SAVED");
await page.click(`${G} .sr-open`);
await page.waitForSelector(`${G}.open .ed-band`);
await page.click(`${G} [data-act="revert"]`);
await page.waitForSelector("dialog[open]");
check(/Revert GLACIER\?/.test(await page.textContent("dialog[open]")), "Revert asks first");
await page.click("dialog[open] .btn-ice");
await page.waitForTimeout(300);
check(!(await page.$(`${G} .ed-band`)) && !/EDITED/.test(await page.textContent(`${G} .nm-t`)), "reverted: GLACIER as it was");

// ---------------------------------------------------------------- SAVE AS: a new shard, opened; ABYSS as it was
await page.click(`${A} .sr-open`);
await page.waitForSelector(`${A}.open [data-act="save-as"]`);
await page.click(`${A} [data-act="save-as"]`);
await page.waitForSelector("dialog.ed-sheet[open]");
check(await page.inputValue("dialog.ed-sheet .name-in") === "ABYSS II", "SAVE AS: the name ABYSS II to start");
await page.fill("dialog.ed-sheet .name-in", "bad/name");
await page.click("dialog.ed-sheet .btn-ice");
check(/A name: 1 to 16/.test(await page.textContent("dialog.ed-sheet .form-say")), "a name the FM-1 can't show: said");
await page.fill("dialog.ed-sheet .name-in", "abyss ii");
await page.click("dialog.ed-sheet .btn-ice");
await page.waitForFunction(() => !document.querySelector("dialog.ed-sheet[open]"));
await page.waitForTimeout(400);
const mine = (await api(page, "get", "/api/shards")).items;
const ii = mine.find((x) => x.name === "ABYSS II"), ab = mine.find((x) => x.id === abyss.id);
const dec = (t) => Uint8Array.from(atob(t), (c) => c.charCodeAt(0)).subarray(16);
const diff = [...dec(ii.data)].map((v, i) => (v !== dec(abyss.data)[i] ? i : -1)).filter((i) => i >= 0);
check(!!ii && ab.data === abyss.data && ii.origin_author == null && diff.join() === [V0 + P.DECAY, V0 + P.DEPTH, V0 + P.TARGET1].join(),
      `ABYSS II saved: only its three changes differ from ABYSS (bytes ${diff.join(", ")}: DECAY, DEPTH, TARGET 1; its drone as kept), ABYSS unchanged`);
check(await page.$(`#m-${ii.id}.open`) && !/EDITED/.test(await page.textContent(`${A} .nm-t`)), "ABYSS II opened in the list; ABYSS not edited any more");

// ---------------------------------------------------------------- SAVE: one's own shard's sound replaced; shared, asked first
const N = `#m-${north.id}`;
await api(page, "post", `/api/shards/${north.id}/share`);
await page.reload();
await page.waitForSelector(N);
await page.click(`${N} .sr-open`);
await page.waitForSelector(`${N}.open .oc-body`);
await sec(N, "FX");
await page.waitForTimeout(200);
await drag(N, P.DECAY, -60);
await page.click(`${N} [data-act="save"]`);
await page.waitForSelector("dialog[open]");
check(/Replace NORTH RIVER\?/.test(await page.textContent("dialog[open]")), "SAVE on a shared shard: asked first");
await page.click("dialog[open] .btn-ice");
await page.waitForTimeout(600);
const n2 = (await api(page, "get", "/api/shards")).items.find((x) => x.id === north.id);
check(n2.data !== north.data && n2.name === "NORTH RIVER", "SAVE: its sound replaced, its name kept");
check(!/EDITED/.test(await page.textContent(`${N} .nm-t`)) && !(await page.$(`${N} .ed-band`)), "...and not edited any more");

// ---------------------------------------------------------------- someone else's, from the Community River: credited
await page.goto(BASE + "/");
const K = `#s-${kat.id}`;
await page.waitForSelector(K);
await page.click(`${K} .sr-open`);
await page.waitForSelector(`${K}.open .oc-body`);
await sec(K, "FX");
await page.waitForTimeout(800);
await drag(K, P.DECAY, 50);
await page.click(`${K} [data-act="save-as"]`);
await page.waitForSelector("dialog.ed-sheet[open]");
check(/After KATABATIC, by Mira K\./.test(await page.textContent("dialog.ed-sheet .ed-from")), "someone else's: credited in the sheet (CC BY 4.0)");
await page.click("dialog.ed-sheet .btn-ice");
await page.waitForFunction(() => !document.querySelector("dialog.ed-sheet[open]"));
const k2 = (await api(page, "get", "/api/shards")).items.find((x) => x.name === "KATABATIC II");
check(!!k2 && k2.origin_author === "Mira K." && k2.origin_id === kat.id, "KATABATIC II in Styves' River, Mira credited");

// ---------------------------------------------------------------- signed out: the sheet says sign in; the draft kept
const { page: anon } = await person("anon", null);
await anon.goto(BASE + "/");
await anon.waitForSelector(K);
await anon.click(`${K} .sr-open`);
await anon.waitForSelector(`${K}.open .oc-body`);
await anon.evaluate((k) => { const d = [...document.querySelectorAll(`${k} details.sec`)].find((x) => x.querySelector(".sec-name").firstChild.textContent === "FX"); d.open = true; }, K);
await anon.waitForTimeout(800);
const box = await anon.$eval(`${K} .oc-body .cell[data-p="21"]`, (c) => { c.scrollIntoView({ block: "center" }); const r = c.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
await anon.mouse.move(box.x, box.y); await anon.mouse.down();
for (let i = 1; i <= 8; i++) await anon.mouse.move(box.x + i * 8, box.y);
await anon.mouse.up();
await anon.click(`${K} [data-act="save-as"]`);
await anon.waitForSelector("dialog.ed-sheet[open]");
check(/Sign in/.test(await anon.textContent("dialog.ed-sheet")) && !(await anon.$("dialog.ed-sheet .name-in")), "signed out: SAVE AS says sign in");
check(await anon.evaluate((id) => !!sessionStorage.getItem("river-edit:shard:" + id), kat.id), "...the draft kept in the tab");

// ---------------------------------------------------------------- a phone
const { page: ph } = await person("phone", null, { width: 390, height: 844 }, { deviceScaleFactor: 2, isMobile: true, hasTouch: true });
await ph.goto(BASE + "/");
await ph.waitForSelector(K);
await ph.click(`${K} .sr-open`);
await ph.waitForSelector(`${K}.open .oc-body`);
const w = await ph.evaluate(() => document.documentElement.scrollWidth);
check(w <= 390, `phone: an opened card, no horizontal scroll (${w})`);
await page.screenshot({ path: path.join(OUT, "editor-1440.png") });

check(!errors.length, "no console errors" + (errors.length ? ": " + errors.slice(0, 4).join(" / ") : ""));
await browser.close();
console.log(fails ? `river_editor: ${fails} FAILED` : "river_editor: all ok");
process.exit(fails ? 1 : 0);
