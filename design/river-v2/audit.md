# FRASIL River v2 — the audit

The River as it is on `dev` (073eeef), built with `tools/website.sh` and served by the service on this machine, used
like a player: two accounts (Styves, then a signed-out visitor), a simulated FM-1 holding the example backup
(`website/tests/fake_fm1.mjs`), 28 shards in the private library, 10 shared. Every page and flow at 1440 and 390 wide.
The screenshots are in `audit/` and the script that takes them is `scripts/audit.mjs`.

Styves' own points come first in each part, marked **(S)**. The others are mine.

---

## 1. Everywhere

**G1. The page titles (S).** Each page opens on the marketing site's title: a 128 px serif on two lines, the second
one in italic ("Community / *River*", "Quick / *Backup*", "*Tools*", "*Profile*"). It repeats the header, where the
same word is already lit. It also takes the first screen: at 1440×900 the Community River shows no shard at all above
the fold, and on a phone the first shard is about 2.5 screens down.
![](audit/c01b-community-fold-1440.png) ![](audit/c01-community-390.png)

**G2. Headers stacked on headers (S).** Each section has a label, a serif title and a count that say the same thing:
- MY SHARDS / *Your shards* / 2 SHARDS;
- GRAB FROM MY FM-1 / *From the FM-1 to Your River*;
- MY BACKUPS / *Your backups* / 1 BACKUP;
- COMMUNITY RIVER / *Every shard in the River* / 10 SHARDS · NEWEST FIRST.
![](audit/b04-yours-2-shards-1440.png)

**G3. Blocks everywhere (S).** The parts are all boxes:
- the cards;
- the boxes inside the cards (the line of what plays, the keyboard);
- the bordered buttons;
- the tiles, the auth card, the profile's boxes.

The eye never rests. Every closed card also repeats its labels (KEY, DRONE, GLINT): 84 labels on the 28-shard page.
![](audit/b10-yours-28-shards-1440.png)

**G4. Documentation inside the app.** "How the River works" and "Good to know" sit at the bottom of the Community
River and Quick Backup (a third of the page). They also hold advice that is now wrong ("hold PLAY first").
![](audit/c01-community-1440.png)

**G5. One action, several words.** One action often carries two or three names:
- **getting scenes from the FM-1:** "Grab from my FM-1" (empty state), "Back up my FM-1" (its button), "Back up your
  FM-1" (Quick Backup);
- **the Community River:** "Navigate the River" (Your Private River), "Community River" (header), "Every shard in the
  River";
- **sending to the FM-1:** "Send", "Put into slots", "Restore", "Send several to my FM-1…";
- **keeping a shard:** "Save", "SAVED", "Save to my River", plus a bookmark icon;
- **"Share your shards":** a button on the Community River that only goes to Your Private River.

**G6. Bug: in dialogs, disabled buttons look enabled.** `.river .btn:disabled` only applies inside `main.river`, but
the dialogs are appended to `body`. So "SEND TO THE FM-1" shines ice-blue before anything is read or placed.
![](audit/d06-send-no-fm1-1440.png)

**G7. Status far from its cause.** The page's "voice" (a dot and a sentence) shows at the very top of Your Private
River, whatever it is about. On a new account it says the same thing as the orange banner just under it.
![](audit/b01-yours-new-unverified-1440.png)

## 2. Your Private River

**Y1. The main actions are sections at the bottom (S).** The signed-in page is three sections stacked: My shards,
Grab from my FM-1, My backups. Getting scenes from the FM-1, the first thing a player does, sits under the whole
library: with 28 shards it is 1,300 px down. Backups are a third section with one row.
![](audit/b10-yours-28-shards-1440.png)

**Y2. The empty state takes two clicks and two names.** "Grab from my FM-1" only scrolls to another section, whose
button says "Back up my FM-1". ![](audit/b01-yours-new-unverified-1440.png)

**Y3. The grab flow is heavy.** It lays out the 11 non-empty slots as full cards in the middle of the page, factory
scenes included. Under them sits a bar with two different outcomes mixed together: "Select the shards to save / Select
all saved / Save to my River" and "A note for the whole backup / Store the whole backup". It also warns "Hold PLAY
first". ![](audit/b03-yours-grab-read-1440.png)

**Y4. It does not scale.** 28 shards make 7 rows of 4 boxed cards, with no search, sort or filter in one's own
library (the Community River has them). The SHARED badge cuts the 16-character names, though the name is all a shard
is called: "POLAR NI…", "SLOW THA…", "NORTHER…", "FRAZIL D…", "ST LAWRE…".
![](audit/b10-yours-28-shards-1440.png)

**Y5. "Send several to my FM-1…" lists the whole library.** It opens the slot dialog with all 28 shards in a column,
each with a "Not placed" menu. There is no way to choose a few first. ![](audit/b11-send-several-28-1440.png)

## 3. The shard card

**C1. Things repeated (S).**
- **The author:** "by Styves" appears twice in the opened card (under the name, and "BY Styves" at the right).
- **The dates:**
  - the closed card says "FROM SLOT 03 · 23 OCT 2026";
  - the opened one says "Added 23 Oct 2026 · … shared 23 Oct 2026";
  - the Community card says "Shared 23 Oct 2026".
- **The sound:** the head shows KEY, DRONE and GLINT, and the sections under it show them again:
  - D PHRYGIAN · FIFTH again in HARMONY;
  - ABYSS again in DRONE;
  - GLINT OFF three times.
![](audit/c03b-other-card-open-whole.png)

**C2. Two action zones (S).** Someone else's card has SEND, SAVE and a flag in the card's foot, then COPY LINK and
REPORT at the side of the opened part. So Report appears twice. One's own card has SEND, SHARED, a pencil and a bin,
then SEE IT IN THE RIVER and EDIT, so Edit appears twice.
![](audit/b08b-own-card-open-whole.png)

**C3. Unclear icons (S).** SAVE is a bookmark and Report is a flag. Edit and delete have no words at all (pencil, bin).

**C4. A name and a title (S).** ABYSS is also titled "Under the ice": two names for one shard. With no title, the
card says "No title yet." in italics, a placeholder that draws the eye.

**C5. Bug: empty tags print "0".** In an opened card with no tags, a lone "0" appears (phone screenshot, under "No
title yet."). The cause is `x.tags && x.tags.length && h(…)`, which returns `0`, rendered as text, in
`river-yours.js` `about()` and `river-navigate.js` `about()`.
![](audit/b14-own-card-open-390.png)

**C6. The device hints look like buttons (S).** EDIT, ENV, LFO, ARP, SEQ, FX, SEL, HOME and GLO have the same 1 px
border, caps and radius as SEND or COPY LINK. Nothing happens when you click them.
![](audit/b09-own-card-open-sections.png)

**C7. The opened card's ▶ floats.** It sits in the middle of the head row. Closing is a small chevron at the far right
of the action row. ![](audit/c03b-other-card-open-whole.png)

**C8. Zeros read as noise.** "0 plays · sent to 0 FM-1s · saved 0 times".

**C9. A closed card hides the best words.** The description, the only thing a person wrote, never shows until the
card opens. Most factory-based shards show only "SOUND ONLY", so nothing tells them apart.
![](audit/c02-other-card-closed.png)

## 4. The Community River

**R1. The first screen holds no shard.** It holds:
- the 2-line title;
- a lede;
- two buttons that leave the page (Share your shards, Quick Backup);
- a legal paragraph;
- a decorative line of drifting shards.

At 1440 wide the list's header starts around y = 850 and the first shard around y = 1,060. ![](audit/c01b-community-fold-1440.png)

**R2. The list's own header repeats the page.** COMMUNITY RIVER / *Every shard in the River* / 10 SHARDS · NEWEST
FIRST, then the NEW and CHARTS toggle ("Charts" means most played).

**R3. Saving needs an account, but the button doesn't say so.** Signed out, SAVE opens an invitation to sign in.
![](audit/c04-invite-sheet-1440.png)

## 5. The Send dialog

**S1. It opens empty (S).** It shows "Not read yet", 16 empty dashed boxes and a "Read the FM-1" button: a manual
step before anything can happen. ![](audit/d01-send-opened-1440.png)

**S2. "Hold PLAY on the FM-1 to release everything first" (S).** It is always in the footer, and STOP FIRST stops the
send when anything plays. ![](audit/d04-send-stopfirst-1440.png)

**S3. A failure is one line under an empty grid (S).** "FRASIL not found. Plug the FM-1 in…" appears with a red dot
under 16 empty boxes. It doesn't tell "nothing plugged in" apart from "plugged in, but not FRASIL".
![](audit/d06-send-no-fm1-1440.png)

**S4. Words for several shards, used for one.** For one shard, the dialog still says:
- the label "PUT SHARDS INTO SLOTS";
- the title "ABYSS, into your FM-1";
- the lede "Drag each shard onto a slot, or choose its slot".

The real control is a small "Not placed" menu, and dragging is never visible. ![](audit/d02-send-read-1440.png)

**S5. No confirmation before replacing (S).** A sentence under the grid ("ABYSS goes into slot 14, empty now.") is
all there is before Send. ![](audit/d03-send-placed-1440.png)

**S6. On a phone, the sheet is one long scroll.** Its sticky footer covers text (the words behind "Hold PLAY…").
![](audit/d01-send-opened-390.png)

## 6. Tools, Quick Backup, Profile

**T1. Tools is a hop.** It holds one card (Quick Backup) and a dashed note about the terminal: one more click to
reach the only tool. ![](audit/a03-tools-1440.png)

**T2. Quick Backup repeats the private grab** with its own words, its own grid of cards, its map of slots and its
bar. Its breadcrumb, its 2-line title and the header say where you are three times.
![](audit/a06-quickbackup-read-1440.png)

**P1. The profile is five boxes of uneven heights** in three columns, with a hole. "ALL OF IT" is vague. The "Signed
in · Styves · email · [Your Private River]" block repeats the header. ![](audit/b12-profile-1440.png)

## 7. The phone

**M1. The title, the lede and the buttons push everything under the first screen.** On the Community River the
first shard is about 2.5 screens down. ![](audit/c01-community-390.png)

**M2. An opened card is very long.** Each closed card carries 3 or 4 bordered action boxes.
![](audit/c03-other-card-open-390.png)

---

### The screenshots

The `audit/` folder, by name prefix:
- `a*`: signed out (the empty River, Your Private River, Tools, Profile, Quick Backup and its flows);
- `b*`: Your Private River signed in (new account, grab, 2 shards, edit sheet, own card closed and opened, 28 shards,
  send several, profile);
- `c*`: the Community River (cards closed and opened, invitation, report);
- `d*`: the Send dialog (opened, read, placed, STOP FIRST, done, no FM-1).

Each prefix is taken at 1440 wide, and at 390 for the key screens.
