/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Every page: the download count, things rise into view, the guide's menu on phones, its contents following the reading, a screen enlarged. */
"use strict";
(() => {
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));

  // ------------------------------------------------------------ the download count (stats/downloads.json, same origin; hidden until it is there)
  const dl = $$("[data-downloads]"), me = document.currentScript;
  if (dl.length && me && me.src) {
    fetch(new URL("../../stats/downloads.json", me.src), { cache: "no-cache" })
      .then((r) => (r.ok ? r.json() : Promise.reject()))
      .then((d) => {
        if (!Number.isInteger(d.total) || d.total < 1) return;
        const n = d.total.toLocaleString("en-US").replace(/,/g, "\u2009") + (d.total === 1 ? " download" : " downloads");
        dl.forEach((el) => { el.textContent = n; el.hidden = false; });
      })
      .catch(() => {});
  }

  // ------------------------------------------------------------ reveals
  const rv = $$(".rv");
  if ("IntersectionObserver" in window) {
    const io = new IntersectionObserver((entries) => {
      for (const e of entries) if (e.isIntersecting) { e.target.classList.add("in"); io.unobserve(e.target); }
    }, { rootMargin: "0px 0px -8% 0px", threshold: 0.08 });
    rv.forEach((el) => io.observe(el));
  } else rv.forEach((el) => el.classList.add("in"));

  // ------------------------------------------------------------ the guide's chapters, in a menu on phones
  const side = document.querySelector(".g-side"), menu = document.querySelector(".g-menu");
  if (side && menu) {
    const set = (open) => { side.classList.toggle("open", open); menu.setAttribute("aria-expanded", String(open)); };
    menu.addEventListener("click", () => set(menu.getAttribute("aria-expanded") !== "true"));
    side.addEventListener("keydown", (e) => { if (e.key === "Escape" && side.classList.contains("open")) { set(false); menu.focus(); } });
    $$(".g-nav a", side).forEach((a) => a.addEventListener("click", () => set(false)));
  }

  // ------------------------------------------------------------ the contents: the section being read
  const toc = $$(".g-toc a");
  if (toc.length) {
    const heads = toc.map((a) => document.getElementById(decodeURIComponent(a.hash.slice(1)))).filter(Boolean);
    let queued = false;
    const mark = () => {
      queued = false;
      let cur = heads[0];
      if (!cur) return;
      for (const h of heads) if (h.getBoundingClientRect().top < window.innerHeight * 0.3) cur = h;
      toc.forEach((a) => a.classList.toggle("on", a.hash === "#" + cur.id));
    };
    addEventListener("scroll", () => { if (!queued) { queued = true; requestAnimationFrame(mark); } }, { passive: true });
    mark();
  }

  // ------------------------------------------------------------ a screen, larger
  let box = null;
  function lightbox() {
    if (box) return box;
    box = document.createElement("dialog");
    box.className = "lightbox";
    box.setAttribute("aria-label", "The screen, larger");
    box.innerHTML = '<div class="screen"><img alt="" width="240" height="240"></div>' +
      '<div class="lightbox-bar"><p></p><button class="btn lightbox-close" type="button">Close</button></div>';
    box.querySelector(".lightbox-close").addEventListener("click", () => box.close());
    box.addEventListener("click", (e) => { if (e.target === box) box.close(); });
    document.body.appendChild(box);
    return box;
  }
  // ------------------------------------------------------------ the guide's panel, full screen
  let pbox = null;
  function panelbox(svg) {
    if (!pbox) {
      pbox = document.createElement("dialog");
      pbox.className = "lightbox lightbox-panel";
      pbox.setAttribute("aria-label", "The FM-1's panel under FRASIL, full screen");
      pbox.innerHTML = '<div class="lightbox-panel-scroll"></div>' +
        '<div class="lightbox-bar"><p>Under each button, what a tap does; in amber, what it does held.</p>' +
        '<button class="btn lightbox-close" type="button" autofocus>Close</button></div>';
      pbox.querySelector(".lightbox-close").addEventListener("click", () => pbox.close());
      pbox.addEventListener("click", (e) => { if (e.target === pbox) pbox.close(); });
      const copy = svg.cloneNode(true);       // (its ids its own: the clip of the screen, the title)
      copy.querySelectorAll("[id]").forEach((n) => { n.id += "-lb"; });
      copy.querySelectorAll("[clip-path]").forEach((n) => n.setAttribute("clip-path", n.getAttribute("clip-path").replace(")", "-lb)")));
      copy.setAttribute("aria-labelledby", (copy.getAttribute("aria-labelledby") || "").split(" ").filter(Boolean).map((s) => s + "-lb").join(" "));
      pbox.querySelector(".lightbox-panel-scroll").appendChild(copy);
      document.body.appendChild(pbox);
    }
    return pbox;
  }
  document.addEventListener("click", (e) => {
    const pz = e.target.closest(".panel-zoom");
    if (pz) {
      const svg = pz.parentNode.querySelector("svg");
      const d = svg && panelbox(svg);
      if (d && typeof d.showModal === "function") d.showModal();
      return;
    }
    const z = e.target.closest(".zoom");
    if (!z) return;
    const d = lightbox();
    if (typeof d.showModal !== "function") { window.open(z.dataset.zoom, "_blank"); return; }
    const img = d.querySelector("img");
    img.src = z.dataset.zoom;
    img.alt = z.dataset.alt || "";
    d.querySelector("p").textContent = z.dataset.cap || "";
    d.showModal();
  });
})();
