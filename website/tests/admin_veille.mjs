// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Styves Goupil
// The veille (admin.frasilsynth.com/veille/) in Chrome, with its service: the agent's mentions, the head, the list by
// day and priority, a link opened (seen, and it stays), Répondu, À suivre, Ignorer with UNDO (the button, u), the keys, a
// note, several at once, the filters, a service that refuses, what the web wrote shown as text; a phone's width, the
// system's day; no console errors.
//   node website/tests/admin_veille.mjs [OUT_DIR]          (screenshots; default a temp folder)
// It starts the service itself (server/.venv's uvicorn, a fresh database, port VEILLE_TEST_PORT or 8058, the admin site
// as tools/website.sh built it, or ADMIN_SITE; the key sent as Caddy would). PLAYWRIGHT: Playwright's index.mjs (default: the global one).
import fs from "fs";
import os from "os";
import path from "path";
import http from "http";
import { spawn } from "child_process";
import { fileURLToPath } from "url";
const { chromium } = await import(process.env.PLAYWRIGHT || "/opt/homebrew/lib/node_modules/playwright/index.mjs");

const REPO = path.join(path.dirname(fileURLToPath(import.meta.url)), "../..");
const OUT = process.argv[2] || fs.mkdtempSync(path.join(os.tmpdir(), "admin-veille-"));
fs.mkdirSync(OUT, { recursive: true });
const PORT = process.env.VEILLE_TEST_PORT || "8058", URL_ = `http://127.0.0.1:${PORT}`;
const KEY = "test-key", TOKEN = "test-token";
let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "  ok" : "FAIL"} ${msg}`); if (!ok) fails++; };

// Discord, as far as the service sees it
const posts = [];
let refuse = false;
const discord = http.createServer((q, s) => {
  let b = "";
  q.on("data", (c) => (b += c));
  q.on("end", () => {
    posts.push({ method: q.method, url: q.url, body: b ? JSON.parse(b) : null });
    s.writeHead(refuse ? 403 : 200, { "Content-Type": "application/json" });
    s.end(JSON.stringify({ id: String(1000 + posts.length) }));
  });
}).listen(Number(PORT) + 1, "127.0.0.1");

const work = fs.mkdtempSync(path.join(os.tmpdir(), "veille-db-"));
const server = spawn(path.join(REPO, "server/.venv/bin/uvicorn"), ["app:create_app", "--factory", "--port", PORT, "--log-level", "warning"], {
  cwd: path.join(REPO, "server"), stdio: "inherit",
  env: { ...process.env, RIVER_DB: path.join(work, "river.sqlite"), RIVER_STATIC: process.env.ADMIN_SITE || path.join(REPO, "build/website/admin"), RIVER_URL: URL_, ADMIN_URL: URL_,
    FRASIL_ADMIN_KEY: KEY, FRASIL_VEILLE_TOKEN: TOKEN, COOKIE_SECURE: "0", DISCORD_BOT_TOKEN: "test-bot", DISCORD_API: `http://127.0.0.1:${Number(PORT) + 1}`, DISCORD_MADE_CHANNEL: "42" } });
const stop = () => { server.kill(); discord.close(); };
process.on("exit", stop);

for (let i = 0; i < 80; i++) {
  try { if ((await fetch(`${URL_}/api/health`)).ok) break; } catch { /* not up yet */ }
  await new Promise((r) => setTimeout(r, 150));
}

const agent = (p, body) => fetch(`${URL_}/api/veille/${p}`, { method: body ? "POST" : "GET", body: body && JSON.stringify(body),
  headers: { Authorization: `Bearer ${TOKEN}`, "Content-Type": "application/json" } }).then((r) => r.json());
const admin = (q = "status=all") => fetch(`${URL_}/api/veille/mentions?${q}`, { headers: { "X-Frasil-Admin": KEY } }).then((r) => r.json());
const mention = (url, kw) => ({ url, platform: "blog", kind: "article", title: "Un titre", summary: "Ce qu'on en dit.", language: "fr", sentiment: "neutral", priority: 2, ...kw });
const hours = (n) => new Date(Date.now() - n * 3600e3).toISOString();

const { run_id } = await agent("ingest", { run: { started_at: hours(0.1) }, items: [
  mention("https://www.reddit.com/r/synthesizers/comments/abc123/fm1/", { platform: "reddit", kind: "thread", title: "FM-1 alt firmwares for ambient?", author: "someuser",
    community: "r/synthesizers", summary: "Demande quel firmware pour l'ambient; un commentaire recommande FRASIL pour les drones.", language: "en", sentiment: "question", priority: 3, engagement: 18, published_at: hours(30) }),
  mention("https://youtu.be/dQw4w9WgXcQ?si=1", { platform: "youtube", kind: "video", title: "FRASIL : un drone de 40 minutes", author: "Le Bruit Doux", community: "Le Bruit Doux", summary: "Vidéo enthousiaste, montre GLINT et le TAPE.", sentiment: "positive", priority: 2, engagement: 41, published_at: hours(5), moments: [{ at: 754, quote: "and this one is FRASIL" }, { at: 3725, quote: "" }] }),
  mention("https://www.youtube.com/watch?v=dQw4w9WgXcQ&lc=Ugx1", { platform: "youtube", kind: "comment", title: "Commentaire sous la vidéo de Hainbach", author: "mo", community: "Hainbach", summary: "Dit que la mise à jour a bloqué son FM-1 au boot.", sentiment: "bug", priority: 3, parent_url: "https://www.youtube.com/watch?v=hainbach1", language: "en" }),
  mention("https://github.com/StyvesGoupil/frasil/issues/12", { platform: "github", kind: "issue", title: "Support for the FM-1 v2", author: "styvesgoupil", summary: "Réponse de Styves sur la v2.", is_own: true, priority: 1 }),
  mention("https://modwiggler.com/forum/viewtopic.php?t=9", { platform: "forum", kind: "thread", title: "<img src=x onerror=window.pwned=1> Forum thread", community: "ModWiggler", summary: "<b>Trop</b> cher pour ce que c'est.", sentiment: "negative", priority: 1, language: "en" }),
  mention("https://blog.test/frasil-review", { title: "Une revue de FRASIL", author: "Ève", summary: "Bon article, très détaillé.", quote: "FRASIL sonne comme un lac gelé.", sentiment: "positive" }) ] });
await agent(`runs/${run_id}/finish`, { status: "partial", notes: "YouTube OK, Reddit OK, ModWiggler inaccessible" });

const browser = await chromium.launch({ channel: "chrome" });
const errors = [];
async function open(opts = {}) {
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 900 }, colorScheme: "dark", ...opts });
  await ctx.setExtraHTTPHeaders({ "X-Frasil-Admin": KEY });
  await ctx.route((u) => !u.href.startsWith(URL_), (r) => r.fulfill({ status: 200, contentType: "text/html", body: "<title>out</title>" }));
  await ctx.route("**/stats/downloads.json", (r) => r.fulfill({ status: 200, contentType: "application/json", body: '{"total": 447}' }));
  const page = await ctx.newPage();
  page.on("console", (m) => { if (m.type() === "error" && !/50[02]/.test(m.text())) errors.push(m.text()); });
  page.on("pageerror", (e) => { console.log(String(e.stack)); errors.push(String(e)); });
  await page.goto(`${URL_}/veille/`);
  await page.waitForSelector(".vl-card");
  return page;
}
const titles = (page) => page.$$eval(".vl-card .vl-t a", (a) => a.map((x) => x.textContent));
const counts = (page) => page.$$eval(".vl-count b", (b) => b.map((x) => Number(x.textContent)));
const settle = (page) => page.waitForTimeout(260);
const statusOf = async (title) => (await admin()).items.find((m) => m.title === title);

let page = await open();
check((await counts(page)).join() === "6,0,0,6", "the head: new, activity, to follow, total");
check(/il y a \d+ min — 6 nouveaux \(partielle\)/.test(await page.textContent("[data-run]")), "the last pass, partial");
check((await page.textContent("[data-run]")).includes("ModWiggler inaccessible"), "what failed in it");
check(!(await page.$(".vl-run.bad")), "not in red: recent, not in error");
check(await page.textContent(".vl-day") === "Aujourd'hui6", "grouped by the day found");
const first = await titles(page);
check(first.length === 6 && first.slice(0, 2).every((t) => /Hainbach|alt firmwares/.test(t)), "priority 3 first");
check(first[0].includes("Hainbach") && first.at(-1).includes("Support"), "then the date, the mention's own");
check((await page.$$eval(".vl-tag.own", (t) => t.map((x) => x.textContent))).join() === "Toi", "Toi on one's own");
check(await page.$eval(".vl-card.own", (c) => getComputedStyle(c).opacity < 1), "dimmed");
check(!(await page.evaluate(() => window.pwned)) && !(await page.$(".vl-card img")) && first.some((t) => t.startsWith("<img")), "what the web wrote shown as text");
check(await page.$$eval(".vl-t a", (a) => a.every((x) => x.target === "_blank" && x.rel.includes("noreferrer"))), "links in a new tab, no referrer");
check((await page.textContent(`.vl-card:has-text("Une revue de FRASIL") .vl-by`)).includes("trouvé il y a"), "no date of its own: when it was found, said so");
check(!(await page.textContent(`.vl-card:has-text("FRASIL : un drone") .vl-by`)).includes("trouvé"), "...one with a date: that date");
check((await page.textContent(`.vl-card:has-text("FRASIL : un drone") .vl-by`)).match(/Le Bruit Doux/g).length === 1, "an author who is the channel: once");
const moments = await page.$$eval(".vl-moments a", (a) => a.map((x) => [x.querySelector("b").textContent, x.getAttribute("href"), x.target]));
check(moments.length === 2 && moments[0][0] === "12:34" && moments[0][1].endsWith("watch?v=dQw4w9WgXcQ&t=754s") && moments[0][2] === "_blank", "a video's moment: its time, a link there");
check(moments[1][0] === "1:02:05" && (await page.textContent(".vl-moments span")) === "and this one is FRASIL", "...over an hour, and what is said");
check(await page.$eval(".vl-kid .vl-by a", (a) => a.textContent === "mo" && a.href.endsWith("&lc=Ugx1")), "a comment: its link, under its video's");
check((await page.$eval('.vl-card:has(.vl-kid) .vl-t a', (a) => a.href)).endsWith("watch?v=hainbach1"), "...the card's title the video's");
await page.screenshot({ path: path.join(OUT, "desktop-dark.png"), fullPage: true });

check(await page.$eval('.vl-card:has-text("Une revue") .vl-sum', (p) => p.textContent === "FRASIL sonne comme un lac gelé." && p.classList.contains("said")), "their words, not our summary");
check(await page.$eval(".vl-filters", (f) => f.firstElementChild.classList.contains("vl-row") && f.lastElementChild.classList.contains("vl-tabs")), "the filters above the statuses");
await page.click('.vl-chip[data-pf="youtube"]');
await page.waitForFunction(() => document.querySelector(".vl-tab i").textContent === "2");
check(true, "a provenance chosen: the statuses' counts follow it");
await page.click('.vl-chip[data-pf="youtube"]');
await page.waitForFunction(() => document.querySelector(".vl-tab i").textContent === "6");

// the head, the same on both pages
await page.waitForFunction(() => document.querySelector("[data-dl-digits]")?.getAttribute("aria-label") === "447 downloads");
await page.waitForTimeout(700);
check((await page.$$eval(".adm-cell", (c) => c.map((x) => x.dataset.d).join(""))) === "447" && (await page.textContent(".adm-cell:first-child")).startsWith("4"), "the downloads, as a flip clock");
const headOf = () => page.evaluate(() => [...document.querySelectorAll(".adm-nav a")].map((a) => [a.textContent, Math.round(a.getBoundingClientRect().left)]).join());
const inVeille = await headOf();
check(await page.$eval('.adm-nav a[aria-current="page"]', (a) => a.textContent === "VEILLE"), "VEILLE lit on its page");
await page.click('.adm-nav a:text("ADMIN")');
await page.waitForSelector('.adm-nav a[aria-current="page"]:text("ADMIN")');
await page.waitForTimeout(200);
check((await headOf()) === inVeille, `the navigation doesn't move from page to page (${inVeille})`);
await page.click('.adm-nav a:text("VEILLE")');
await page.waitForSelector(".vl-card");

// the pointer over a card
const yt = page.locator('.vl-card[data-pf="youtube"]').first();
const look = () => yt.evaluate((c) => { const s = getComputedStyle(c); return [s.transform, s.borderTopColor, s.backgroundColor].join(" | "); });
await page.mouse.move(0, 0);
await page.waitForTimeout(260);
const rest = await look();
await yt.hover();
await page.waitForTimeout(260);
const over = (await look()).split(" | ");
check(over[0].endsWith(", -3)") && over[1] === "rgb(255, 90, 82)" && over[2] !== rest.split(" | ")[2], "the pointer on a card: it lifts, its platform's colour, a lighter ground");
await page.mouse.move(0, 0);

// a link opened: seen, it stays there
const blog = "Une revue de FRASIL";
const [tab] = await Promise.all([page.context().waitForEvent("page"), page.click(`.vl-t a:text("${blog}")`)]);
await tab.close();
await settle(page);
check((await statusOf(blog)).status === "seen", "opening a link: new → seen");
check((await titles(page)).includes(blog), "...and it stays to be answered");
check((await counts(page))[0] === 5, "...Nouveaux follows");

// Répondu, UNDO's button
const q = "FM-1 alt firmwares for ambient?";
await page.click(`.vl-card:has-text("${q}") .vl-act:text("Répondu")`);
check(await page.isVisible("[data-toast] button"), "a toast with Annuler");
await settle(page);
check(!(await titles(page)).includes(q), "Répondu: it leaves the list");
check((await statusOf(q)).status === "replied", "...sent");
const before = await titles(page);
await page.click("[data-toast] button");
await settle(page);
check((await titles(page))[1] === q && (await statusOf(q)).status === "new", "Annuler: back where it was, and in the service");

// the keys
for (let i = 0; i < 6; i++) await page.keyboard.press("k");
await page.keyboard.press("j");
check((await page.textContent(".vl-card.cur .vl-t a")).includes("alt firmwares"), "j: the next");
await page.keyboard.press("k");
check((await page.textContent(".vl-card.cur .vl-t a")).includes("Hainbach"), "k: the one before");
await page.keyboard.press("f");
await settle(page);
check((await statusOf("Commentaire sous la vidéo de Hainbach")).status === "follow_up", "f: à suivre");
check((await counts(page))[2] === 1, "...counted");
check((await page.textContent(".vl-card.cur .vl-t a")).includes("alt firmwares"), "...the next one current");
await page.keyboard.press("i");
await settle(page);
check((await statusOf(q)).status === "ignored", "i: ignored");
await page.keyboard.press("u");
await settle(page);
await page.keyboard.press("u");
await settle(page);
check((await statusOf(q)).status === "new" && (await statusOf("Commentaire sous la vidéo de Hainbach")).status === "new", "u twice: both undone");
check((await titles(page)).join() === first.join(), "...the list as it was");

// a note
const was = (await statusOf(await page.textContent(".vl-card.cur .vl-t a"))).status;
await page.keyboard.press("n");
await page.keyboard.type("À rappeler");
await page.waitForTimeout(800);
await page.keyboard.press("Escape");
await settle(page);
const noted = (await admin()).items.find((m) => m.notes);
check(noted && noted.notes === "À rappeler" && noted.status === was, "n: a note, saved as it is typed; the status left");
check((await page.textContent(".vl-note-v")) === "À rappeler", "...shown");
await page.keyboard.press("j");
check((await page.evaluate(() => document.activeElement.className)).includes("vl-card"), "...the keys work again");

// several at once
await page.keyboard.press("x");
await page.keyboard.press("j");
await page.keyboard.press("x");
check(await page.textContent("[data-bulk-n]") === "2 sélectionnées", "x: two picked");
await page.click('[data-bulk-act="ignored"]');
await settle(page);
check((await admin("status=ignored")).items.length === 2 && (await titles(page)).length === 4, "ignored at once");
check(await page.isHidden("[data-bulk]"), "...nothing picked any more");
await page.click("[data-toast] button");
await settle(page);
check((await admin("status=ignored")).items.length === 0 && (await titles(page)).length === 6, "Annuler: both back");
await page.click(".vl-tick input");
await page.locator(".vl-tick input").last().click({ modifiers: ["Shift"] });
check(await page.textContent("[data-bulk-n]") === "6 sélectionnées", "shift: a range");
await page.click("[data-bulk-none]");

// into #made-with-frasil
const drone = "FRASIL : un drone de 40 minutes", act = `.vl-card:has-text("${drone}") .vl-do`;
await page.click(`${act} button:text("Discord")`);
await page.waitForSelector(`${act} a:text("Posté")`);
check(posts.length === 1 && posts[0].url === "/channels/42/messages" && posts[0].body.content.includes("watch?v=dQw4w9WgXcQ&t=754s"), "Discord: posted to the channel, at its moment");
check(!posts[0].body.content.includes("enthousiaste") && posts[0].body.allowed_mentions.parse.length === 0, "...not our summary, no mention possible");
check((await page.getAttribute(`${act} a:text("Posté")`, "href")).endsWith("/42/1001"), "...its button now the message's link");
check((await statusOf(drone)).status !== "replied" && (await titles(page)).includes(drone), "...the mention stays where it was");
await page.click("[data-toast] button");
await page.waitForSelector(`${act} button:text("Discord")`);
await settle(page);
check(posts.at(-1).method === "DELETE" && posts.at(-1).url === "/channels/42/messages/1001" && !(await statusOf(drone)).discord_url, "Annuler: the message taken back");
refuse = true;
await page.click(`${act} button:text("Discord")`);
await page.waitForFunction(() => document.querySelector("[data-toast]").textContent.includes("Discord n'a pas accepté"));
check(!(await statusOf(drone)).discord_url && await page.isVisible(`${act} button:text("Discord")`), "Discord refuses: said, and it can be tried again");
refuse = false;

// a service that refuses
await page.route("**/api/veille/mentions/*", (r) => r.fulfill({ status: 500, body: "{}" }));
await page.click(`.vl-card:has-text("${q}") .vl-act:text("Ignorer")`);
await page.waitForTimeout(350);
check((await titles(page)).includes(q) && (await page.textContent("[data-toast]")).includes("500"), "a refused change comes back, said");
await page.unroute("**/api/veille/mentions/*");

// the tabs, the filters
await page.click('.vl-count:has-text("Total")');
await page.waitForSelector(".vl-card");
await page.click('.vl-chip[data-pf="youtube"]');
await page.waitForFunction(() => document.querySelectorAll(".vl-card").length === 2);
check(true, "a platform's chip");
await page.click('.vl-chip[data-pf="youtube"]');
await page.fill("[data-find]", "evenement");
await page.waitForTimeout(350);
await page.fill("[data-find]", "eve detaille");
await page.waitForFunction(() => document.querySelectorAll(".vl-card").length === 1);
check((await titles(page))[0] === blog, "a search, accents aside");
await page.fill("[data-find]", "");
await page.selectOption("[data-sentiment]", "bug");
await page.waitForFunction(() => document.querySelector(".vl-card .vl-t a")?.textContent.includes("Hainbach") && document.querySelectorAll(".vl-card").length === 1);
check(true, "a sentiment");
await page.selectOption("[data-sentiment]", "");
await page.selectOption("[data-priority]", "1");
await page.waitForFunction(() => document.querySelectorAll(".vl-card").length === 2);
check(true, "a priority");
await page.selectOption("[data-priority]", "");
await page.click('.vl-tab:has-text("Répondus")');
await page.waitForSelector(".vl-empty");
check((await page.textContent(".vl-empty b")) === "Rien ici", "an empty tab says so");

// new activity, an old pass in error
const all = (await admin()).items, seenOne = all.find((m) => m.title === "FRASIL : un drone de 40 minutes");
await fetch(`${URL_}/api/veille/mentions/${seenOne.id}`, { method: "PATCH", body: JSON.stringify({ status: "replied" }),
  headers: { "X-Frasil-Admin": KEY, Origin: URL_, "Content-Type": "application/json" } });
const again = await agent("ingest", { run: { started_at: hours(9) }, items: [mention(seenOne.url, { title: seenOne.title, summary: seenOne.summary, platform: "youtube", kind: "video", sentiment: "positive", engagement: 50 })] });
await agent(`runs/${again.run_id}/finish`, { status: "error", notes: "Tout a échoué" });
page = await open();
check(await page.isVisible(".vl-run.bad"), "a pass in error: in red");
check((await counts(page))[1] === 1 && await page.isVisible('.vl-tag.lit'), "new activity, a badge and in the list");
await page.click(`.vl-card:has(.vl-tag.lit) .vl-act:text("Répondu")`);
await settle(page);
check((await admin()).counts.new_activity === 0 && (await statusOf(seenOne.title)).status === "replied", "the same status again clears it");

// a phone
const phone = await open({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true, deviceScaleFactor: 2 });
check(await phone.evaluate(() => document.documentElement.scrollWidth <= innerWidth), "a phone: no sideways scroll");
const tall = await phone.$$eval(".vl-card .vl-act", (b) => Math.min(...b.map((x) => x.getBoundingClientRect().height)));
check(tall >= 44, `its buttons for a thumb (${tall}px)`);
await phone.screenshot({ path: path.join(OUT, "phone-dark.png"), fullPage: false });
await phone.tap(".vl-tick input");
check(await phone.isVisible("[data-bulk]"), "a tick, the bar");
check(await phone.evaluate(() => document.querySelector("[data-bulk]").getBoundingClientRect().right <= innerWidth), "...inside the screen");
await phone.screenshot({ path: path.join(OUT, "phone-bulk.png") });

// the system's day
const day = await open({ colorScheme: "light" });
check(await day.evaluate(() => getComputedStyle(document.body).backgroundColor === "rgb(243, 246, 249)"), "the system's day: a light page");
await day.screenshot({ path: path.join(OUT, "desktop-light.png"), fullPage: true });

// all dealt with
await day.emulateMedia({ colorScheme: "dark" });
await day.click(".vl-tick input");
await day.click("[data-bulk-all]");
await day.click('[data-bulk-act="ignored"]');
await day.waitForSelector(".vl-empty");
check((await day.textContent(".vl-empty b")) === "Rien de neuf, tout est traité", "nothing left to do: it says so");
check(!(await day.textContent("[data-toast]")).includes("undefined"), "the toast's words");
await day.screenshot({ path: path.join(OUT, "desktop-empty.png") });

// UNDO from another tab; a page still on its way when the tab changes
await agent("ingest", { items: Array.from({ length: 60 }, (_, n) => mention(`https://more.test/${n}`, { title: `Encore ${n}` })) });
await day.reload();
await day.waitForSelector(".vl-card");
const undone = await day.textContent(".vl-card.cur .vl-t a");
await day.keyboard.press("f");
await day.click('.vl-tab:has-text("Vus")');
await day.waitForFunction(() => document.querySelector('.vl-tab[aria-selected="true"]')?.textContent.startsWith("Vus"));
await day.waitForTimeout(150);
await day.keyboard.press("u");
await day.waitForTimeout(300);
check((await statusOf(undone)).status === "new" && (await counts(day))[2] === 0, "UNDO from another tab: undone, and the head says so");
await day.click('.vl-tab:has-text("À traiter")');
await day.waitForFunction(() => document.querySelectorAll(".vl-card").length === 50);
await day.route("**/api/veille/mentions?*cursor=*", async (r) => { await new Promise((x) => setTimeout(x, 500)); await r.continue(); });
await day.locator("[data-more]").scrollIntoViewIfNeeded();
await day.waitForTimeout(100);
await day.click('.vl-tab:has-text("Ignorés")');
await day.waitForTimeout(800);
const stale = await titles(day);
check(stale.length > 0 && stale.length < 50 && !stale.some((t) => t.startsWith("Encore")), `a page from the list before: dropped (${stale.length} shown)`);
await day.unroute("**/api/veille/mentions?*cursor=*");

// a pass that never ended, long ago
await agent("ingest", { run: { started_at: hours(9) }, items: [] });
await day.reload();
await day.waitForSelector(".vl-run.bad");
check((await day.textContent("[data-run]")).includes("il y a 9 h — jamais terminée"), "a pass over 8 hours old: in red");

// a thread and its comments, the owner's reply among them
const T = "https://www.reddit.com/r/synthesizers/comments/grp1/";
const inT = (id, kw) => mention(`${T}comment/${id}/`, { platform: "reddit", kind: "comment", parent_url: T, parent_title: "Best ambient firmware?", community: "r/synthesizers", ...kw });
await agent("ingest", { items: [inT("aaa", { author: "ana", title: "c1", summary: "Recommande FRASIL.", published_at: hours(5) }), inT("bbb", { author: "bea", title: "c2", summary: "Aussi.", published_at: hours(4) }),
  inT("ccc", { author: "cid", title: "c3", summary: "Une question.", sentiment: "question", published_at: hours(3) })] });
await day.reload();
await day.fill("[data-find]", "Best ambient");
await day.waitForSelector('.vl-card:has-text("Best ambient firmware?")');
const G = '.vl-card:has-text("Best ambient firmware?")';
check((await day.$$(`${G} .vl-kid`)).length === 3 && (await day.textContent(`${G} .vl-by`)).includes("3 commentaires"), "three comments on a thread: one card, under its title");
await agent("ingest", { items: [inT("me", { author: "styvesgoupil", title: "c4", summary: "Merci !", is_own: true, published_at: hours(3.5) })] });
await day.click('.vl-tab:has-text("Semble avoir répondu")');
await day.waitForSelector(G);
check(true, "the owner's reply: the card under Semble avoir répondu");
await day.click('.vl-tab:has-text("À traiter")');
await day.waitForSelector(G);
check((await day.$$(`${G} .vl-kid`)).length === 4, "one comment after the reply: the card stays to be dealt with, all of it");
await day.click(`${G} .vl-act:text("Ignorer")`);
await settle(day);
check((await day.$$(G)).length === 0 && (await admin("q=c1&status=ignored")).items.length === 4, "an action on a card: every comment of it");
await day.click("[data-toast] button");
await settle(day);
check((await admin("status=ignored&q=c1")).items.length === 0 && (await day.$$(G)).length === 1, "Annuler: each as it was");
await day.click(`${G} .vl-act:text("Keeper")`);
await settle(day);
check((await day.$$(G)).length === 0 && (await admin("status=keeper&q=c1")).items.length === 4, "Keeper: the card leaves for the Hall of Fame");
await day.click('.vl-tab:has-text("Keepers")');
await day.waitForSelector(G);
check((await day.getAttribute(`${G} .vl-act:text("Keeper")`, "aria-pressed")) === "true", "...its tab holds it");
await day.fill("[data-find]", "");

check(!errors.length, `no console errors${errors.length ? ": " + errors.join(" | ") : ""}`);
await browser.close();
stop();
console.log(`\n${fails ? `${fails} FAILED` : "all ok"} (${OUT})`);
process.exit(fails ? 1 : 0);
