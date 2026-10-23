/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The play page: the emulator's panel gets a little depth (a gradient its drawing can't name by itself), and a
 * switch over it: the FM-1's own names on its controls, or what each one does under FRASIL. */
"use strict";
(() => {
  const svg = document.getElementById("panel");
  if (!svg) return;
  const defs = document.createElementNS("http://www.w3.org/2000/svg", "defs");
  defs.innerHTML = '<linearGradient id="fr-body" x1="0" y1="0" x2="0" y2="1">' +
    '<stop offset="0" stop-color="#1d242d"/><stop offset=".5" stop-color="#151b22"/><stop offset="1" stop-color="#0f141a"/></linearGradient>';
  svg.insertBefore(defs, svg.firstChild);
  const body = svg.querySelector('rect[fill="#3a3d42"]');
  if (body) body.classList.add("fr");

  // the labels: the FM-1's, or FRASIL's (GLINT's in ice, BASS's in lamp, as on the screen)
  const FRASIL = {
    MASTER: "VOLUME", SELECT: "KEYBOARD", PRESETS: "PRESET", ALGORITHM: "FULLNESS",
    KNOB1: "K1", KNOB2: "K2", KNOB3: "K3", KNOB4: "K4",
    FX: "EFFECTS", SEL: "HARMONY", ENV: "SHAPE", LFO: "MOTION", EDIT: "LAYERS", GLO: "GLOBAL",
    HOME: "SOUND", SAVE: "SCENES", ARP: "GLINT", SEQ: "BASS", PLAY: "START", STOP: "STOP", REC: "RECORD",
  };
  const TINT = { ARP: "#6ed0ff", SEQ: "#f2a65a" };
  const labels = [];
  for (const t of svg.querySelectorAll("text")) {
    const name = t.textContent.trim();
    if (!(name in FRASIL)) continue;
    const face = t.parentNode.querySelector("rect");          // a button's face: its label must fit inside
    labels.push({ t, name, fill: t.getAttribute("fill"), room: face ? parseFloat(face.getAttribute("width")) - 8 : 0 });
  }
  if (!labels.length) return;
  // (the site's .btn sets the panel's labels at 14 px, tracked: FRASIL's names on the buttons, all at the size the
  // longest fits in its face)
  const fit = Math.min(14, ...labels.filter((l) => l.room).map((l) => Math.floor(l.room / (FRASIL[l.name].length * 0.6))));
  function show(mode) {
    for (const l of labels) {
      const fr = mode === "frasil";
      l.t.textContent = fr ? FRASIL[l.name] : l.name;
      l.t.setAttribute("fill", fr && TINT[l.name] ? TINT[l.name] : l.fill);
      l.t.style.fontSize = fr && l.room ? fit + "px" : "";
      l.t.style.letterSpacing = fr && l.room ? ".06em" : "";
    }
    for (const b of sw.querySelectorAll("button")) b.setAttribute("aria-pressed", String(b.dataset.m === mode));
    try { localStorage.setItem("frasil-panel-labels", mode); } catch (e) { /* (a private window: not kept) */ }
  }
  const sw = document.createElement("div");
  sw.className = "label-switch";
  sw.setAttribute("role", "group");
  sw.setAttribute("aria-label", "Labels on the panel");
  sw.innerHTML = '<span class="ls-lab" aria-hidden="true">Labels</span>' +
    '<button type="button" data-m="fm1" aria-pressed="true">FM-1 buttons</button>' +
    '<button type="button" data-m="frasil" aria-pressed="false">FRASIL buttons</button>';
  sw.addEventListener("click", (e) => {
    const b = e.target.closest("button");
    if (b) show(b.dataset.m);
  });
  const box = document.querySelector(".devicebox") || svg.parentNode;
  box.parentNode.insertBefore(sw, box);
  let mode = "fm1";
  try { mode = localStorage.getItem("frasil-panel-labels") === "frasil" ? "frasil" : "fm1"; } catch (e) { /* fm1 */ }
  show(mode);
})();
