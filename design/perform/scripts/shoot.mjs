// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// Perform's mockups as PNGs (1920 x 1080, the bar hidden): each frame in each direction, from the page served at the
// repo's root (python3 -m http.server 8093 --directory .).
//   node design/perform/scripts/shoot.mjs [frame-dir ...]
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");
const HERE = path.dirname(fileURLToPath(import.meta.url)), PNG = path.join(HERE, "../png");
fs.mkdirSync(PNG, { recursive: true });
const ALL = ["connect", "live", "knob", "scene", "keys", "freeze"];
const only = process.argv.slice(2);
const browser = await chromium.launch({ channel: "chrome" });
const page = await browser.newPage({ viewport: { width: 1920, height: 1080 }, deviceScaleFactor: 1 });
const errs = [];
page.on("pageerror", (e) => errs.push(String(e)));
page.on("console", (m) => m.type() === "error" && !/404/.test(m.text()) && errs.push(m.text()));
for (const f of only.length ? only : ALL) {
  const i = ALL.indexOf(f);
  await page.goto(`http://127.0.0.1:8093/design/perform/?bare#${f}`);
  await page.waitForTimeout(2600);
  await page.screenshot({ path: path.join(PNG, `${String(i + 1).padStart(2, "0")}-${f}.png`) });
  console.log("shot", f);
}
await browser.close();
if (errs.length) { console.log("errors:\n" + errs.join("\n")); process.exit(1); }
