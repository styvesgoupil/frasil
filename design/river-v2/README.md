# FRASIL River v2 — mockups

The River rethought from Styves' brief (`.omc/plans/frasil-river-ux-v2.md`). These are static mockups at real
resolution: the site's own tokens and fonts, and real FRASIL content. The keyboards, GLINT's strips, the bass's
sequence and every page's settings were drawn by the River's own `river-cards.js` from the example backup. The FM-1's
screens in the Send dialog are the device's own, shot by the simulator. Nothing under `website/`, `server/`, `app/` or
`base/` was changed.

- **`audit.md`**: what is wrong today, each point with its screenshot (`audit/`).
- **`png/00-overview.png`**: every screen on one sheet. Each screen also has its own PNG at 1440 wide, and the key
  ones at 390 (`*-390.png`, rendered at 2×).
- **`*.html`, `v2.css`**: the mockups. `v2.css` imports `base.css`, `river.css` and `river-head.css`; the header is
  the approved one, untouched.
- **`assets/dev-*.png`**: the FM-1's screens (its play page, the restore question, STOP FIRST, RESTORED, NOT
  RESTORED), 240 × 240 as the device draws them.
- **`scripts/`**:
  - `audit.mjs` takes the audit's screenshots (the service running, see `server/README.md`);
  - `dump.mjs` takes the real card content (the River at `RIVER_URL`; the assembled site served by
    `website/serve.py build/website/river PORT` is enough);
  - `device-screens.sh` shoots the FM-1's screens with the simulator;
  - `build.mjs` and `parts.mjs` write the HTML;
  - `render.mjs` writes the PNGs and the contact sheet:
    `node design/river-v2/scripts/build.mjs && node design/river-v2/scripts/render.mjs`.

## This pass (24 Oct), after Styves' decisions

- **The list's columns:** no DRONE column (it would read CUSTOM almost everywhere; the drone stays in the opened card).
  A **BASS** column instead: **SEQ** when the shard's bass plays its step sequence, empty otherwise. It is GLINT's
  badge in the bass's colour, lamp, so ARP/SEQ (ice) and SEQ (lamp) read as siblings, the device's track colours.
  AURORA (the example's: D2, HOLD, A2, CUT, G2, E2) and a few others carry one. Every screen with rows: 01, 02, 04,
  04b, 05, 15, 30, and Quick Backup's slots (40).
- **New 04b:** AURORA opened with BASS open: SEQ on BASS's row, then the sequence's line of time (a NOTE a lamp line,
  on through its HOLDs, a CUT a gap), as `river-cards.js` draws it. It also shows "No description. Add one".
- **The toolbar's filters** use the FM-1's words: KEY, SCALE, GLINT, BASS, SHARED (SHARED only in one's own River).
  No DRONE, no tags.
- **The Community River (30, 05)** is sorted **NEWEST** by default. MOST PLAYED and FAVORITES sit beside it, one click
  each. Its rows end with the favorites count and the date shared; no titles or tags anywhere.
- **Send to FM-1 (10–19c)** keeps today's interaction (the cards dragged into the 4 × 4 grid), redrawn in v2's
  language, with the states Styves asked for. See below.

## The direction, in one paragraph

**The FM-1's own screen, made into a library.**
- **No more blocks.** The shards are rows on hairlines, a river you read from top to bottom. A row shows ▶, the
  shard's glyph, its name and the first line of its description, the line of what plays, the key, GLINT's type, the
  bass's sequence and one date. It scales from 2 shards to 2,000 without changing shape.
- **One title language.** Each page and each dialog is titled the way the FM-1 titles its pages (tracked capitals, a
  count in a light weight, a waterline under it). The serif stays for what people wrote and for sentences.
- **One action zone per shard**, in the head of the opened card.
- **The device's buttons are drawn as a map of the panel**, never as buttons.
- **In the dialogs, the cable between the laptop and the FM-1 is the river.**

The colours keep the device's meaning:
- ice is a choice made, something sending, GLINT;
- lamp is the hand (a warning, a card being dragged, the FM-1 waiting for SEL) and the bass.

## Screen by screen

**Your Private River (01, 02, 02c).** The title line is also the switch: **SHARDS 28 · FAVORITES 4 · BACKUPS 3**.
- The subject you are in is large; the others are small beside it. Backups are one of these subjects, no longer a
  section at the bottom.
- At the right of the title sits the dock: **GET FROM FM-1 | SEND TO FM-1**. It is one control with two halves, the
  cable's two directions, side by side at the top as asked.
- **With 2 shards:** there is no toolbar, only the list and one quiet line under it (get more from the FM-1, or
  favorite some).
- **With 100:** a toolbar appears.
  - A search covers names and descriptions.
  - The filters use the FM-1's own words (KEY, SCALE, GLINT, BASS, SHARED) and need no tags.
  - A sort, and paging by 24.
- **The columns:** NAME, WHAT PLAYS, KEY, GLINT, BASS, then SHARED and the date added.
- **Selecting:**
  - Ticking rows turns the dock's Send half ice: **SEND ③ TO FM-1**.
  - A selection bar replaces the column labels, for the rarer bulk actions (Download, Share, Delete, Clear).
- **No more truncated names:** a row gives the name its whole 16 characters. SHARED moved to its own quiet column, so
  it no longer eats the name.

**The shard card (04 one's own, 04b with the bass's sequence, 05 someone else's).** Opening a row lifts it into a
panel in place.
- **Its head is the only action zone.**
  - Someone else's shard: **Send to FM-1** (primary), **Favorite** (a GLINT star), **Copy link**, and **Report** as
    plain text with no icon.
  - One's own: **Send to FM-1**, **Edit**, **Share / Shared** (a toggle), **Copy link** (once shared), and
    **Delete** as plain text.
- **Nothing repeats.**
  - The name exists once, and a shard has only a name and a description. When an own shard has no description,
    "No description. Add one" replaces it.
  - Author, dates, licence, firmware and counts sit in one quiet line. Zero counts are left out.
  - The key, the drone, GLINT and the bass live in the sections.
- **The body:**
  - left: what plays (the keyboard as the screen draws it, the bass's sequence under it in lamp) and GLINT;
  - right: every page, each with its map of the FM-1. An opened BASS shows the sequence's line of time first.
- **On a phone** the actions wrap under the name, the section rows take two lines, and the settings show two by two.

**Page titles (03).** The FM-1 screen header, for the web:
- the subject in Barlow capitals, 30 px, tracked;
- its siblings small beside it (13 px, mist);
- a count in a light weight;
- a waterline under the whole line, brightest under the title.

It never repeats the header's words: it names **what is on the page**, never where you are. On each page:
- SHARDS / FAVORITES / BACKUPS;
- SHARDS 142, with one sentence for newcomers;
- QUICK BACKUP;
- ACCOUNT.

It takes about 120 px under the header, where today's top took about 600, so the first shard is on the first screen, at 1440 and on a phone.

**The device hints (06).** Each section shows a small diagram of the FM-1's block of twelve buttons (two rows of six,
as on the panel) with the buttons that lead there lit in ice, then their names in quiet text: "EDIT · ENV · LFO". It
has no frame, no fill and no hover, so it reads as a picture of the instrument. An opened section also names each
page's own button under the page's name. Two alternatives are shown for comparison: quiet italic text only ("on
EDIT, ENV, LFO"), and keycaps with no frame.

**Later: live and the editor (07, 08).** These are one frame each, to keep room in the design.
- **Live, while a shard plays:**
  - the keys sounding now light up;
  - GLINT's sequence runs under a playhead;
  - a new left column holds the three LFOs, each a moving scope with its targets in their track's colours (drone
    fog, GLINT ice, bass lamp).
- **The editor:** the opened card turns into an editor.
  - An EDITING band appears, with Undo / Revert / **Save as…** in the head.
  - Touched values turn lamp, the hand's colour. The value being dragged is framed.
  - Save as asks for a new name (16 characters, the FM-1's) and an optional description. It never overwrites the
    original.

**Send to FM-1 (10–19c).** Today's dialog (`river-device.js` placeDialog), which Styves likes, in v2's language. It is
titled as the FM-1 titles a page: **SEND TO FM-1** and a count (**3 SHARDS**), a waterline under it; no serif
headline. It reads the FM-1 by itself on opening: there is no "Read the FM-1" button.
- **The cable's states** share one drawing: the laptop with the River on its screen, the USB-C cable (the river, sagging
  to the table and up into the FM-1), and the FM-1 showing its real play page. Each state is the drawing, one status,
  one sentence.
  - **Reading (10):** a glint, FRASIL's four-pointed star, travels along the cable from the FM-1 to the laptop, its wake
    behind it. Cancel.
  - **Not connected (11):** the cable lies unplugged on the table, the FM-1's screen dark and its port glowing lamp.
    One sentence, one hint, **Try again**.
  - **FRASIL not installed (12):** the FM-1's screen shows another firmware's colours under a lamp plate, FRASIL NOT
    INSTALLED, and the same words under the drawing. **Install FRASIL**, **Try again**.
- **Ready (13: one shard, from the Community River; 14: three of one's own):**
  - left, **THE SHARDS**: a card per shard, what you drag. Its name, the slot it came from (FROM 12), and its own slot
    picker (SLOT 03 · ABYSS, or CHOOSE A SLOT). **Back where they came from** under them, when some came from a slot;
  - right, **THE FM-1 · 16 SLOTS**, four by four: each slot's number, name and state (SAVED, FACTORY; EMPTY dashed).
    A placed slot shows the shard in an ice frame and what happens: REPLACES ABYSS, INTO AN EMPTY SLOT. A 16-character
    name takes two lines in a slot, never cut;
  - over the slots: **● FRASIL 1.1 · read just now**, and a quiet **READ AGAIN**;
  - under them, a line per shard: "SLOW THAW goes into slot 03, in place of ABYSS: it is in your River."
  - **14 shows a card mid-drag:** NORTHERN LIGHT under the hand over slot 13, which turns lamp and says INTO AN EMPTY
    SLOT; its card waits, dashed, in the list. The Send button counts what is placed (Send 2 to FM-1).
  - **On a phone (13, 14 at 390)** there is no dragging: the card's slot picker is the control ("Choose a slot for
    each shard"), the unplaced one lit; the slots stay as a compact map.
- **Choose (15):** with nothing selected, a first step: search, **SHARDS 28 · FAVORITES 4**, the rows ticked, up to 16
  at once. **Choose their slots** leads to Ready.
- **Confirmation (16):** **SEND 3 SHARDS NOW?** "What plays on the FM-1 stops. Slots 03, 12 and 13 are replaced: SLOW
  THAW in place of ABYSS, which is in your River; POLAR NIGHT and NORTHERN LIGHT into empty slots. The other slots
  and the settings stay." Then, quietly: "the FM-1 asks once: answer YES with SEL." On a phone it is a sheet over the
  sheet.
- **Sending (17):** the glint travels the other way, to the FM-1; progress on a waterline.
- **The FM-1, close (18–19c).** Once sent, the drawing moves in on the FM-1: its screen is the device's own (the
  simulator's shot), legible, and its twelve buttons are named, lit as the device lights them.
  - **Press SEL (18):** the restore question as FRASIL draws it: RESTORE FROM USB, **RESTORE?**, NO [FX] / YES [SEL].
    Only FX and SEL are lit; SEL also wears a lamp ring, the hand's colour: the River waits for it.
  - **Done (19):** RESTORED, **OK [SEL]**, only SEL lit; "SLOW THAW in 03, POLAR NIGHT in 12, NORTHERN LIGHT in 13."
  - **Not sent (19b):** NOT RESTORED, "FX, OR NO ANSWER IN A MINUTE. NOTHING CHANGED.", OK [SEL]. **Send again**.
  - **Something still held (19c, a variant):** STOP FIRST. The River sends CC 120 before restoring, so the FM-1 lets go
    of what plays by itself; only a key held down, or a note held by another MIDI device, still stops it.
- "Hold PLAY" is gone from the River everywhere.

**Get from FM-1 (20, 21).** The same reading and failure states as Send, and the same title language. Then the
sixteen slots appear as a checklist:
- NEW scenes can be ticked;
- scenes already in your River are marked and can't be ticked;
- factory scenes can be ticked but start unticked;
- empty slots are dim (hidden on a phone).

Under the list, one option: **Also store the whole backup**, with an optional note. The button says exactly what will
happen: "Save 2 shards and the backup".

**Community River (30).** The same rows, with the author's name in the serif, then the favorites count and the date
shared. The toolbar is the private one's without SHARED, and its order is three words side by side: **NEWEST** (the
default: a river flows), MOST PLAYED, FAVORITES. The marketing hero, the drifting line, "How the River works" and "Good
to know" are gone; the licence stays in the one sentence under the title. The first shard is at y ≈ 390 (today
≈ 1,060), and on a phone it is on the first screen.

**Tools / Quick Backup (40).** With one tool, the Tools page *is* Quick Backup, with no hop.
- The dock pairs **Back up the FM-1 | Open a file**.
- After a backup, a source line ("From your FM-1 · 20:43 · 4 saved, 7 factory, 5 empty, and the settings") carries
  the file's own actions: **Download the backup** (primary) and **Restore it all…**
- The sixteen slots use the same rows, numbered, with the same KEY, GLINT and BASS columns. The selection sends
  shards to the FM-1 or downloads them.
- The terminal note shrinks to one line at the bottom.

**Profile (41).** One column of rows on hairlines: a label, the value in the serif, a quiet note, and the row's own
action at the right.
- The rows: display name, email, password, sessions, your data, privacy, leaving.
- It replaces the five uneven boxes.
- The title is ACCOUNT, since the header already shows the name.

## Styves' decisions (24 Oct, plan §5)

1. **The direction is approved:** rows on hairlines, the FM-1-style titles, GET FROM FM-1 | SEND TO FM-1 at the top,
   one action zone per shard, the device hints drawn as a map of the panel.
2. **The Send dialog** keeps today's interaction (the cards dragged into the slots), with the automatic read, the
   drawing and its states, the confirmation, the sending and the result. The drawn FM-1 follows SEL = yes, FX = no:
   OK [SEL], NO [FX] / YES [SEL].
3. **The list's columns:** no DRONE; a BASS column, SEQ or empty. It follows the bass sequencer
   (`.omc/plans/frasil-bass-seq.md`), firmware first.
4. **The moving waveform while a shard plays** stays, in the row and in the opened card.
5. **Favorite is a bookmark, not a copy.** A `favorites` table; favorites are listed under FAVORITES in Your Private
   River and sent to the FM-1 like one's own. If the author unshares one, it stays, marked "no longer shared".
6. **No titles, no tags.** A name (16 characters) and a description only. The filters use the FM-1's words: key,
   scale, GLINT, bass.
7. **The Community River:** newest first by default; Most played and Favorites one click away.
8. **The README's recommendations stand where nothing was said:**
   - the "FM-1 connected" dot on the dock comes later;
   - phones: the dock on Android; on iOS one line ("Sending needs Chrome or Edge on a computer, or an Android phone");
   - factory scenes allowed in Get from FM-1, unticked;
   - the Tools tab opens Quick Backup.
9. **The FM-1 drawing is right:** seven endless encoders and the MASTER pot, eight round controls.
10. **CC 120 before sending:** yes. FRASIL makes it RELEASE ALL; CC 123 stays notes-off.
11. **Order:** the bass sequencer first (firmware, then schema), then River v2.

## Found while drawing the FM-1's screens

- **A pop-up line is cut on the device.** NOT RESTORED after FX (or no answer) reads "FX, OR NO ANSWER IN A MINUT":
  app.c's `pop.line[3][28]` holds 27 characters and the line has 29. `device-screens.sh` shoots it from a copy with
  the buffer at 32, so the mockup shows the line whole. The fix in app.c is that one number (or a shorter line).
- **STOP FIRST still says "HOLD PLAY TO RELEASE ALL, THEN RESTORE AGAIN."** With CC 120 sent first, it now only
  appears when a key is held down or another MIDI device holds a note; holding PLAY doesn't help with a key under a
  hand. 19c's River sentence says "Let go of it, then send again"; the device's words may want the same.

## Two bugs to fix whatever is chosen (from the audit)

- **G6. Disabled buttons in dialogs look enabled.** `.river .btn:disabled` doesn't reach the dialogs, which are
  appended to `body`. For example, "SEND TO THE FM-1" shines before anything is read.
- **C5. An opened card with no tags shows a lone "0".** The cause is `x.tags && x.tags.length && h(…)` in
  `river-yours.js` and `river-navigate.js`. (Tags go away altogether with v2.)

## To confirm

- **The FM-1 close-up (18–19c).** Once sent, the drawing moves in on the FM-1 so its real screen can be read; the
  laptop and the cable leave the frame. The other way is to keep the whole drawing and show the screen enlarged
  beside it.
- **SEL's lamp ring** on the question, beside the device's own lights (FX and SEL both lit, as the device lights them).
- **The Send button counts what is placed** (Send 2 to FM-1 while one card waits), as today's sends only the placed.
- **SHARED** is left out of the Community's filters (every shard there is shared).
- **Get from FM-1 (20, 21)** only took the new title language; its interaction is as before.
