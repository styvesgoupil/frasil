/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The admin's head, on all its pages: FRASIL's downloads as a flip clock, the total the main site shows
 * (/stats/downloads.json, Caddy's, behind the same basic auth; hidden until it is there). */
"use strict";
(() => {
  const box = document.querySelector("[data-dl]"), digits = box.querySelector("[data-dl-digits]");
  const half = (cls, d) => { const s = document.createElement("span"); s.className = cls; const i = document.createElement("i"); i.textContent = d; s.append(i); return s; };
  const still = (cell, d) => { cell.dataset.d = d; cell.replaceChildren(half("up", d), half("lo", d)); };
  function flip(cell, d) {
    const was = cell.dataset.d;
    if (was === d) return;
    if (matchMedia("(prefers-reduced-motion: reduce)").matches) return still(cell, d);
    cell.dataset.d = d;
    cell.replaceChildren(half("up", d), half("lo", was), half("up go", was), half("lo go", d));
    setTimeout(() => { if (cell.dataset.d === d) still(cell, d); }, 400);
  }
  function show(total) {
    const s = String(total);
    while (digits.children.length < s.length) {
      const c = document.createElement("span");
      c.className = "adm-cell";
      still(c, "0");
      digits.prepend(c);
    }
    [...digits.children].forEach((c, i) => setTimeout(() => flip(c, s[i]), i * 70));
    digits.setAttribute("aria-label", `${s} downloads`);
    box.hidden = false;
  }
  async function load() {
    try {
      const r = await fetch("/stats/downloads.json", { cache: "no-cache" });
      const d = r.ok ? await r.json() : null;
      if (Number.isInteger(d?.total) && d.total > 0) show(d.total);
    } catch { /* (no count: nothing shown) */ }
  }
  load();
  setInterval(load, 5 * 60e3);
})();
