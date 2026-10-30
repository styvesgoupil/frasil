# FRASIL River — the card laid out again, THE KEYS

Styves' asks (2026-10-25), after the editor shipped: a keyboard at the bottom of the card, the whole width, to edit the
notes, with a selector (DRONE, GLINT, BASS) and its settings beside it (HARMONY for the drone, the arpeggio's or the
sequence's for GLINT, the bass's), LATCH or not (an arpeggio built with the mouse); the bass's sequence in the middle
column under GLINT; the LFOs' pictures in their sections too; the first two columns as one, narrower, the LFOs under
the sequences, the pages at the right more airy; a balance when every section is closed. Open `index.html`.

`scripts/shoot.mjs` starts the River's service (a fresh database), gives Styves AURORA, GLACIER and NORTH RIVER, opens a
card, lays it out again with the prototype (`proto/keys.js`, `keys.css`) and plays it for real (the keys light with
what the engine plays).

    ./build.sh --host && ./build.sh --web && tools/website.sh
    node design/river-keys/scripts/shoot.mjs [40 ...]

## The design

- **Two columns.** At the left (5 of 12), what moves: GLINT (its walk or sequence), the BASS's sequence (or NO
  SEQUENCE and its note), the LFOs three abreast. At the right (7 of 12), every page; HARMONY gone down to the keys.
  Closed, the ten sections end about where the left column does.
- **An LFO's section, opened:** its picture first, live (as BASS's shows its sequence).
- **THE KEYS**, the whole width at the bottom, in the FM-1's words:
  - KEYS: DRONE, GLINT, BASS (each its colour); KEY MODE: LATCH, HOLD.
  - A keyboard C1 to C8, the scale's notes marked (the root ice), what is latched drawn (the track chosen bright, the
    others dim), what sounds lit live.
  - DRONE: a click latches the chord HARMONY makes on that key (the key under the mouse shows it in lamp); again, it
    lets go. HARMONY's four settings under the keys (CHORD on SINGLE: one note).
  - GLINT on ARP: a click adds the note to the arpeggio, again removes it; the arpeggio's settings under the keys.
  - GLINT on SEQ, BASS with SEQUENCE: the steps over the keys; a step chosen, a key writes it, then the next step;
    REST, TIE, ACCENT (GLINT), CUT, HOLD (BASS); the sequence's settings under the keys.
  - HOLD: a key plays while it is down; nothing is kept.
- **A phone:** the keys scroll sideways, opened where the notes are.

## Styves' decisions (2026-10-25)

1. The small WHAT PLAYS keyboard goes. 2. HARMONY moves to THE KEYS; GLINT's and BASS's settings stay in their pages
too. 3. The keyboard always C1 to C8. 4. Sequences written step by step (LIVE later); for GLINT, he asked for a grid
(a piano roll): mockup 45, APPROVED with the rest ("oui tout me convient"). The bass keeps its steps over the
keys (43), with AUTO NEXT STEP [ON | OFF]; no instruction sentences (memory ui-text-minimal). The build plan:
.omc/plans/frasil-river-keys.md.

## Built (2026-10-25)

In the River itself: `website/assets/js/river-keys.js` (THE KEYS), `river-cards.js` (the card laid out again, an LFO's
wide picture), `river-edit.js` (what it plays edited, through the engine: `app/web.c` web_latch, web_clear, web_hold,
web_scene_take, web_param on the sequences' steps). Beside the mockups, decided while building:
- GLINT's settings under the keys start with its type (SEQ: OFF, ARP, SEQ), BASS's with SEQUENCE (OFF, ON): the track
  can be turned on from the keys.
- GLINT's and the bass's keys play the note clicked, as a MIDI note does on the FM-1 (no scale); the drones' go
  through HARMONY, as ever.
- The bass's step written past SEQ STEPS takes it in (SEQ STEPS grows to it); GLINT's grid does the same with LENGTH.
- The legend under the keys names all GLINT's RANGE plays, over C8 too.
- HOLD with nothing playing: the keys dim and ▶ is nudged (no words).
Tests: `website/tests/river_keys.mjs`, `river_audition.mjs` (the exports, byte by byte).
