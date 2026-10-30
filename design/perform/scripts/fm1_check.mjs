// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// Perform's live page with the FM-1 itself (FRASIL with PERFORM, plugged in): CONNECT, the scene named, the river fed
// by the FM-1's frames, a knob turned on the FM-1 (fm1_remote.py) shown, then turned back; PNGs png/fm1-*.png.
//   node design/perform/scripts/fm1_check.mjs     (the repo served at 127.0.0.1:8093)
import path from "path";
import { execFileSync } from "child_process";
import { fileURLToPath } from "url";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");
const HERE = path.dirname(fileURLToPath(import.meta.url)), PNG = path.join(HERE, "../png"), REPO = path.join(HERE, "../../..");
const remote = (...a) => execFileSync(path.join(REPO, ".venv/bin/python"), [path.join(REPO, "base/tools/fm1_remote.py"), ...a.map(String)]);
const b = await chromium.launch({ channel: "chrome" });
const ctx = await b.newContext({ viewport: { width: 1920, height: 1080 } });
await ctx.grantPermissions(["midi", "midi-sysex"], { origin: "http://127.0.0.1:8093" });
const p = await ctx.newPage();
const errs = [];
p.on("pageerror", (e) => errs.push(String(e)));
p.on("console", (m) => m.type() === "error" && !/404/.test(m.text()) && errs.push(m.text()));
const ok = (c, what) => { console.log((c ? "ok   " : "FAIL ") + what); if (!c) process.exitCode = 1; };
await p.goto("http://127.0.0.1:8093/design/perform/live/");
await p.waitForTimeout(600);
await p.click("#connect-btn");
await p.waitForTimeout(3000);
const st = await p.evaluate(() => ({ hidden: document.querySelector("#connect").hidden, scene: PerformShow.S.scene, say: document.querySelector("#say").textContent }));
ok(st.hidden, `connected (${st.say || "no message"})`);
ok(!!st.scene, `scene: ${st.scene}`);
await p.screenshot({ path: `${PNG}/fm1-1-connected.png` });
remote("turn", "K2", 1);                 // (one detent each way: no acceleration, the scene as it was)
await p.waitForTimeout(500);
const shown = await p.evaluate(() => document.querySelector("#now").textContent.replace(/\s+/g, " ").trim());
ok(/2\s*[A-Z]/.test(shown), `K2 shown in LIVE: "${shown}"`);
const lfo = await p.evaluate(() => [...document.querySelectorAll(".lfo header span")].map((e) => e.textContent).join(" | "));
ok(lfo.length > 0, `the LFOs: ${lfo}`);
await p.screenshot({ path: `${PNG}/fm1-2-knob.png` });
remote("turn", "K2", -1);
await p.waitForTimeout(2500);
remote("tap", "FX", "wait", 400, "tap", "HOME");
await p.waitForTimeout(300);
await p.screenshot({ path: `${PNG}/fm1-3-page.png` });
ok(!errs.length, "no errors" + (errs.length ? ":\n" + errs.join("\n") : ""));
await b.close();
