// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// Perform's live page, checked in Chrome: DEMO plays (the engine's frames come back, the river has voices), a knob
// turned from the keys reaches the engine, the next scene loads; PNGs of each moment in png/live-*.png.
//   node design/perform/scripts/live_check.mjs     (the repo served at 127.0.0.1:8093)
import path from "path";
import { fileURLToPath } from "url";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");
const PNG = path.join(path.dirname(fileURLToPath(import.meta.url)), "../png");
const b = await chromium.launch({ channel: "chrome", args: ["--autoplay-policy=no-user-gesture-required"] });
const p = await b.newPage({ viewport: { width: 1920, height: 1080 } });
const errs = [];
p.on("pageerror", (e) => errs.push(String(e)));
p.on("console", (m) => m.type() === "error" && !/404/.test(m.text()) && errs.push(m.text()));
const ok = (c, what) => { console.log((c ? "ok   " : "FAIL ") + what); if (!c) process.exitCode = 1; };
await p.goto("http://127.0.0.1:8093/design/perform/live/");
await p.waitForTimeout(800);
await p.screenshot({ path: `${PNG}/live-1-connect.png` });
await p.click("#demo-btn");
await p.waitForTimeout(3500);
const st = await p.evaluate(() => ({ state: RiverPlayer.state, live: RiverPlayer.live() && Array.from(RiverPlayer.live().slice(0, 16)), scene: PerformShow.S.scene }));
ok(st.state === "playing", `the engine plays (${st.state})`);
ok(st.live && st.live.slice(0, 8).some((n) => n >= 0), `voices sound: ${st.live && st.live.slice(0, 8).join(",")}`);
ok(st.scene === "NORTH RIVER", `scene ${st.scene}`);
await p.screenshot({ path: `${PNG}/live-2-demo.png` });
await p.keyboard.press("Tab");
await p.keyboard.press("Tab");
await p.keyboard.press("Tab");          // CLOUD
await p.keyboard.press("2");
const before = await p.evaluate(() => PerformShow.S.v[21]);
for (let i = 0; i < 8; i++) await p.keyboard.press("Shift+ArrowUp");
await p.waitForTimeout(400);
const after = await p.evaluate(() => PerformShow.S.v[21]);
ok(after > before, `CLOUD DECAY turned ${before} -> ${after}`);
ok(await p.evaluate(() => !PerformShow.S.auto), "AUTO off once the keys took the hand");
await p.screenshot({ path: `${PNG}/live-3-knob.png` });
await p.keyboard.press("b");
await p.keyboard.press("ArrowUp");
await p.waitForTimeout(300);
await p.screenshot({ path: `${PNG}/live-4-knob-b.png` });
await p.keyboard.press("ArrowRight");
await p.waitForTimeout(3000);
ok(await p.evaluate(() => PerformShow.S.scene === "LUMIERE NOCTURNE" && RiverPlayer.state === "playing"), "the next scene plays");
await p.keyboard.press("k");
await p.waitForTimeout(500);
await p.screenshot({ path: `${PNG}/live-5-keys.png` });
await p.keyboard.press(" ");
ok(await p.evaluate(() => PerformShow.S.auto), "SPACE: AUTO again");
await p.waitForTimeout(9000);
ok(await p.evaluate(() => PerformShow.S.changed), "AUTO turned something");
await p.screenshot({ path: `${PNG}/live-6-auto.png` });
ok(!errs.length, "no errors" + (errs.length ? ":\n" + errs.join("\n") : ""));
await b.close();
