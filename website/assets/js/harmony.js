/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * HARMONY on the FM-1's keys, in the browser: app/harmony.c, line for line. A note outside the scale falls to the
 * scale's note below it; the diatonic chords are steps up the scale from that note, the fixed ones the same
 * intervals on every note; OPEN lifts the second note an octave, WIDE drops the root an octave.
 * The keys: 27, at octave −1, F2 (MIDI 41) to G4 (67): app.c's 53 + key + 12 × octave. */
"use strict";
(() => {
  const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const bits = (...ks) => ks.reduce((b, k) => b | (1 << k), 0);
  const SCALE = [0xfff, bits(0, 2, 4, 5, 7, 9, 11), bits(0, 2, 3, 5, 7, 8, 10), bits(0, 2, 3, 5, 7, 9, 10),
    bits(0, 1, 3, 5, 7, 8, 10), bits(0, 2, 4, 6, 7, 9, 11), bits(0, 2, 4, 5, 7, 9, 10), bits(0, 1, 3, 5, 6, 8, 10),
    bits(0, 2, 3, 5, 7, 8, 11), bits(0, 2, 3, 5, 7, 9, 11), bits(0, 2, 4, 7, 9), bits(0, 3, 5, 7, 10)];
  const SCALE_NAME = ["CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "LOCRIAN", "HARMONIC",
    "MELODIC", "PENTA MAJ", "PENTA MIN"];
  const CHORD_NAME = ["SINGLE", "FIFTH", "TRIAD", "SEVENTH", "SUS2", "SUS4", "ADD9", "QUARTAL", "MAJ", "MIN", "MAJ7",
    "MIN7", "DOM7", "AUG", "DIM"];
  const SHAPE = [[0], [0, 4], [0, 2, 4], [0, 2, 4, 6], [0, 1, 4], [0, 3, 4], [0, 2, 4, 8], [0, 3, 6],
    [0, 4, 7], [0, 3, 7], [0, 4, 7, 11], [0, 3, 7, 10], [0, 4, 7, 10], [0, 4, 8], [0, 3, 6]];
  const DIATONIC = 8;
  const name = (n) => NOTE[n % 12] + (Math.floor(n / 12) - 1);
  const inScale = (n, r, b) => (b >> (((n - r) % 12 + 12) % 12)) & 1;
  function scaleUp(n, steps, r, b) { while (steps > 0) { n++; if (inScale(n, r, b)) steps--; } return n; }
  function chord(played, h) {
    const b = SCALE[h.scale];
    let base = played;
    while (!inScale(base, h.root, b)) base--;
    const out = SHAPE[h.chord].map((s) => h.chord < DIATONIC ? scaleUp(base, s, h.root, b) : base + s);
    if (h.voicing === 1 && out.length >= 3) out[1] += 12;
    else if (h.voicing === 2 && out.length >= 2) out[0] -= 12;
    return { base, notes: out.filter((n) => n >= 0 && n <= 127) };
  }

  // the panel: the lower row 16 keys, the upper row 11, placed as on the FM-1 (and its emulator)
  const LOWER = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
  const UPPER = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25], UPPER_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14];
  const FIRST = 41;

  document.querySelectorAll("[data-harmony]").forEach((root) => {
    const board = root.querySelector(".h-keys"), out = root.querySelector(".h-out");
    const sel = {};
    root.querySelectorAll("select[data-h]").forEach((s) => (sel[s.dataset.h] = s));
    const keys = [];
    const pitch = 100 / 16, kw = pitch * .8;
    const make = (k, cls, left, top, w, h) => {
      const b = document.createElement("button");
      b.type = "button";
      b.className = "h-key " + cls;
      Object.assign(b.style, { left: left + "%", top: top + "%", width: w + "%", height: h + "%" });
      b.innerHTML = `<span class="nm">${name(FIRST + k)}</span>`;
      b.dataset.k = k;
      keys[k] = b;
      return b;
    };
    UPPER.forEach((k, i) => board.appendChild(make(k, "up", (UPPER_AFTER[i] + 1) * pitch - kw * .4, 2, kw * .8, 36)));
    LOWER.forEach((k, i) => board.appendChild(make(k, "low", i * pitch + (pitch - kw) / 2, 44, kw, 54)));
    let cur = 9;                                     // D3, as the manual's example

    function h() {
      return { root: +sel.root.value, scale: +sel.scale.value, chord: +sel.chord.value, voicing: +sel.voicing.value };
    }
    function show() {
      const hh = h(), b = SCALE[hh.scale];
      const { base, notes } = chord(FIRST + cur, hh);
      const tones = new Set(notes);
      keys.forEach((el, k) => {
        const n = FIRST + k, inS = b !== 0xfff && inScale(n, hh.root, b);
        el.classList.toggle("in", !!inS);
        el.classList.toggle("tone", tones.has(n));
        el.classList.toggle("hand", k === cur);
        el.setAttribute("aria-pressed", String(k === cur));
        el.setAttribute("aria-label", name(n) + (inS ? ", in the scale" : "") + (tones.has(n) ? ", sounding" : ""));
      });
      const key = `${NOTE[hh.root]} ${SCALE_NAME[hh.scale]}`;
      const fell = base !== FIRST + cur ? ` It is outside the scale: it falls to <span class="k">${name(base)}</span>.` : "";
      const beyond = notes.some((n) => n < FIRST || n > FIRST + 26) ? " Some of it sounds beyond the keys." : "";
      out.innerHTML = `<span class="label">${name(FIRST + cur)}</span> plays <b>${[...notes].sort((a, b) => a - b).map(name).join(" · ")}</b>` +
        ` in <span class="k">${key}</span> with <span class="k">${CHORD_NAME[hh.chord]}</span>.${fell}${beyond}`;
    }
    board.addEventListener("click", (e) => {
      const b = e.target.closest(".h-key");
      if (!b) return;
      cur = +b.dataset.k;
      show();
    });
    Object.values(sel).forEach((s) => s.addEventListener("change", show));
    show();
  });
})();
