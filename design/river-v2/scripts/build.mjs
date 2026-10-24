// FRASIL River v2: the mockups, written as static HTML into design/river-v2/ (then scripts/render.mjs makes the PNGs).
//   node design/river-v2/scripts/build.mjs
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { D, I, page, glyph, row, listHead, opened, illus, illusClose, dlg, dh, dhMap, ms, shardOf, realShard, real, PANEL } from "./parts.mjs";
const OUT = path.join(path.dirname(fileURLToPath(import.meta.url)), "..");
const pages = {};
const put = (name, html) => { pages[name] = html; fs.writeFileSync(path.join(OUT, name + ".html"), html); };

// ================================================================ the library
const ME = "Styves";
const abyss = realShard("ABYSS", { desc: "Two fifths in D phrygian, a bass far below. Let it rise slowly, then FREEZE the choir.", date: "23 OCT", shared: true });
const cathedral = realShard("CATHEDRAL", { desc: "", date: "23 OCT" });
const glacier = realShard("GLACIER", { desc: "A sequence in seven over a slow D major: the ice knocks, the river carries it.", date: "21 OCT", shared: true });
const aurora = realShard("AURORA", { desc: "", date: "19 OCT", bseq: true });     // (the example's: D2, HOLD, A2, CUT, G2, E2)
const S = (a, o) => shardOf(a, o);
const BQ = { bseq: true };                                                          // the bass plays its sequence
const MINE = [
  abyss, cathedral, glacier,
  S(["NORTHERN LIGHT", "A", "LYDIAN", "ADD9", "SHIMMER", "ARP", "Lydian shimmer, the arpeggio high and slow. OCT +1 on GLINT.", ME, "20 OCT", true]),
  S(["FRAZIL DRIFT", "E", "MINOR", "FIFTH", "HAZE", "SEQ", "", ME, "20 OCT", false], BQ),
  aurora,
  S(["SLOW THAW", "F", "DORIAN", "SUS2", "TIDE", "OFF", "For the end of a set: let it ring for ten minutes.", ME, "18 OCT", true]),
  S(["BLACK ICE", "C", "PHRYGIAN", "FIFTH", "DEEP", "OFF", "", ME, "17 OCT", false]),
  S(["ST LAWRENCE", "G", "MIXOLYDIAN", "TRIAD", "CHANTER", "ARP", "The river at Tadoussac, four in the morning.", ME, "15 OCT", true]),
  S(["FOG HORN", "D", "MINOR", "SINGLE", "HOLLOW", "OFF", "", ME, "14 OCT", false]),
  S(["WHITEOUT", "B", "PENTA MIN", "QUARTAL", "WIND", "SEQ", "", ME, "12 OCT", false]),
  S(["POLAR NIGHT", "C#", "MINOR", "MIN7", "NOCTURNE", "OFF", "Dark, close, warm in the middle.", ME, "11 OCT", true]),
  S(["ICE FOG", "A", "DORIAN", "SUS4", "MIST", "ARP", "", ME, "09 OCT", false]),
  S(["BOREAL", "E", "LYDIAN", "MAJ7", "ENSEMBLE", "OFF", "", ME, "08 OCT", false]),
  S(["CALVING", "F#", "PHRYGIAN", "FIFTH", "RUST", "SEQ", "Low and broken; PUMP on the beat.", ME, "06 OCT", false], BQ),
  S(["PACK ICE", "D", "MINOR", "SEVENTH", "ORGAN", "OFF", "", ME, "05 OCT", false]),
  S(["UNDERTOW", "G", "HARMONIC", "MIN", "DEEP", "OFF", "", ME, "03 OCT", false], BQ),
  S(["EDDY", "C", "MAJOR", "ADD9", "BELLS", "ARP", "", ME, "02 OCT", false]),
  S(["HOARFROST", "A", "MINOR", "QUARTAL", "RIME", "SEQ", "", ME, "29 SEP", false]),
  S(["LAKE EFFECT", "B", "DORIAN", "SUS2", "VOWEL", "OFF", "", ME, "27 SEP", false]),
  S(["FIRST SNOW", "E", "PENTA MAJ", "TRIAD", "SHIMMER", "ARP", "", ME, "26 SEP", false]),
  S(["SEA SMOKE", "D", "PHRYGIAN", "FIFTH", "HAZE", "OFF", "", ME, "24 SEP", false], BQ),
  S(["NEEDLE ICE", "F", "LYDIAN", "ADD9", "SPECTRAL", "SEQ", "", ME, "22 SEP", false]),
  S(["CATHEDRAL OF ICE", "D", "MAJOR", "MAJ7", "CATHEDRAL", "OFF", "", ME, "20 SEP", false]),
];
/* the Community River, newest first (its default order); the date each was shared */
const COMM = [
  S(["TADOUSSAC 4AM", "D", "DORIAN", "FIFTH", "TIDE", "ARP", "Belugas somewhere under the fog. GLINT on CHIME, very far.", "Léa Painchaud", "23 OCT", true, 3]),
  { ...abyss, author: ME, favs: 4, date: "23 OCT" },
  S(["NILAS", "A", "PENTA MIN", "QUARTAL", "SPECTRAL", "SEQ", "Thirty-two steps, half of them rests.", "Jonas H.", "22 OCT", true, 6], BQ),
  S(["GREASE ICE", "E", "MINOR", "MIN7", "HAZE", "OFF", "Warm, close voicing; turn BODY down for a thinner one.", "northbound", "22 OCT", true, 9]),
  { ...glacier, author: ME, favs: 37, date: "21 OCT" },
  S(["KATABATIC", "C", "PHRYGIAN", "FIFTH", "WIND", "OFF", "The wind coming down the glacier. LFO 2 on BREATH.", "Oksana", "21 OCT", true, 19], BQ),
  S(["POLYNYA", "F#", "LYDIAN", "ADD9", "SHIMMER", "OFF", "", "tidewater", "20 OCT", true, 22]),
  S(["BERGY BIT", "G", "MIXOLYDIAN", "SUS4", "BELLS", "ARP", "", "cold.room", "19 OCT", true, 17]),
  S(["PANCAKE ICE", "B", "DORIAN", "SUS2", "VOWEL", "OFF", "Soft, round, a little out of tune on purpose.", "Mira K.", "18 OCT", true, 15]),
  S(["ANCHOR ICE", "D", "MINOR", "SINGLE", "DEEP", "OFF", "", "northbound", "18 OCT", true, 12], BQ),
  S(["DIAMOND DUST", "E", "LYDIAN", "MAJ7", "RIME", "SEQ", "An arpeggio like snow on a tin roof.", "Léa Painchaud", "17 OCT", true, 11]),
  S(["FJORD", "A", "MINOR", "FIFTH", "CHANTER", "OFF", "", "Jonas H.", "16 OCT", true, 41]),
  S(["SASTRUGI", "C#", "HARMONIC", "MIN", "RUST", "SEQ", "", "Oksana", "15 OCT", true, 8], BQ),
  S(["MORAINE", "F", "MAJOR", "TRIAD", "ORGAN", "OFF", "Church organ, far away, through the snow.", "tidewater", "14 OCT", true, 7]),
  S(["GROWLER", "G", "PHRYGIAN", "FIFTH", "PULSE", "ARP", "", "cold.room", "12 OCT", true, 29], BQ),
  S(["ICE SHELF", "D", "DORIAN", "QUARTAL", "ENSEMBLE", "OFF", "", "Mira K.", "11 OCT", true, 5]),
  S(["LEAD", "E", "PENTA MIN", "SUS4", "HOLLOW", "OFF", "", "northbound", "09 OCT", true, 4]),
  S(["CREVASSE", "B", "LOCRIAN", "FIFTH", "DEEP", "SEQ", "", "Jonas H.", "08 OCT", true, 33]),
];

// ================================================================ the pages' parts
const dock = (n = 0) => `<div class="dock"><button class="dock-b">${I.get()} Get from FM-1</button><button class="dock-b${n ? " is-hot" : ""}">${I.send()} ${n ? `Send <span class="cnt">${n}</span> to FM-1` : "Send to FM-1"}</button></div>`;
const tabs = (on, list) => `<div class="pt-tabs">${list.map(([t, n]) => `<a class="pt-t${t === on ? " is-on" : ""}" href="#">${t}${n != null ? ` <span class="n">${n}</span>` : ""}</a>`).join("")}</div>`;
const privTitle = (on, counts, n = 0) => `<div class="pt">${tabs(on, [["SHARDS", counts[0]], ...(counts[1] ? [["FAVORITES", counts[1]]] : []), ["BACKUPS", counts[2]]])}<div class="pt-acts">${dock(n)}</div></div>`;
const FILTERS = (pub) => [["KEY", "Any"], ["SCALE", "Any"], ["GLINT", "Any"], ["BASS", "Any"], ...(pub ? [] : [["SHARED", "Any"]])];
/* the order: a menu in one's own River; in the Community's, its three orders side by side, one click each */
const sortCtl = (o) => o.pub ? `<div class="seg" role="radiogroup" aria-label="Order">${["NEWEST", "MOST PLAYED", "FAVORITES"].map((t) => `<span class="${t === (o.sort || "NEWEST") ? "on" : ""}">${t}</span>`).join("")}</div>`
  : `<span class="f"><span class="f-l">SORT</span><span class="f-v">${o.sort || "Newest"}</span>${I.dn()}</span>`;
const toolbar = (o = {}) => `<div class="tb"><div class="tb-q">${I.q()}${o.q ? `<span class="typed">${o.q}</span><span class="caret"></span>` : `<span>${o.ph || "Find a shard: a name, a word"}</span>`}</div>
  <div class="tb-fs">${(o.filters || FILTERS(o.pub)).map(([l, v]) => `<span class="f"><span class="f-l">${l}</span><span class="f-v${v !== "Any" ? " is-set" : ""}">${v}</span>${I.dn()}</span>`).join("")}</div>
  <div class="tb-end"><span class="tb-n">${o.n}</span>${sortCtl(o)}</div></div>`;
const metaMine = (x) => `Added 23 Oct 2026 from slot 03 of your FM-1<span class="dot">·</span>FRASIL 1.1<span class="dot">·</span>shared in the Community River on 23 Oct under <a href="#">CC BY 4.0</a><span class="dot">·</span>12 plays, sent to 3 FM-1s, 4 favorites`;

// ================================================================ 1. Your Private River
{
  const lst = [abyss, cathedral];
  const body = `${privTitle("SHARDS", [2, 0, 1])}
  <div class="list">${listHead()}${row(abyss)}${row(cathedral, { hover: true })}</div>
  <div class="list-end"><p>Get more from your FM-1, or favorite shards in the <a href="#">Community River</a>.</p></div>`;
  put("01-private-2", page({ title: "Your Private River, 2 shards", cur: "yours", who: ME, body }));
}
{
  const rows = MINE.map((s, i) => row(s, { sel: [3, 6, 9].includes(i), playing: i === 1 })).join("");
  const body = `${privTitle("SHARDS", [100, 12, 3], 3)}
  ${toolbar({ n: "100 SHARDS" })}
  <div class="list"><div class="selbar"><b>3 SELECTED</b><span class="sep"></span><span class="b b-txt">${I.dl()} Download</span><span class="b b-txt">${I.share()} Share</span><span class="b b-txt">Delete</span><span class="b b-txt" style="margin-left:auto">Clear</span></div>${rows}</div>
  <div class="list-end"><p>24 of 100</p><span class="more">SHOW 24 MORE</span></div>`;
  put("02-private-100", page({ title: "Your Private River, 100 shards", cur: "yours", who: ME, body }));
}
{
  const bks = [
    ["23 OCT 2026", "20:43", "Before the show", [["03", "ABYSS"], ["09", "CATHEDRAL"], ["10", "GLACIER"], ["11", "AURORA"]], "4 saved, 7 factory, 5 empty · FRASIL 1.1"],
    ["17 OCT 2026", "22:10", "", [["01", "BLACK ICE"], ["02", "SLOW THAW"], ["03", "ABYSS"], ["05", "FOG HORN"], ["09", "CATHEDRAL"], ["12", "POLAR NIGHT"], ["13", "ICE FOG"]], "7 saved, 5 factory, 4 empty · FRASIL 1.1"],
    ["02 OCT 2026", "09:05", "The first one, still on 1.0", [], "Made by FRASIL 1.0: brought up to date when restored"],
  ];
  const rows = bks.map(([d, t, note, names, meta]) => `<div class="bu"><div class="bu-when"><b>${d}</b><span>${t}</span></div>
    <div><p class="bu-note${note ? "" : " none"}">${note || "No note"}</p><p class="bu-names">${names.length ? names.map(([n, x]) => `<i>${n}</i>${x}`).join('<span class="sep">·</span>') : ""}</p>${names.length ? "" : `<p class="bu-meta">${meta}</p>`}${names.length ? `<p class="bu-meta">${meta}</p>` : ""}</div>
    <div class="bu-acts"><span class="a">${I.send(15)} RESTORE TO FM-1</span><span class="a">${I.dl()} DOWNLOAD</span><span class="a a-txt">DELETE</span></div></div>`).join("");
  const body = `${privTitle("BACKUPS", [100, 12, 3])}
  <div class="list" style="margin-top:6px">${rows}</div>
  <div class="list-end"><p>A backup holds the FM-1's sixteen slots and its settings. Store one with <span style="color:var(--fog)">Get from FM-1</span>.</p></div>`;
  put("02c-private-backups", page({ title: "Your Private River, backups", cur: "yours", who: ME, body }));
}

// ================================================================ 2. the card: one's own, someone else's
{
  const body = `${privTitle("SHARDS", [28, 4, 1])}
  <div class="list">${listHead()}${row(MINE[3])}
  ${opened("ABYSS", { mine: true, shared: true, desc: abyss.desc, meta: metaMine(), open: ["DRONE"], pagesMax: 2 })}
  ${row(cathedral)}${row(MINE[4])}</div>`;
  put("04-card-own", page({ title: "One's own shard, opened", cur: "yours", who: ME, body }));
}
{
  const meta = `by <b>Styves</b><span class="dot">·</span>shared 21 Oct 2026 under <a href="#">CC BY 4.0</a><span class="dot">·</span>FRASIL 1.1<span class="dot">·</span>212 plays, sent to 31 FM-1s, 37 favorites`;
  const body = `${commTitle()}
  ${toolbar({ n: "142 SHARDS", pub: true })}
  <div class="list pub">${listHead({ pub: true, sort: "SHARED" })}${row(COMM[3], { pub: true })}
  ${opened("GLACIER", { pub: true, desc: glacier.desc, meta, open: [], fav: false })}
  ${row(COMM[5], { pub: true })}${row(COMM[6], { pub: true })}</div>`;
  put("05-card-other", page({ title: "Someone else's shard, opened", cur: "community", who: "Mira K.", body }));
}
{
  const meta = `Added 23 Oct 2026 from slot 11 of your FM-1<span class="dot">·</span>FRASIL 1.1`;
  const body = `${privTitle("SHARDS", [28, 4, 1])}
  <div class="list">${listHead()}${row(glacier)}
  ${opened("AURORA", { mine: true, desc: "", meta, open: ["BASS"], pagesMax: 3 })}
  ${row(MINE[6])}${row(MINE[7])}</div>`;
  put("04b-card-bass", page({ title: "One's own shard, its bass's sequence", cur: "yours", who: ME, body }));
}
function commTitle() {
  return `<div class="pt">${tabs("SHARDS", [["SHARDS", 142]])}<p class="pt-sub">Scenes shared by FRASIL players under CC BY 4.0: play them here, send them to your FM-1.</p></div>`;
}

// ================================================================ 3. the Community River
{
  const rows = COMM.map((s, i) => row(s, { pub: true, playing: i === 0, hover: i === 3 })).join("");
  const body = `${commTitle()}${toolbar({ n: "142 SHARDS", pub: true })}
  <div class="list pub">${listHead({ pub: true, sort: "SHARED" })}${rows}</div>
  <div class="list-end"><p>18 of 142</p><span class="more">SHOW 24 MORE</span></div>`;
  put("30-community", page({ title: "The Community River", cur: "community", body }));
}

// ================================================================ 4. page titles
{
  const t = (cap, html) => `<div style="padding:26px 0 8px"><p class="bd-l" style="padding:0 0 0">${cap}</p>${html}</div>`;
  const body = `<div class="board"><h2>PAGE TITLES</h2>
  <p>The FM-1's own screen header, for the web: the page's subject in the instrument's tracked capitals, its siblings small beside it, a count, and a waterline lit under the title. It names what is on the page, never the place: the header already says where you are.</p></div>
  ${t("<b>YOUR PRIVATE RIVER</b><span>The shards, the favorites, the backups: the title is also how you switch between them.</span>", privTitle("SHARDS", [28, 4, 3]))}
  ${t("<b>YOUR PRIVATE RIVER · BACKUPS</b><span>The same line, another subject.</span>", privTitle("BACKUPS", [28, 4, 3]))}
  ${t("<b>COMMUNITY RIVER</b><span>How many shards drift there, and one sentence for whoever arrives from the main site.</span>", commTitle())}
  ${t("<b>TOOLS</b><span>One tool, so the page is that tool.</span>", toolsTitle())}
  ${t("<b>PROFILE</b><span>The account; its name is already in the header.</span>", `<div class="pt">${tabs("ACCOUNT", [["ACCOUNT"]])}<p class="pt-sub">Since October 2026 · 100 shards, 9 shared</p></div>`)}
  <div style="height:30px"></div>`;
  put("03-titles", page({ title: "Page titles", cur: "yours", who: ME, body }));
}
function toolsTitle(n) {
  return `<div class="pt"><div class="pt-tabs"><a class="pt-t is-on" href="#">QUICK BACKUP</a><span class="pt-sub">No account. Nothing leaves this computer.</span></div>
  <div class="pt-acts"><div class="dock"><button class="dock-b">${I.get()} Back up the FM-1</button><button class="dock-b">${I.open()} Open a file</button></div></div></div>`;
}

// ================================================================ 5. the device hints
{
  const secs = real("GLACIER").secs;
  const v2 = secs.map((x) => `<div class="sx"><div class="sx-r">${I.chev()}<span class="sx-n">${x.name}</span><span class="sx-m">${x.mid}</span>${dh(x.keys)}</div></div>`).join("");
  const now = secs.map((x) => `<div class="sx"><div class="sx-r">${I.chev()}<span class="sx-n">${x.name}</span><span class="sx-m">${x.mid}</span><span class="sec-keys">${x.keys.map((k) => `<span class="sec-key">${k}</span>`).join("")}</span></div></div>`).join("");
  const alt = (kind) => secs.slice(0, 5).map((x) => `<div class="sx"><div class="sx-r">${I.chev()}<span class="sx-n">${x.name}</span><span class="sx-m">${x.mid}</span>${kind === "text" ? `<span class="dh-k" style="font:italic 400 14px var(--f-text);letter-spacing:0;color:var(--dim)">on ${x.keys.join(", ")}</span>` : `<span class="dh">${x.keys.map((k) => `<span style="display:inline-grid;place-items:center;height:20px;padding:0 7px;border-radius:5px;background:linear-gradient(#1e2731,#141a22);box-shadow:inset 0 -2px 0 #0a0d12, 0 1px 0 #000;font:700 9.5px/1 var(--f-ui);letter-spacing:.12em;color:#8b95a4">${k}</span>`).join("")}</span>`}</div></div>`).join("");
  const big = (() => {
    let s = `<svg width="460" height="150" viewBox="0 0 460 150" aria-hidden="true">`;
    PANEL.forEach((rw, y) => rw.forEach((b, x) => {
      const on = b === "EDIT" || b === "ENV" || b === "LFO";
      s += `<rect x="${10 + x * 74}" y="${14 + y * 66}" width="60" height="38" rx="8" fill="${on ? "rgba(110,208,255,.16)" : "#141a22"}" stroke="${on ? "#6ed0ff" : "#26303c"}"/>`;
      s += `<text x="${40 + x * 74}" y="${38 + y * 66}" text-anchor="middle" font-family="Barlow Semi Condensed" font-weight="600" font-size="12" letter-spacing="1.6" fill="${on ? "#f3f6f9" : "#77818f"}">${b}</text>`;
    }));
    return s + `</svg>`;
  })();
  const body = `<div class="board"><h2>THE FM-1'S BUTTONS, IN A CARD</h2>
  <p>Today the buttons that lead to each page (EDIT, ENV, LFO, ARP…) are drawn exactly like the River's own buttons: a frame, caps, a radius. They look clickable, and they are not. In v2 they become a diagram: the FM-1's block of twelve buttons, tiny, with the ones that lead there lit, and their names as quiet text. Nothing framed, nothing that reacts to the mouse: information about the instrument, not a control of the page.</p></div>
  <div class="vs"><div class="vs-c"><h3 class="now">NOW: THEY LOOK LIKE BUTTONS</h3><div class="oc-right" style="border:0">${now}</div></div>
  <div class="vs-c"><h3 class="v2">V2: A MAP OF THE PANEL (RECOMMENDED)</h3><div class="oc-right" style="border:0"><div class="sx-h"><span>EVERY PAGE</span><span>ON THE FM-1</span></div>${v2}</div></div></div>
  <div class="vs-c" style="margin-top:40px;display:grid;grid-template-columns:500px minmax(0,1fr);gap:40px;align-items:center"><div><h3>HOW TO READ IT</h3>${big}</div><p style="font:400 18px/1.55 var(--f-text);color:var(--text-2)">The twelve buttons at the FM-1's right, as they sit on its panel. DRONE's pages are under <b style="color:var(--fog);font-weight:500">EDIT</b>, <b style="color:var(--fog);font-weight:500">ENV</b> and <b style="color:var(--fog);font-weight:500">LFO</b>, so those three are lit, and their names follow in quiet text. In an opened section each page also names its own button, under its name. The map is never framed, never reacts to the mouse: it reads as a picture of the instrument.</p></div>
  <div class="vs">
    <div class="vs-c"><h3>ALTERNATIVE B: QUIET TEXT ONLY</h3><div class="oc-right" style="border:0">${alt("text")}</div></div>
    <div class="vs-c"><h3>ALTERNATIVE C: THE KEYCAP, NO FRAME</h3><div class="oc-right" style="border:0">${alt("cap")}</div></div>
  </div><div style="height:30px"></div>`;
  put("06-device-hints", page({ title: "The device hints", cur: "community", body }));
}

// ================================================================ 6. later: live, the editor
{
  const lfo = (n, shape, rate, tg, phase, on = true) => {
    const W = 180, H = 44;
    let d = "";
    for (let x = 0; x <= W; x++) { const u = x / W; const v = shape === "sine" ? Math.sin(2 * Math.PI * u) : shape === "tri" ? (u < .25 ? 4 * u : u < .75 ? 2 - 4 * u : 4 * u - 4) : (Math.sin(2 * Math.PI * u * 2) * .6 + Math.sin(2 * Math.PI * u * 5 + 1) * .3); d += `${x ? "L" : "M"}${x} ${(H / 2 - v * (H / 2 - 4)).toFixed(1)}`; }
    const px = phase * W, pv = shape === "sine" ? Math.sin(2 * Math.PI * phase) : shape === "tri" ? (phase < .25 ? 4 * phase : phase < .75 ? 2 - 4 * phase : 4 * phase - 4) : (Math.sin(2 * Math.PI * phase * 2) * .6 + Math.sin(2 * Math.PI * phase * 5 + 1) * .3);
    const py = H / 2 - pv * (H / 2 - 4);
    return `<div class="lfo-c"><div class="cap">LFO ${n} <span>${rate}</span></div><svg width="${W}" height="${H}" viewBox="0 0 ${W} ${H}" style="display:block;overflow:visible"><line x1="0" x2="${W}" y1="${H / 2}" y2="${H / 2}" stroke="#26303c"/><path d="${d}" fill="none" stroke="${on ? "#6ed0ff" : "#4a5563"}" stroke-width="1.3" opacity="${on ? .45 : .8}"/><path d="${d.split("L").slice(0, Math.round(px) + 1).join("L")}" fill="none" stroke="${on ? "#6ed0ff" : "#4a5563"}" stroke-width="1.5"/>${on ? `<line x1="${px}" x2="${px}" y1="0" y2="${H}" stroke="#6ed0ff" opacity=".25"/><circle cx="${px}" cy="${py}" r="7" fill="#6ed0ff" opacity=".2"/><circle cx="${px}" cy="${py}" r="2.8" fill="#f3f6f9"/>` : ""}</svg><div class="tg">${tg}</div></div>`;
  };
  const col = `<div class="lfo-col">${lfo(1, "sine", "0.08 HZ", '<span style="color:#dce3ea">CHOIR LEVEL</span><span style="color:#dce3ea">BREATH</span>', .36)}${lfo(2, "tri", "1/2", '<span style="color:#6ed0ff">GLINT BRIGHT</span>', .7)}${lfo(3, "drift", "OFF", '<span style="color:var(--dim)">NO TARGET</span>', .2, false)}</div>`;
  const meta = `Added 21 Oct 2026<span class="dot">·</span>FRASIL 1.1<span class="dot">·</span>shared under <a href="#">CC BY 4.0</a>`;
  const body = `${privTitle("SHARDS", [28, 4, 1])}
  <div class="list">${opened("GLACIER", { mine: true, shared: true, playing: true, desc: glacier.desc, meta, open: [], mids: { "LFO 1": `<svg class="sec-gl" width="46" height="14" viewBox="0 0 46 14" aria-hidden="true"><line x1="1" x2="45" y1="7" y2="7" stroke="#26303c"/><path d="M1 7.0L2 6.2L3 5.5L4 4.7L5 4.0L6 3.4L7 2.8L8 2.4L9 2.0L10 1.7L11 1.6L12 1.5L13 1.6L14 1.7L15 2.0L16 2.4L17 2.8L18 3.4L19 4.0L20 4.7L21 5.5L22 6.2L23 7.0L24 7.8L25 8.5L26 9.3L27 10.0L28 10.6L29 11.2L30 11.6L31 12.0L32 12.3L33 12.4L34 12.5L35 12.4L36 12.3L37 12.0L38 11.6L39 11.2L40 10.6L41 10.0L42 9.3L43 8.5L44 7.8L45 7.0" fill="none" stroke="#6ed0ff" stroke-width="1.3" stroke-linejoin="round"/></svg><span class="sig-v">SINE</span><span class="sig-v" style="color:#aeb7c2">0.08 HZ</span><span class="sig-v">CHOIR LEVEL</span><span class="sig-v">BREATH</span>`, "LFO 2": `<svg class="sec-gl" width="46" height="14" viewBox="0 0 46 14" aria-hidden="true"><line x1="1" x2="45" y1="7" y2="7" stroke="#26303c"/><path d="M1 7L12 1.5L34 12.5L45 7" fill="none" stroke="#6ed0ff" stroke-width="1.3" stroke-linejoin="round"/></svg><span class="sig-v">TRIANGLE</span><span class="sig-v" style="color:#aeb7c2">1/2</span><span class="sig-v" style="color:#6ed0ff">GLINT BRIGHT</span>` }, live: { col, keys: [{ n: 50, c: "white" }, { n: 74, c: "ice" }, { n: 81, c: "ice" }, { n: 38, c: "lamp" }], step: 10 } })}</div>
  <div class="board" style="padding-top:26px"><p>While a shard plays, its card follows the sound: the keys the drones, GLINT and BASS hold light up as they sound, GLINT's sequence runs under a playhead, and a column of the three LFOs moves at their own rate, each showing what it moves in its track's colour. Nothing to read: a way to see why a sound breathes as it does.</p></div>`;
  put("07-later-live", page({ title: "Later: live while it plays", cur: "yours", who: ME, body }));
}
{
  const e = { sec: "DRONE", changed: ["CHOIR/1", "DRONE PARAMS/1"], grab: "CHOIR/0", vals: { "CHOIR/0": "96", "CHOIR/1": "34", "DRONE PARAMS/1": "88" }, f: { "CHOIR/0": .76, "CHOIR/1": .27, "DRONE PARAMS/1": .69 } };
  const editActs = `<span class="a">${I.undo()} UNDO</span><span class="a a-txt">REVERT</span><span class="a a-ice">SAVE AS…</span>`;
  const meta = `Added 23 Oct 2026 from slot 03 of your FM-1<span class="dot">·</span>FRASIL 1.1`;
  const pop = `<div class="pop" style="right:58px;top:68px"><p class="field-l" style="color:var(--mist)">SAVE AS A NEW SHARD</p>
    <div style="display:grid;gap:7px"><span class="field-l">NAME, AS THE FM-1 SHOWS IT</span><div class="inp name">ABYSS II<span class="caret" style="display:inline-block;width:1px;height:17px;background:var(--lamp);margin-left:2px"></span><span style="margin-left:auto;font:500 11px var(--f-ui);letter-spacing:.08em;color:var(--dim)">8 / 16</span></div></div>
    <div style="display:grid;gap:7px"><span class="field-l">DESCRIPTION</span><div class="inp" style="height:64px;align-items:flex-start;padding-top:10px"><span class="ph">Optional</span></div></div>
    <p style="font:400 14px/1.45 var(--f-text);color:var(--dim)">ABYSS stays as it is. The new shard goes into Your Private River.</p>
    <div style="display:flex;justify-content:flex-end;gap:8px"><span class="a">CANCEL</span><span class="a a-ice">SAVE</span></div></div>`;
  const body = `${privTitle("SHARDS", [28, 4, 1])}
  <div class="list" style="position:relative">${opened("ABYSS", { mine: true, desc: abyss.desc, meta, open: ["DRONE"], pagesMax: 3, edit: e, editActs, playing: true })}${pop}</div>
  <div class="board" style="padding-top:26px"><p>The opened card becomes an editor: drag a value (or turn it with the arrow keys), and the sound in the browser follows at once. A touched value turns lamp, the colour of the hand. <b style="color:var(--fog);font-weight:500">Save as</b> makes a new shard in Your Private River; then Send to FM-1 as any other.</p></div>`;
  put("08-later-editor", page({ title: "Later: the editor", cur: "yours", who: ME, body }));
}

// ================================================================ 7. Send to FM-1
// Today's interaction (river-device.js placeDialog): the shards as small cards, each with its own slot picker; the
// FM-1's sixteen slots four by four; a card dragged onto a slot; Back where they came from; a line per shard. Now in
// v2's language, read by itself on opening, and every state of the cable told by the same drawing.
const behindPriv = () => `${privTitle("SHARDS", [28, 4, 1])}<div class="list">${listHead()}${MINE.slice(0, 9).map((s) => row(s)).join("")}</div>`;
const behindComm = () => `${commTitle()}${toolbar({ n: "142 SHARDS", pub: true })}<div class="list pub">${listHead({ pub: true, sort: "SHARED" })}${COMM.slice(0, 9).map((s) => row(s, { pub: true })).join("")}</div>`;
const W = (t) => `<b style="font:700 13.5px var(--f-ui);letter-spacing:.15em;color:var(--white)">${t}</b>`;
const K = (t) => `<span class="k">${t}</span>`;
const sayBox = (h, p, kind = "") => `<div class="dlg-say">${h ? `<h3${kind ? ` class="${kind}"` : ""}>${h}</h3>` : ""}${p}</div>`;
const sendPage = (file, title, d, o = {}) => put(file, page({ title, cur: o.cur || "yours", who: o.who || ME, body: o.behind || behindPriv(), over: d }));
const stage = (ill, say) => `<div class="dlg-stage">${ill}${say}</div>`;

// ---- the cable's states: the drawing, one status, one sentence
sendPage("10-send-reading", "Send: reading the FM-1", dlg({ title: "SEND TO FM-1", sub: "3 SHARDS",
  body: stage(illus("reading"), sayBox("READING THE FM-1", `<div class="wl flow" style="width:260px;margin:4px 0 2px"><i></i></div><p class="small">To see what each of its sixteen slots holds now.</p>`)),
  foot: `<p></p><div class="bs"><span class="b">Cancel</span></div>` }));
sendPage("11-send-not-connected", "Send: not connected", dlg({ title: "SEND TO FM-1", sub: "3 SHARDS",
  body: stage(illus("off"), sayBox("NOT CONNECTED", `<p>Plug the FM-1 into this computer with a USB-C data cable.</p><p class="small">Another app holding the FM-1's MIDI can hide it: close that app.</p>`, "warn")),
  foot: `<p></p><div class="bs"><span class="b b-ice">Try again</span></div>` }));
sendPage("12-send-no-frasil", "Send: FRASIL not installed", dlg({ title: "SEND TO FM-1", sub: "3 SHARDS",
  body: stage(illus("nofrasil"), sayBox("FRASIL NOT INSTALLED", `<p>The FM-1 answers, but it runs another firmware, or a FRASIL older than 1.1.</p>`, "warn")),
  foot: `<p></p><div class="bs"><span class="b">${I.ext()} Install FRASIL</span><span class="b b-ice">Try again</span></div>` }));

// ---- ready: the cards, the slots
const SLOTS = [["MIST", "factory"], ["GLASS", "factory"], ["ABYSS", "saved"], ["CHORALE", "factory"], ["WIND", "factory"], ["BELLS", "factory"], ["TIDE", "factory"], ["AURORA", "factory"],
  ["CATHEDRAL", "saved"], ["GLACIER", "saved"], ["AURORA", "saved"], ["", "empty"], ["", "empty"], ["", "empty"], ["", "empty"], ["", "empty"]];
const SL = (i) => String(i + 1).padStart(2, "0");
const what = (i) => SLOTS[i][1] === "empty" ? "EMPTY" : SLOTS[i][0];
/* a shard's card, what you drag: its slot of origin (from), its slot (to), lifted while it is dragged */
const card = (name, o = {}) => `<div class="cd${o.to != null ? " placed" : ""}${o.lifted ? " lifted" : ""}">
  <div class="cd-top"><span class="grip" aria-hidden="true"></span>${glyph(name, 22)}<span class="cd-n">${name}</span>${o.from != null ? `<span class="cd-from">FROM ${SL(o.from)}</span>` : ""}</div>
  <div class="pick${o.to != null ? " set" : ""}${o.open ? " open" : ""}"><span>${o.to != null ? `SLOT ${SL(o.to)} <i>· ${what(o.to)}</i>` : "CHOOSE A SLOT"}</span>${I.dn()}</div></div>`;
/* what the card shows under the hand: the shard, small, and the pointer */
const ghost = (name) => `<div class="ghost" aria-hidden="true">${glyph(name, 20)}<b>${name}</b></div>
  <svg class="cursor" width="19" height="23" viewBox="0 0 19 23" aria-hidden="true"><path d="M1.5 1.5v17.2l4.3-4.1 2.9 6.6 3.1-1.4-2.9-6.5h6z" fill="#f3f6f9" stroke="#06080c" stroke-width="1.3" stroke-linejoin="round"/></svg>`;
/* the FM-1's sixteen slots, four by four: places {index: shard}; o.over the slot under the card being dragged */
const tiles = (places = {}, o = {}) => `<div class="tiles" role="list">${SLOTS.map(([n, st], i) => {
  const t = places[i];
  if (t) return `<div class="tile placed" role="listitem"><span class="t-top"><span class="t-no">${SL(i)}</span>${glyph(t, 17)}</span><span class="t-n">${t}</span><span class="t-was">${st === "empty" ? "INTO AN EMPTY SLOT" : `REPLACES ${n}`}</span></div>`;
  const over = o.over === i;
  return `<div class="tile ${st}${over ? " over" : ""}" role="listitem"><span class="t-top"><span class="t-no">${SL(i)}</span><span class="t-st">${st === "factory" ? "FACTORY" : st === "saved" ? "SAVED" : ""}</span></span><span class="t-n">${st === "empty" ? "EMPTY" : n}</span>${over ? `<span class="t-was">${st === "empty" ? "INTO AN EMPTY SLOT" : `REPLACES ${n}`}</span>${ghost(o.ghost)}` : ""}</div>`;
}).join("")}</div>`;
/* where the FM-1 stands: read by itself on opening; a quiet way to read it again */
const fmCap = `<div class="p-cap"><span class="p-cap-t">THE FM-1 <i>16 SLOTS</i></span><span class="devl">FRASIL 1.1 <i>· read just now</i></span><span class="lnk">READ AGAIN</span></div>`;
const sumLine = (name, txt) => `<li>${glyph(name, 16)}<span>${K(name)} ${txt}</span></li>`;
const place = ({ cards, n, places, over, ghostName, back, sum }) => `<p class="p-lead"><span class="on-desk">${n === 1 ? "Drag the shard onto a slot, or choose its slot." : "Drag each shard onto a slot, or choose its slot."}</span><span class="on-phone">${n === 1 ? "Choose its slot on the FM-1." : "Choose a slot for each shard."}</span></p>
  <div class="place2"><div class="p-sh"><div class="p-cap"><span class="p-cap-t">THE SHARDS <i>${n}</i></span></div><div class="cds">${cards}</div>${back ? `<p class="p-back"><span class="lnk">BACK WHERE THEY CAME FROM</span></p>` : ""}</div>
  <div class="p-fm">${fmCap}${tiles(places, { over, ghost: ghostName })}</div></div>
  <ul class="p-sum">${sum}</ul>`;
const stays = `<p class="p-stay">The other slots and the settings stay as they are.</p>`;

// one shard, from the Community River: placed in an empty slot
sendPage("13-send-ready-1", "Send: ready, one shard", dlg({ title: "SEND TO FM-1", sub: "1 SHARD", cls: "place-d",
  body: place({ n: 1, cards: card("TADOUSSAC 4AM", { to: 13 }), places: { 13: "TADOUSSAC 4AM" },
    sum: sumLine("TADOUSSAC 4AM", "goes into slot 14, empty now.") }),
  foot: `${stays}<div class="bs"><span class="b">Cancel</span><span class="b b-ice">${I.send(15)} Send to FM-1</span></div>` }),
  { cur: "community", who: "Mira K.", behind: behindComm() });

// three of one's own: two placed, the third dragged over slot 13
const three = (all) => place({ n: 3, back: true,
  cards: card("SLOW THAW", { from: 1, to: 2 }) + card("POLAR NIGHT", { from: 11, to: 11 }) + card("NORTHERN LIGHT", all ? { to: 12 } : { lifted: true }),
  places: all ? { 2: "SLOW THAW", 11: "POLAR NIGHT", 12: "NORTHERN LIGHT" } : { 2: "SLOW THAW", 11: "POLAR NIGHT" }, over: all ? -1 : 12, ghostName: "NORTHERN LIGHT",
  sum: sumLine("SLOW THAW", `goes into slot 03, in place of ${K("ABYSS")}: it is in your River.`) + sumLine("POLAR NIGHT", "goes into slot 12, empty now: back where it came from.") +
    (all ? sumLine("NORTHERN LIGHT", "goes into slot 13, empty now.") : `<li class="sum-left"><span>${K("NORTHERN LIGHT")} is not placed yet: it stays here.</span></li>`) });
sendPage("14-send-ready-3", "Send: ready, three shards (one being dragged)", dlg({ title: "SEND TO FM-1", sub: "3 SHARDS", cls: "place-d", body: three(false),
  foot: `${stays}<div class="bs"><span class="b">Cancel</span><span class="b b-ice">${I.send(15)} Send 2 to FM-1</span></div>` }));

// nothing selected yet: choose first, among one's own and one's favorites
{
  const sel = [1, 3, 6];
  const picks = MINE.slice(0, 8).map((s, i) => `<div class="sr${sel.includes(i) ? " is-sel" : ""}" style="--cols:28px minmax(0,1fr) 210px 46px 46px 54px;height:56px"><span class="ck${sel.includes(i) ? " on" : ""}"></span><span class="nm">${glyph(s.name, 22)}<span class="nm-t"><span class="nm-n" style="font-size:13.5px">${s.name}</span>${s.desc ? `<span class="nm-d" style="font-size:13px">${s.desc}</span>` : ""}</span></span>${s.msHtml || ms(s.sp)}${s.glint === "OFF" ? '<span class="gt off">—</span>' : `<span class="gt">${s.glint}</span>`}${s.bseq ? '<span class="bq">SEQ</span>' : "<span></span>"}<span class="dt">${s.date}</span></div>`).join("");
  sendPage("15-send-choose", "Send: choosing shards (nothing selected)", dlg({ title: "SEND TO FM-1", sub: "CHOOSE UP TO 16", cls: "wide",
    body: `<div class="ch-bar"><div class="tb-q">${I.q()}<span>Find a shard: a name, a word</span></div><div class="ch-tabs"><span class="on">SHARDS <i>28</i></span><span>FAVORITES <i>4</i></span></div></div><div class="list" style="margin:0"><div class="lh" style="--cols:28px minmax(0,1fr) 210px 46px 46px 54px"><span></span><span>NAME</span><span>WHAT PLAYS</span><span>GLINT</span><span>BASS</span><span class="r is-sort">ADDED ↓</span></div>${picks}</div>`,
    foot: `<p>${W("3")} chosen <span style="color:var(--dim)">· up to 16 at once, one per slot</span></p><div class="bs"><span class="b">Cancel</span><span class="b b-ice">Choose their slots</span></div>` }));
}

// the confirmation, before anything goes
{
  const d = dlg({ title: "SEND TO FM-1", sub: "3 SHARDS", cls: "place-d", body: three(true),
    foot: `${stays}<div class="bs"><span class="b">Cancel</span><span class="b b-ice">${I.send(15)} Send 3 to FM-1</span></div>` }) +
    `<div class="scrim2"></div><div class="confirm" role="alertdialog"><div class="confirm-in"><h3>SEND 3 SHARDS<span>NOW?</span></h3><ul>
      <li class="warm"><span>What plays on the FM-1 stops.</span></li>
      <li><span>Slots <b>03</b>, <b>12</b> and <b>13</b> are replaced: ${K("SLOW THAW")} in place of ${K("ABYSS")}, which is in your River; ${K("POLAR NIGHT")} and ${K("NORTHERN LIGHT")} into empty slots.</span></li>
      <li><span>The other slots and the settings stay.</span></li>
      <li class="then"><span>Then the FM-1 asks once: answer ${K("YES")} with SEL.</span></li></ul></div>
      <div class="confirm-f"><span class="b">Cancel</span><span class="b b-ice">${I.send(15)} Send</span></div></div>`;
  sendPage("16-send-confirm", "Send: the confirmation", d);
}

// ---- sending, then the FM-1's own question and its answer, drawn as the device draws them
sendPage("17-send-sending", "Send: sending", dlg({ title: "SEND TO FM-1", sub: "3 SHARDS",
  body: stage(illus("sending"), sayBox("SENDING · 64%", `<div class="wl" style="width:260px;margin:4px 0 2px"><i style="width:64%"></i></div><p class="small">Keep the cable in until the FM-1 asks.</p>`)) }));
sendPage("18-send-press-sel", "Send: the FM-1 asks", dlg({ title: "SEND TO FM-1", sub: "3 SHARDS",
  body: stage(illusClose("ask", { hand: "SEL" }), sayBox("PRESS SEL ON THE FM-1", `<p>Sent and checked. ${K("YES")} with SEL puts the 3 shards in; ${K("NO")} with FX leaves the FM-1 as it was.</p><p class="small">Its other scenes and its settings go back exactly as they are. It waits a minute for an answer.</p>`, "warn")) }));
sendPage("19-send-done", "Send: done", dlg({ title: "SEND TO FM-1", sub: "3 SHARDS",
  body: stage(illusClose("restored"), sayBox("DONE", `<p>${K("SLOW THAW")} in 03, ${K("POLAR NIGHT")} in 12, ${K("NORTHERN LIGHT")} in 13. The other slots and the settings are as they were.</p><p class="small">SEL closes the FM-1's message.</p>`, "ice")),
  foot: `<p></p><div class="bs"><span class="b b-ice">Close</span></div>` }));
sendPage("19b-send-failed", "Send: not sent", dlg({ title: "SEND TO FM-1", sub: "3 SHARDS",
  body: stage(illusClose("notrestored"), sayBox("NOT SENT", `<p>FX was pressed on the FM-1, or nobody answered within a minute. Nothing changed on it.</p><p class="small">SEL closes the FM-1's message.</p>`, "warn")),
  foot: `<p></p><div class="bs"><span class="b">Close</span><span class="b b-ice">Send again</span></div>` }));
sendPage("19c-send-held", "Send: something still held (variant)", dlg({ title: "SEND TO FM-1", sub: "3 SHARDS",
  body: stage(illusClose("stopfirst"), sayBox("SOMETHING IS STILL HELD", `<p>The River let go of all it could. A key is still down on the FM-1, or a note held by another MIDI device. Let go of it, then send again.</p><p class="small">Nothing changed. SEL closes the FM-1's message.</p>`, "warn")),
  foot: `<p></p><div class="bs"><span class="b">Close</span><span class="b b-ice">Send again</span></div>` }));

// (Get from FM-1's: the FM-1 read, the River's words over its slots)
const devLine = `<p class="devl">FM-1 · FRASIL 1.1 <i>· read just now</i></p>`;
const lead = (txt) => `<div style="display:flex;justify-content:space-between;align-items:center;gap:20px;margin-bottom:16px;flex-wrap:wrap"><p style="font:400 18px/1.4 var(--f-text);color:var(--text-2)">${txt}</p>${devLine}</div>`;

// ================================================================ 8. Get from FM-1
{
  const IN_RIVER = ["ABYSS", "CATHEDRAL"];
  const picked = [9, 10];
  const col = (from, to) => SLOTS.slice(from, to).map(([n, st], k) => {
    const i = from + k, no = String(i + 1).padStart(2, "0"), had = st === "saved" && IN_RIVER.includes(n), on = picked.includes(i);
    const tag = st === "empty" ? "" : st === "factory" ? "FACTORY" : had ? "IN YOUR RIVER" : "NEW";
    return `<div class="pk ${st}${had ? " had" : ""}${on ? " on" : ""}">${st === "empty" || had ? '<span></span>' : `<span class="ck${on ? " on" : ""}"></span>`}<span class="no">${no}</span><span class="sn">${st === "empty" ? "EMPTY" : n}</span><span class="tag${tag === "NEW" ? " ice" : ""}">${tag}</span></div>`;
  }).join("");
  put("20-get-ready", page({ title: "Get from FM-1: choose", cur: "yours", who: ME, body: behindPriv(), over: dlg({ title: "GET FROM FM-1",
    body: `${lead("Choose the scenes to keep as shards.")}
    <div class="sls" style="column-gap:24px"><div>${col(0, 8)}</div><div>${col(8, 16)}</div></div>
    <div class="opt"><span class="ck on"></span><div style="flex:1"><b>ALSO STORE THE WHOLE BACKUP</b><p>All sixteen slots and the settings, to restore later from any computer.</p>
      <div class="inp" style="margin-top:10px;max-width:420px"><span class="ph">A note: before the show, the new drones…</span></div></div></div>`,
    foot: `<p>${W("GLACIER")} and ${W("AURORA")}, and the backup.</p><div class="bs"><span class="b">Cancel</span><span class="b b-ice">${I.get(15)} Save 2 shards<span class="long">&nbsp;and the backup</span></span></div>` }) }));
  put("21-get-done", page({ title: "Get from FM-1: done", cur: "yours", who: ME, body: behindPriv(), over: dlg({ title: "GET FROM FM-1", cls: "sm",
    body: `<div class="dlg-stage" style="padding-top:10px">${glyph("GLACIER", 46)}${sayBox("SAVED", `<p>${W("GLACIER")} and ${W("AURORA")} are in your shards, and the backup of 20:43 is stored.</p>`, "ice")}</div>`,
    foot: `<p></p><div class="bs"><span class="b">Close</span><span class="b b-ice">See them</span></div>` }) }));
}

// ================================================================ 9. Tools: Quick Backup; the profile
{
  const real16 = SLOTS.map(([n, st], i) => ({ n, st, i }));
  const D_FACTORY = (n) => D.scenes.find((x) => x.name === n && x.state === "factory") || real(n);
  const rows = real16.map(({ n, st, i }) => {
    const no = String(i + 1).padStart(2, "0");
    if (st === "empty") return `<div class="sr" style="--cols:28px 34px 36px minmax(0,1fr) 232px 172px 52px 52px;height:46px"><span class="ck" style="opacity:.25"></span><span class="dt" style="justify-self:start">${no}</span><span></span><span class="nm-n" style="color:var(--slate);font-weight:500;font-size:13px">EMPTY</span></div>`;
    const s = st === "saved" ? realShard(n) : { name: n, ...(() => { const r = D_FACTORY(n); return { key: r.key.split(" · ")[0], chord: r.key.split(" · ")[1], drone: r.drone, glint: r.glint, msHtml: r.ms }; })() };
    const r = st === "saved" ? real(n) : D_FACTORY(n), bseq = r.secs.some((x) => x.name === "BASS" && x.mid.includes(">SEQ<"));
    const sel = [2, 9].includes(i);
    return `<div class="sr${sel ? " is-sel" : ""}" style="--cols:28px 34px 36px minmax(0,1fr) 232px 172px 52px 52px"><span class="ck${sel ? " on" : ""}"></span><span class="dt" style="justify-self:start">${no}</span><span class="pl">${I.play()}</span><span class="nm">${glyph(n, 24, st)}<span class="nm-t"><span class="nm-n"${st === "factory" ? ' style="color:var(--fog)"' : ""}>${n}</span>${st === "factory" ? '<span class="nm-d" style="font-style:normal;font:600 10px/1 var(--f-ui);letter-spacing:.16em">FACTORY SCENE</span>' : ""}</span></span>${s.msHtml}<span class="kv">${s.key} <i>· ${s.chord}</i></span>${s.glint === "OFF" ? '<span class="gt off">—</span>' : `<span class="gt">${s.glint}</span>`}${bseq ? '<span class="bq">SEQ</span>' : "<span></span>"}</div>`;
  }).join("");
  const body = `${toolsTitle()}
  <div class="src"><span class="devl">FROM YOUR FM-1 <i>· 20:43 · FRASIL 1.1 · 4 saved, 7 factory, 5 empty, and the settings</i></span>
    <span class="b">${I.send(15)} Restore it all…</span><span class="b b-ice">${I.dl()} Download the backup</span></div>
  <div class="list"><div class="selbar"><b>2 SELECTED</b><span class="sep"></span><span class="b b-txt">${I.send(15)} Send to FM-1</span><span class="b b-txt">${I.dl()} Download</span><span class="b b-txt" style="margin-left:auto">Clear</span></div>${rows}
  <div class="sr" style="--cols:28px 34px minmax(0,1fr) auto;height:58px"><span></span><span></span><span class="nm"><span class="nm-t"><span class="nm-n">SETTINGS</span><span class="nm-d" style="font-style:normal">Last scene 11 AURORA · tempo 90 BPM · what comes back at power-on</span></span></span><span class="oc-x">${I.chev()}</span></div></div>
  <div class="list-end"><p>From a terminal: fm1-base's <code>fm1_backup.py</code> does the same.</p></div>`;
  put("40-tools", page({ title: "Tools: Quick Backup", cur: "tools", body }));
}
{
  const r = (l, v, n, act) => `<div class="bu" style="grid-template-columns:200px minmax(0,1fr) auto;min-height:84px"><span class="f-l" style="font-size:11px;color:var(--mist)">${l}</span><div><p class="bu-note">${v}</p>${n ? `<p style="margin-top:5px;font:400 15px/1.4 var(--f-text);color:var(--dim)">${n}</p>` : ""}</div><div class="bu-acts">${act}</div></div>`;
  const body = `<div class="pt">${tabs("ACCOUNT", [["ACCOUNT"]])}<p class="pt-sub">Since October 2026 · 100 shards, 9 shared</p></div>
  <div class="list" style="margin-top:6px">
  ${r("DISPLAY NAME", "Styves", "Public: the author's name on the shards you share.", `<span class="a">CHANGE</span>`)}
  ${r("EMAIL", "styves.goupil@gmail.com <span style='font:600 10.5px var(--f-ui);letter-spacing:.16em;color:var(--ice);margin-left:10px'>CONFIRMED</span>", "Never shown to anyone.", "")}
  ${r("PASSWORD", "••••••••••", "Changing it signs out your other sessions.", `<span class="a">CHANGE…</span>`)}
  ${r("SESSIONS", "This browser, and 2 others", "", `<span class="a">SIGN OUT</span><span class="a a-txt">SIGN OUT EVERYWHERE</span>`)}
  ${r("YOUR DATA", "Your shards, favorites and backups, in one file", "A zip that Quick Backup opens.", `<span class="a">${I.dl()} DOWNLOAD</span>`)}
  ${r("PRIVACY", "What the River keeps about you, and its terms", "", `<span class="a a-txt">READ ${I.ext()}</span>`)}
  ${r("LEAVE THE RIVER", "Delete the account, your shards and your backups", "Copies others favorited of what you shared stay theirs, without your name.", `<span class="a a-txt" style="color:#ff8a73">DELETE MY ACCOUNT…</span>`)}
  </div>`;
  put("41-profile", page({ title: "Profile", cur: "profile", who: ME, body }));
}

fs.writeFileSync(path.join(OUT, "data/pages.json"), JSON.stringify(Object.keys(pages)));
console.log(Object.keys(pages).join("\n"));
