// FRASIL River v2: the mockups' PNGs (design/river-v2/png/), at 1440 wide (and 390 for the key screens), then the
// contact sheet 00-overview.png. Serves the repository on a local port so the site's own CSS and fonts load.
//   node design/river-v2/scripts/render.mjs [only-this-page ...]
import fs from "fs";
import http from "http";
import path from "path";
import { fileURLToPath } from "url";
const { chromium } = await import("/opt/homebrew/lib/node_modules/playwright/index.mjs");
const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2 = path.join(HERE, ".."), REPO = path.join(V2, "../..");
const PNG = path.join(V2, "png");
fs.mkdirSync(PNG, { recursive: true });
const TYPES = { ".html": "text/html", ".css": "text/css", ".png": "image/png", ".woff2": "font/woff2", ".svg": "image/svg+xml", ".js": "text/javascript" };
const srv = http.createServer((q, r) => {
  const p = path.join(REPO, decodeURIComponent(q.url.split("?")[0]));
  if (!p.startsWith(REPO) || !fs.existsSync(p) || fs.statSync(p).isDirectory()) { r.writeHead(404); r.end(); return; }
  r.writeHead(200, { "content-type": TYPES[path.extname(p)] || "application/octet-stream" });
  fs.createReadStream(p).pipe(r);
}).listen(0);
const BASE = `http://localhost:${srv.address().port}/design/river-v2/`;

/* each page: full page or the window only (a dialog), and whether it is a key screen (390 too) */
const PLAN = {
  "01-private-2": { full: true, phone: true },
  "02-private-100": { full: true, phone: true },
  "02c-private-backups": { full: true },
  "03-titles": { full: true },
  "04-card-own": { full: true, phone: true },
  "04b-card-bass": { full: true },
  "05-card-other": { full: true, phone: true },
  "06-device-hints": { full: true },
  "07-later-live": { full: true },
  "08-later-editor": { full: true },
  "10-send-reading": {}, "11-send-not-connected": { phone: true }, "12-send-no-frasil": {}, "13-send-ready-1": { phone: true },
  "14-send-ready-3": { phone: true }, "15-send-choose": {}, "16-send-confirm": { phone: true }, "17-send-sending": {}, "18-send-press-sel": {},
  "19-send-done": {}, "19b-send-failed": {}, "19c-send-held": {},
  "20-get-ready": { phone: true }, "21-get-done": {},
  "30-community": { full: true, phone: true },
  "40-tools": { full: true },
  "41-profile": { full: true },
};
const only = process.argv.slice(2);
const browser = await chromium.launch({ channel: "chrome" });
const desk = await browser.newPage({ viewport: { width: 1440, height: 900 } });
const phone = await browser.newPage({ viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true });
for (const p of [desk, phone]) p.on("console", (m) => { if (m.type() === "error") console.log("  console: " + m.text()); });
const shots = [];
for (const [name, o] of Object.entries(PLAN)) {
  if (only.length && !only.some((x) => name.startsWith(x))) { if (fs.existsSync(path.join(PNG, `${name}-1440.png`))) shots.push([name, o]); continue; }
  for (const [pg, tag] of [[desk, "1440"], ...(o.phone ? [[phone, "390"]] : [])]) {
    await pg.goto(BASE + name + ".html");
    await pg.evaluate(() => document.fonts.ready);
    await pg.waitForTimeout(250);
    await pg.screenshot({ path: path.join(PNG, `${name}-${tag}.png`), fullPage: !!o.full && !(tag === "390" && !o.full) });
  }
  shots.push([name, o]);
  console.log("  " + name);
}

// ---------------------------------------------------------------- the contact sheet
const cards = Object.entries(PLAN).map(([n, o]) => `<figure><div class="im"><img src="${n}-1440.png"></div><figcaption>${n}${o.phone ? ' <span>+ 390</span>' : ""}</figcaption></figure>`).join("");
fs.writeFileSync(path.join(PNG, "_overview.html"), `<!doctype html><html><head><meta charset="utf-8"><link rel="stylesheet" href="../v2.css"><style>
body{background:#06080c;padding:48px 56px}h1{font:700 22px var(--f-ui);letter-spacing:.24em;color:var(--white)}h1 span{color:var(--lamp);font-weight:600;font-size:14px;margin-left:16px;letter-spacing:.2em}
p.l{margin:10px 0 34px;font:400 17px var(--f-text);color:var(--text-2)}
.g{display:grid;grid-template-columns:repeat(5,1fr);gap:28px 22px}figure{margin:0}.im{height:230px;overflow:hidden;border-radius:8px;border:1px solid #1a212a;background:#0a0d12}
img{width:100%;display:block}figcaption{margin-top:9px;font:600 11px var(--f-ui);letter-spacing:.14em;color:var(--fog)}figcaption span{color:var(--dim)}
</style></head><body><h1>FRASIL RIVER V2 <span>MOCKUPS · 24 OCT 2026 · AFTER STYVES'S DECISIONS</span></h1><p class="l">Static mockups at 1440 wide (key screens at 390 too). Audit in audit.md; the direction and Styves's decisions in README.md.</p><div class="g">${cards}</div></body></html>`);
const sheet = await browser.newPage({ viewport: { width: 1600, height: 900 } });
await sheet.goto(BASE + "png/_overview.html");
await sheet.evaluate(() => document.fonts.ready);
await sheet.waitForTimeout(400);
await sheet.screenshot({ path: path.join(PNG, "00-overview.png"), fullPage: true });
fs.rmSync(path.join(PNG, "_overview.html"));
await browser.close();
srv.close();
console.log("done");
