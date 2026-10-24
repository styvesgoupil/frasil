// FRASIL River v2: real content for the mockups, drawn by the River's own code (river-cards.js) from the example
// backup: each saved scene's glyph, its line of what plays, its keyboard, GLINT's strip and cells, every section with
// its pages' settings. Written to design/river-v2/data/scenes.json.
//   node design/river-v2/scripts/dump.mjs   (the River at RIVER_URL or localhost:8050)
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
const { chromium } = await import("/opt/homebrew/lib/node_modules/playwright/index.mjs");
const HERE = path.dirname(fileURLToPath(import.meta.url));
const BASE = process.env.RIVER_URL || "http://localhost:8050";
const browser = await chromium.launch({ channel: "chrome" });
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
await page.goto(BASE + "/backup/#example");
await page.waitForSelector("#shard-2");
const data = await page.evaluate(async () => {
  const out = { defs: document.querySelector('svg defs clipPath#rv-clip-up').closest("svg").outerHTML, scenes: [] };
  for (const art of document.querySelectorAll("#cards .shard:not(.is-empty):not(.settings)")) {
    const name = art.querySelector(".sh-name").textContent;
    RiverCards.openCard(art, true, true);
    await new Promise((r) => setTimeout(r, 50));
    art.querySelectorAll("details.sec").forEach((d) => { d.open = true; d.dispatchEvent(new Event("toggle")); });
    await new Promise((r) => setTimeout(r, 50));
    const q = (s) => art.querySelector(s);
    const sum = [...art.querySelectorAll(".sh-sum dd")].map((d) => d.textContent);
    out.scenes.push({
      name, state: art.classList.contains("is-factory") ? "factory" : "saved", slot: art.id,
      glyph: q(".sh-glyph svg").outerHTML,
      ms: q(".ms").outerHTML,
      key: sum[0], drone: sum[1], glint: sum[2],
      keyboard: q(".sh-plays .scr").innerHTML,
      legend: q(".lg").outerHTML,
      glintHead: { type: q(".sh-glint .gl-type") && q(".sh-glint .gl-type").textContent, meta: [...art.querySelectorAll(".sh-glint .gl-v")].map((x) => x.textContent) },
      strip: q(".sh-glint .scr") ? q(".sh-glint .scr").innerHTML : null,
      cells: q(".sh-glint .cells4").innerHTML,
      secs: [...art.querySelectorAll("details.sec")].map((d) => ({
        name: d.querySelector(".sec-name").textContent,
        mid: d.querySelector(".sec-mid").innerHTML,
        keys: [...d.querySelectorAll(".sec-key")].map((k) => k.textContent),
        strip: d.querySelector(".sec-body > .scr") ? d.querySelector(".sec-body > .scr").innerHTML : null,   // (BASS: its sequence's line of time)
        pages: [...d.querySelectorAll(".pg")].map((pg) => ({ name: pg.querySelector(".pg-name").textContent, cells: [...pg.querySelectorAll(".cell")].map((c) => c.outerHTML) })),
      })),
    });
  }
  return out;
});
fs.mkdirSync(path.join(HERE, "../data"), { recursive: true });
fs.writeFileSync(path.join(HERE, "../data/scenes.json"), JSON.stringify(data, null, 1));
console.log(data.scenes.map((s) => `${s.name} ${s.state} ${s.key} / ${s.drone} / ${s.glint} · ${s.secs.length} sections`).join("\n"));
await browser.close();
