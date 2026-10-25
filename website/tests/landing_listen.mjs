// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The landing's factory scenes (js/listen.js) in Chrome: nothing plays on arrival, nor on a click elsewhere, even
// where the browser lets sound start by itself; in a browser holding sound back (simulated: headless Chrome never does),
// a row's button plays it, and it goes on (its click doesn't stop it); another one replaces it, alone; the one playing
// tapped again stops; each tune in an engine of its own (the one before ended); a phone: no horizontal scroll. The
// assembled site served at SITE_URL (default http://localhost:8047/: tools/website.sh --serve), then
//   node website/tests/landing_listen.mjs [OUT_DIR]        (OUT_DIR: the screenshots; default a temporary folder)
import fs from "fs";
import os from "os";
import path from "path";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");
const OUT = process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), "listen-"));
const SITE = process.env.SITE_URL || "http://localhost:8047/";
let fails = 0;
const check = (ok, what) => { console.log((ok ? "  ok " : "FAIL ") + what); if (!ok) fails++; };
const states = (p) => p.$$eval(".tune", (r) => r.map((x) => x.dataset.state || "-").join(","));
async function run(policy, init) {
  const b = await chromium.launch({ channel: "chrome", args: policy ? ["--autoplay-policy=" + policy] : [] });
  const p = await b.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  if (init) await init(p);
  const errs = [];
  p.on("console", (m) => { if (m.type() === "error" && !/status of 404/.test(m.text())) errs.push(m.text()); });
  p.on("pageerror", (e) => errs.push(String(e)));
  p.on("response", (r) => { if (r.status() >= 400 && !/stats\/downloads/.test(r.url())) errs.push(r.status() + " " + r.url()); });
  if (policy === "user-gesture-required") await p.addInitScript(() => {
    // a browser holding sound back (headless Chrome never does): a context starts suspended, resume() waits for a person
    let person = false;
    const waiting = [];
    for (const t of ["mousedown", "keydown", "touchend", "pointerup"]) addEventListener(t, () => { person = true; waiting.splice(0).forEach((f) => f()); }, true);
    const AC = window.AudioContext;
    window.AudioContext = class extends AC { constructor(o) { super(o); super.suspend(); } resume() { return person ? super.resume() : new Promise((res) => waiting.push(() => super.resume().then(res))); } };
  });
  await p.goto(SITE);
  await p.waitForTimeout(400);
  await p.$eval(".listen", (e) => e.scrollIntoView({ block: "center" }));
  await p.waitForTimeout(1500);
  return { b, p, errs };
}
{ // a browser that holds sound back (the default for a first visit): nothing by itself; a row's button plays it
  const { b, p, errs } = await run("user-gesture-required");
  await p.waitForTimeout(2500);
  check(/^-(,-)+$/.test(await states(p)) && await p.evaluate(() => window.RiverPlayer.state) === "idle", "on arrival: nothing plays (" + await states(p) + ")");
  await p.mouse.click(200, 860);                       // a click anywhere (not a row)
  await p.waitForTimeout(1500);
  check(await p.evaluate(() => window.RiverPlayer.state) === "idle", "a click elsewhere: nothing plays");
  await p.click(".tune:nth-child(1) .tune-b");
  await p.waitForFunction(() => document.querySelector(".tune").dataset.state === "playing", null, { timeout: 30000 });
  await p.waitForTimeout(2500);
  check((await states(p)).startsWith("playing") && await p.evaluate(() => window.RiverPlayer.state) === "playing",
        "the first one's button: it plays, and goes on (" + await states(p) + ")");
  check(await p.evaluate(() => window.RiverPlayer.level()) > 1e-4, "it sounds (level " + (await p.evaluate(() => window.RiverPlayer.level())).toExponential(2) + ")");
  await (await p.$(".listen")).screenshot({ path: `${OUT}/listen-playing.png` });
  const heard = [];
  await p.exposeFunction("__heard", (id) => heard.push(id));
  await p.evaluate(() => window.RiverPlayer.addEventListener("playing", (e) => window.__heard(e.detail.id)));
  await p.click(".tune:nth-child(4) .tune-b");
  await p.waitForFunction(() => document.querySelectorAll(".tune")[3].dataset.state === "playing", null, { timeout: 10000 });
  await p.waitForTimeout(1500);
  check(/^-,-,-,playing(,-)+$/.test(await states(p)) && heard.join() === "3", "another one: it replaces the first, alone (" + await states(p) + ")");
  await (await p.$(".listen")).screenshot({ path: `${OUT}/listen-open-water.png` });
  await p.click(".tune:nth-child(4) .tune-b");
  await p.waitForTimeout(300);
  check(/^-(,-)+$/.test(await states(p)) && await p.evaluate(() => window.RiverPlayer.state) === "idle", "tapped again: it stops");
  await p.mouse.click(200, 860);
  await p.waitForTimeout(800);
  check(await p.evaluate(() => window.RiverPlayer.state) === "idle", "stopped: a click elsewhere starts nothing");
  check(!errs.length, "no console errors " + errs.join(" | "));
  await b.close();
}
{ // each tune in an engine of its own: switching makes a fresh one and ends the one before (nothing blends)
  const { b, p, errs } = await run("no-user-gesture-required", (page) => page.addInitScript(() => {
    const W = window.AudioWorkletNode;
    window.__engines = [];
    window.AudioWorkletNode = class extends W {
      constructor(...a) {
        super(...a);
        const e = { ops: [] };
        window.__engines.push(e);
        const post = this.port.postMessage.bind(this.port);
        this.port.postMessage = (m, t) => { if (m && m.type === "app") e.ops.push(m.op); return post(m, t); };
      }
    };
  }));
  await p.click(".tune:nth-child(1) .tune-b");
  await p.waitForFunction(() => document.querySelector(".tune").dataset.state === "playing", null, { timeout: 30000 });
  await p.click(".tune:nth-child(4) .tune-b");
  await p.waitForFunction(() => document.querySelectorAll(".tune")[3].dataset.state === "playing", null, { timeout: 30000 });
  await p.click(".tune:nth-child(2) .tune-b");
  await p.waitForFunction(() => document.querySelectorAll(".tune")[1].dataset.state === "playing", null, { timeout: 30000 });
  await p.waitForTimeout(600);
  const en = await p.evaluate(() => window.__engines.map((e) => e.ops.join("+")));
  check(en.length === 3 && en[0].endsWith("end") && en[1].endsWith("end") && en[2] === "quiet+scene",
        "each tune its own fresh engine, the ones before ended: " + en.join(" | "));
  check(await p.evaluate(() => window.RiverPlayer.level()) > 1e-4, "the new one sounds");
  check(!errs.length, "no console errors " + errs.join(" | "));
  await b.close();
}
{ // a browser that would let it play by itself (a site often visited, or the setting): still nothing until asked
  const { b, p, errs } = await run("no-user-gesture-required");
  await p.waitForTimeout(3000);
  check(await p.evaluate(() => window.RiverPlayer.state) === "idle" && /^-(,-)+$/.test(await states(p)), "sound allowed: nothing plays by itself");
  check(!errs.length, "no console errors " + errs.join(" | "));
  await b.close();
}
{ // a phone
  const b = await chromium.launch({ channel: "chrome" });
  const p = await b.newPage({ viewport: { width: 390, height: 844 }, deviceScaleFactor: 3, isMobile: true, hasTouch: true });
  await p.goto(SITE);
  await p.$eval(".listen", (e) => e.scrollIntoView({ block: "center" }));
  await p.waitForTimeout(2200);
  const sw = await p.evaluate(() => document.documentElement.scrollWidth);
  check(sw <= 390, "phone: no horizontal scroll (" + sw + ")");
  await p.screenshot({ path: `${OUT}/listen-phone.png` });
  await b.close();
}
console.log(fails ? `listen: ${fails} FAILED` : "listen: all ok");
process.exit(fails ? 1 : 0);
