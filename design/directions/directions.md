# FRASIL — three screen directions (240×240 RGB565)

All mockups are drawn by `mock.py`, a stand-in for fm1-base's `gfx.c`:
- colours are quantised to RGB565;
- text uses `gen_font.py`'s real glyphs (Barlow Semi Condensed with tabular figures, 4-bit alpha), blended exactly the way `cv_text` blends;
- lines are `cv_line`'s Bresenham;
- the extra helpers (per-pixel blend, anti-aliased rings, discs, dither) are integer operations an app can add over `cv_px`, which the unity build makes visible to `app.c`.

Content is the same on every screen:
- four latched voices, D2, A2, E3 and B3; E3 is under 85 % pressure, B3 is still swelling;
- the three macros, TIMBRE 58, MOTION 35 and SPACE 64;
- the reverb's CLOUD page: SIZE 72, DECAY 12.4 S, TONE 45 and SHIMMER 30;
- octave -1.

The name is **FRASIL**: the granular ice crystals that form in the moving water of the St. Lawrence. It is one constant (`NAME`) in each direction, and "Frasil" in B's serif title.

**All on-screen text is English**, per the user. Macros are TIMBRE, MOTION and SPACE. The edit pages show CLOUD with SIZE, DECAY, TONE and SHIMMER; the next page is CLOUD II. Freeze is FREEZE, the time axis reads NOW, the overlay says OF 100, and B's header shows the scene, e.g. "03 GLASS". The name's meaning reshaped A; B and C only carry the name.

Files are named `A-*.png`, `B-*.png` and `C-*.png`. Each direction has a play, edit, freeze and overlay screen and a 3× sheet (`*-sheet.png`). B has three more: `B-play-nuit.png`, `B-play-serif.png` and `B-edit-serif.png`. `compare.png` holds play, edit and a third screen for all three at 2×: freeze for A and C, the night theme for B (B's row uses the serif titles).

---

## A — RIVER (frasil)
**The river at night, carrying frasil.**
- **Pitch** is altitude above the waterline.
- **A voice** is a current: a thin line carrying ice crystals in suspension. A louder voice means a longer current and more crystals.
- **Pressure** warms the crystals to lamp light: pressing warms the ice.
- **FREEZE** sets them: the crystals lock into a lattice and the current stops.
- **The reverb** is the haze rising off the water.

The name fits most closely here: frasil is ice that is already crystalline but still moving, and FREEZE is the moment it sets.

**Palette**

| Role | Hex | RGB565 |
|---|---|---|
| Night (background) | #0A0D12 | 0x0862 |
| Steel (tracks) | #26303C | 0x2187 |
| Slate (waterline) | #4A5563 | 0x4AAC |
| Mist (labels) | #8B95A4 | 0x8CB4 |
| Fog (values, crystals) | #DCE3EA | 0xDF1D |
| Haze | #33414F | 0x3209 |
| **Lamp — the one accent** | **#F2A65A** | **0xF52B** |

**Type**
- XS labels tracked by 1 px, small caps feel. The name is XS tracked by 3, so it stays quiet.
- Values use M.
- The overlay value uses L.

**Grid**
- Margins are 14 px. The header sits at y 8.
- The field runs from y 30 to the waterline at y 176.
- The macros run from y 192 to 230. TIMBRE, MOTION and SPACE are aligned left, centre and right, matching the three encoders on the left of the panel.

**Voices**
- Pitch is fixed, not auto-scaled: C1 sits on the waterline and the scale is about 2.4 px a semitone.
- Each current is a faint halo of water plus an anti-aliased core line, deliberately quieter than the ice it carries.
- The crystals are frasil's own shapes:
  - grains: single pixels;
  - needles: 3 px streaks lying along the flow;
  - discs: small pluses, about 8 % of crystals, catching the light.
- Crystals cluster around the current with a gaussian spread. The number is 18 + 56 × level, plus 30 × pressure.
- MOTION shows as the current's wobble.
- Note names sit in the left column; the pressed note's name turns lamp-coloured.

**Edit page**
- The scene shrinks to a dimmed strip at the top, with fewer crystals.
- Below it, four values sit in a 2×2 grid. Each has a hairline gauge with a fog-white tip.

**FREEZE**
- The crystals snap to a 6 px lattice in three rows (on the current and 3 px either side), drawn as crosses joined by hairlines.
- The currents go straight and solid, and the waterline turns white.
- A fog-white tag with "FREEZE" in it appears top right.
- The change from drifting grains to lattice is the instrument's freeze made visible.

**Overlay**
- The field dims, with fewer crystals.
- The label, the L value and a gauge appear in the empty sky above the water.
- The matching footer column stays lit; the other two dim.

**Motion**
- Crystals drift downstream (left to right) at a speed set by MOTION. Each has a slight vertical jitter; new ones appear upstream and fade out downstream.
- Currents wobble slowly.
- On note-on a current spreads outward from the centre over the attack, and its crystals thicken.
- On FREEZE the crystals ease to their lattice positions over about 300 ms, then hold.
- The haze rises and falls with SPACE and DECAY.

**Cost and risks**
- Blends must round: `(c·a + bg·(amax−a) + amax/2) / amax`. Truncating, as `cv_text` does, turns faint greys green in RGB565, because green has 6 bits and keeps a level where red and blue fall to 0. This applies to all three directions.
- The halos plus haze are about 15 k blends. Crystals are at most about 4 voices × 100 × 1–5 blends each, about 1.5 k. Together that is roughly 0.4 M instructions a frame, so 20–30 fps is easy.
- The crystal state is about 8 voices × 100 × 4 bytes, around 3 KB of RAM, or the crystals can be derived each frame from a seed plus time.
- The 240×144 canvas blit takes about 12 ms over SPI, so animate only the field band. The header and footer redraw only when a value changes.
- Risk: too many crystals look like noise. The density curve and the 8 % share of discs were tuned to stay calm.
- Risk: the plain non-anti-aliased `cv_line` would look harsh for the currents, so they need a 2-row Wu blend, which is trivial.

## B — LOGBOOK
**A field notebook that plots the drone as it is played.**
- **Time** runs right to left. The right edge is now; the left edge is 40 s ago.
- **A voice** is an ink trace at its pitch. Its thickness follows the level, and it wavers as it drifts.
- **Pressure** is red pencil drawn over the ink.
- **The reverb** is the ink spreading into wet paper: a cool wash around each trace, wider as SPACE rises.

**Palette**

| Role | Hex | RGB565 |
|---|---|---|
| Paper | #ECE6DA | 0xEF3B |
| Ink | #1F1E1C | 0x18E3 |
| Graphite (labels) | #857F73 | 0x83EE |
| Rule (hairlines) | #CEC5B5 | 0xCE36 |
| Wash | #9DA7B0 | 0x9D36 |
| **Red pencil — the one accent** | **#CF3F28** | **0xC9E5** |

The night variant (`B-play-nuit`) uses paper #15140F, ink #E9E2D2, rule #3A372F and wash #56636E. The red stays the same.

**Type**
- B is used for the name and titles. The header carries an editorial ink rule underneath.
- The macros are big L numerals with XS captions under them, like a museum label, separated by vertical hairlines.
- The edit page reads as a spec sheet: S labels, leader dots, right-aligned M values, and a 2 px ink scale under each row. The last value touched is drawn in red.
- Proposed extra face: **Instrument Serif Italic** (SIL OFL 1.1, at `fonts-dl/`). It is used only for the instrument's name and page titles, at 25 px. It gives B its notebook voice; Barlow keeps every number. The cost is roughly 9 KB of flash for 32..126 at 25 px with 4-bit alpha. See `B-play-serif.png` and `B-edit-serif.png`, which are rendered honestly through the same 4-bit pipeline.

**Grid**
- Margins are 14 px. The header rule is at y 27.
- The plot spans x 40–202 and y 36–166. Dotted octave rules sit at C2, C3 and C4, named in the right margin.
- The time axis is at y 170. The macros run from y 186 to 236.

**Voices**
- Note names sit in the left margin; the pressed note is in red bold.
- A pen dot sits at "now" on each trace.
- B3 is visibly young: a short thin trace at the right.

**FREEZE**
- A red double-frame stamp reading "FREEZE" appears.
- Each trace ends in an ink bar at now, and the wash dries into hatching.

**Overlay**
- A paper slip with an ink border, a shadow, the SPACE label, the L value "64", "OF 100" and a red scale bar.
- The other two footer values turn to rule colour.

**Motion**
- The plot scrolls left about 4 px a second. The pen dots breathe a little with the level.
- Red pencil appears in real time as you press.
- The display is a history of the performance: you see your swells.

**Cost and risks**
- Scrolling needs a history buffer: per voice, per column, the level, pressure and pitch wobble. That is 162 columns × 8 voices × 3 bytes, about 4 KB of RAM. The plot redraws 162×130 px every time it scrolls, at 4–10 fps; the wash is about 15 k blends.
- Risk: a light screen at night is bright, which is why the dark "nuit" theme is offered; it could be a GLO setting.
- Risk: RGB565 paper tones are fine, but the wash must stay subtle to avoid banding.
- Risk: the history is new state to maintain. If the octave changes, a trace's pitch changes and the line should jump honestly.

## C — ORBITS
**An orrery of the drone.**
- **A voice** is an orbit whose radius is the pitch. Low notes sit near the centre.
- **A body** travels each orbit as the voice drifts, trailing a comet tail as long as MOTION.
- **Pressure** lights the orbit violet with a glow.
- **The reverb** is the halo around the system: a dotted ring whose density follows SPACE, plus a faint glow.
- **Readouts** sit in the four corners around the circle.

**Palette**

| Role | Hex | RGB565 |
|---|---|---|
| Space (background) | #07080C | 0x0041 |
| Track (empty arcs) | #1C212B | 0x1905 |
| Dust (labels) | #5D6676 | 0x5B2E |
| Star (values, orbits) | #E8EBF1 | 0xEF5E |
| Halo | #2A2F4A | 0x2969 |
| **Violet — the one accent** | **#B3A4FF** | **0xB53F** |

**Type**
- The corners hold XS labels tracked by 1 and M values. Top corners put the label above the value; bottom corners put the value above the label.
- The name sits top centre in XS tracked by 3, the octave bottom centre.
- Note names are T (10 px). Each sits in a gap its orbit leaves open at 12 o'clock, like a contour-map label.

**Grid**
- The centre is (120, 120). Orbit radius is 16 + 2 px a semitone above G1, capped at 80. The halo is at r 88–91.
- The corner blocks are about 60×40 px and stay outside the circle.

**Voices**
- B3 is still drawing its orbit: a partial arc whose length follows the level.
- Bodies grow with level; the pressed body is bigger and violet.

**Edit page**
- The system dims to 16 %.
- The four parameters are the four quarters of one ring, each filling clockwise. The last one touched is violet.
- The values sit in the matching corners, with "CLOUD 1/2" in the centre.

**FREEZE**
- Bodies become fixed stars (small crosses) and the tails vanish.
- The halo's dots turn white, and "FREEZE" replaces the name.

**Overlay**
- An outer ring at r 96 fills to 64 %.
- The label and L value sit in the centre.
- The SPACE corner lights violet; the others dim.

**Motion**
- Bodies orbit slowly. The period is about 20–90 s and follows the pitch, so low notes move slowest. MOTION sets tail length and adds a little eccentric wobble.
- The tide (TIDE) could rotate the whole system slowly.
- On note-on, the orbit draws itself around over the attack.

**Cost and risks**
- Rings are the costly part. Use anti-aliased Wu circles: each ring is about 2πr points, so 4 rings plus the halo come to around 2 k blends, and the tails 300–800 more. That is far cheaper than A or B, and 30 fps is fine.
- The circle spans canvas strips (144 px maximum), so draw the field in two bands with `cv_band` and the same geometry. That is about 24 ms of blit at full refresh, so update at 15–20 fps or only the changed band.
- Risk: when two voices are a semitone apart, their orbits sit 2 px apart and blur together. Labels would collide; stagger them left and right of 12 o'clock.
- Risk: more than 6 voices crowd the system.
