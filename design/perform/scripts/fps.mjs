// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// Perform's mockups: frames drawn per second (and the slowest), the film playing, at a Retina laptop's full screen.
//   node design/perform/scripts/fps.mjs [scale]
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");
const b = await chromium.launch({ channel: "chrome", args: ["--enable-gpu", "--use-angle=metal", "--ignore-gpu-blocklist"] });
const p = await b.newPage({ viewport: { width: 1512, height: 982 }, deviceScaleFactor: +(process.argv[2] || 2) });
const errs = [];
p.on("pageerror", (e) => errs.push(String(e)));
await p.goto("http://127.0.0.1:8093/design/perform/?bare#film-a");
await p.waitForTimeout(1500);
const r = await p.evaluate(() => new Promise((res) => {
  const ts = []; const t0 = performance.now();
  const f = (t) => { ts.push(t); if (t - t0 < 6000) requestAnimationFrame(f); else res(ts); };
  requestAnimationFrame(f);
}));
const d = r.slice(1).map((t, i) => t - r[i]).sort((a, b) => a - b);
console.log(`fps ${(r.length / ((r.at(-1) - r[0]) / 1000)).toFixed(1)}  median ${d[d.length >> 1].toFixed(1)} ms  p95 ${d[Math.floor(d.length * 0.95)].toFixed(1)} ms  canvas`, await p.evaluate(() => document.querySelector("#river").width));
if (errs.length) console.log(errs.join("\n"));
await b.close();
