/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Every page: the header (stuck, its menu; on the landing the mark once the big logo is gone, and where the reading is),
 * the download count, things rise into view, the guide's menu on phones, its contents following the reading, a screen
 * enlarged. */
"use strict";
(() => {
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));

  // ------------------------------------------------------------ the header
  const head = document.querySelector(".site-head");
  if (head) {
    const stuck = () => head.classList.toggle("stuck", scrollY > 2);   /* (its shadow once the page moves under it) */
    addEventListener("scroll", stuck, { passive: true });
    stuck();
    const big = document.querySelector(".hero-logo");           /* the landing: the mark once the big logo has gone */
    if (head.classList.contains("over")) {
      if (big && "IntersectionObserver" in window)
        new IntersectionObserver(([e]) => head.classList.toggle("past", !e.isIntersecting),
          { rootMargin: `-${head.offsetHeight}px 0px 0px 0px` }).observe(big);
      else head.classList.add("past");
    }

  }

  // ------------------------------------------------------------ the menu (every page's, the River's too): opened by its
  // button, closed by it, ESC, a click elsewhere or a link taken
  const btn = document.querySelector(".menu-btn"), panel = document.getElementById("site-menu");
  if (btn && panel) {
    let shut = 0;
    const isOpen = () => btn.getAttribute("aria-expanded") === "true";
    const open = (on) => {
      clearTimeout(shut);
      btn.setAttribute("aria-expanded", String(on));
      if (on) { panel.hidden = false; requestAnimationFrame(() => panel.classList.add("open")); }
      else { panel.classList.remove("open"); shut = setTimeout(() => (panel.hidden = true), 250); }
    };
    btn.addEventListener("click", () => open(!isOpen()));
    const over = (e) => e.target.closest && e.target.closest("dialog[open]");   // (a dialog opened from it: it stays)
    document.addEventListener("keydown", (e) => { if (e.key === "Escape" && isOpen() && !over(e)) { open(false); btn.focus(); } });
    document.addEventListener("click", (e) => { if (isOpen() && !over(e) && !panel.contains(e.target) && !btn.contains(e.target)) open(false); });
    $$("a", panel).forEach((a) => a.addEventListener("click", () => open(false)));
  }

  // ------------------------------------------------------------ the landing's sections (main section[data-nav]),
  // floating at the bottom once the hero has gone: the one being read named, a tick each (lit, past, to come); the
  // name opens the list
  const secs = $$("main section[data-nav]");
  if (secs.length) {
    const nav = document.createElement("nav");
    nav.className = "sec-float off";
    nav.setAttribute("aria-label", "On this page");
    nav.innerHTML = '<button class="sf-cur" type="button" aria-expanded="false" aria-controls="sf-list"></button><div class="sf-ticks">' +
      secs.map((s) => `<a href="#${s.id}" title="${s.dataset.nav}" aria-label="${s.dataset.nav}"></a>`).join("") + "</div>" +
      '<ol class="sf-list" id="sf-list" hidden>' +
      secs.map((s, i) => `<li><a href="#${s.id}"><span>${String(i + 1).padStart(2, "0")}</span>${s.dataset.nav}</a></li>`).join("") + "</ol>";
    document.body.appendChild(nav);
    const cur = nav.querySelector(".sf-cur"), list = nav.querySelector(".sf-list"), ticks = $$(".sf-ticks a", nav), rows = $$("a", list);
    const listOpen = (on) => { list.hidden = !on; cur.setAttribute("aria-expanded", String(on)); };
    cur.addEventListener("click", () => listOpen(list.hidden));
    rows.forEach((a) => a.addEventListener("click", () => listOpen(false)));
    document.addEventListener("click", (e) => { if (!list.hidden && !nav.contains(e.target)) listOpen(false); });
    document.addEventListener("keydown", (e) => { if (e.key === "Escape" && !list.hidden) { listOpen(false); cur.focus(); } });
    let queued = false, was = -2;
    const mark = () => {
      queued = false;
      let at = -1;
      secs.forEach((s, i) => { if (s.getBoundingClientRect().top < innerHeight * 0.4) at = i; });
      const foot = document.querySelector(".site-foot");
      nav.classList.toggle("off", at < 0 || (foot && foot.getBoundingClientRect().top < innerHeight - 40));
      if (at === was || at < 0) return;
      was = at;
      cur.innerHTML = `${secs[at].dataset.nav}<span>${at + 1}/${secs.length}</span>`;
      ticks.forEach((a, i) => { a.classList.toggle("on", i === at); a.classList.toggle("past", i < at); });
      rows.forEach((a, i) => a.classList.toggle("on", i === at));
    };
    addEventListener("scroll", () => { if (!queued) { queued = true; requestAnimationFrame(mark); } }, { passive: true });
    mark();
  }

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

  // ------------------------------------------------------------ what is to come (FRASIL for iPad, the VST): its
  // waitlist, an email left to be told when it is out (POST /api/waitlist {list, email}: server/waitlist.py, on this
  // origin: the main site's sent to the River's service). No email sent, none verified
  let wl = null, wlFrom = null;
  function waitlist() {
    if (wl) return wl;
    wl = document.createElement("dialog");
    wl.className = "wl";
    wl.setAttribute("aria-labelledby", "wl-t");
    wl.setAttribute("aria-describedby", "wl-p");
    wl.innerHTML = '<div class="wl-in"><div class="wl-h"><span class="wl-ico"></span><h2 class="wl-t" id="wl-t" tabindex="-1"></h2>' +
      '<button class="wl-x" type="button" aria-label="Close"><svg width="16" height="16" viewBox="0 0 16 16" aria-hidden="true">' +
      '<path d="M3 3l10 10M13 3L3 13" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></svg></button></div>' +
      '<p class="wl-p" id="wl-p">Coming soon. Leave your email: we’ll tell you when it’s out.</p>' +
      '<form class="wl-f" novalidate><label class="wl-l" for="wl-e">Email</label><div class="wl-row">' +
      '<input id="wl-e" name="email" type="email" inputmode="email" autocomplete="email" autocapitalize="off" spellcheck="false" ' +
      'maxlength="254" required placeholder="you@example.com" aria-describedby="wl-say">' +
      '<button class="btn btn-ice" type="submit">Notify me</button></div><p class="wl-say" id="wl-say" role="alert"></p></form>' +
      '<p class="wl-done" tabindex="-1" role="status" hidden>Thanks. We’ll let you know.</p></div>';
    const form = wl.querySelector("form"), input = form.querySelector("input"), go = form.querySelector("button"),
      say = form.querySelector(".wl-say"), done = wl.querySelector(".wl-done");
    const tell = (t) => { say.textContent = t; input.setAttribute("aria-invalid", String(!!t)); };
    wl.querySelector(".wl-x").addEventListener("click", () => wl.close());
    wl.addEventListener("click", (e) => { if (e.target === wl) wl.close(); });   // (outside its box: the backdrop)
    wl.addEventListener("close", () => { if (wlFrom && wlFrom.isConnected) wlFrom.focus(); });
    input.addEventListener("input", () => { if (say.textContent) tell(""); });
    form.addEventListener("submit", async (e) => {
      e.preventDefault();
      const email = input.value.trim();
      if (!email || !input.checkValidity() || !/@[^@.\s]+\.[^@\s]+$/.test(email)) { tell("Enter a valid email."); input.focus(); return; }
      go.disabled = true;
      tell("");
      try {
        const r = await fetch("/api/waitlist", { method: "POST", headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ list: wl.dataset.list, email }) });
        if (!r.ok) {
          const d = await r.json().catch(() => ({}));
          throw new Error(r.status === 429 ? "Too many tries. Wait a little." :
            r.status === 422 ? "Enter a valid email." : (typeof d.detail === "string" && d.detail.length < 90 && d.detail) || "That didn’t work. Try again.");
        }
        form.hidden = true;
        wl.querySelector(".wl-p").hidden = true;
        done.hidden = false;
        done.focus();
      } catch (err) {
        tell(err instanceof TypeError ? "No connection. Try again." : err.message);
        input.focus();
      } finally { go.disabled = false; }
    });
    document.body.appendChild(wl);
    return wl;
  }
  document.addEventListener("click", (e) => {
    const b = e.target.closest("[data-waitlist]");
    if (!b) return;
    const d = waitlist();
    if (typeof d.showModal !== "function") return;
    wlFrom = b;
    d.dataset.list = b.dataset.waitlist;
    d.querySelector(".wl-t").textContent = b.dataset.title || "Coming soon";
    const ico = b.querySelector(".way-ico"), slot = d.querySelector(".wl-ico");
    slot.replaceChildren(...(ico ? [ico.cloneNode(true)] : []));
    const form = d.querySelector("form"), input = form.querySelector("input");
    form.hidden = false;                          // (a new one each time: the last thanks forgotten, its email kept)
    d.querySelector(".wl-p").hidden = false;
    d.querySelector(".wl-done").hidden = true;
    form.querySelector(".wl-say").textContent = "";
    input.removeAttribute("aria-invalid");
    if (!d.open) d.showModal();
    input.focus();
  });

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
