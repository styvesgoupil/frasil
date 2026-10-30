# FRASIL River — editor mockups

Phase 2 of `.omc/plans/frasil-river-ux-v2.md` §2 ("Plus tard"): the opened card becomes an editor; the sound follows;
SAVE AS puts a new shard in Your Private River. Open `index.html`.

The edits are the engine's own. Two exports were added to `app/web.c`:
- `web_param(p, x)`: a sound setting (or TEMPO) set as a MIDI CC learned on it sets it (`param_set`: a GLINT preset's
  sound, CUSTOM once a drone's setting is turned, GLINT's type...);
- `web_scene_edited()`: the shard played, with its sound and TEMPO as they are now.

`app_worklet.js` takes `{op: "param", p, x}`. The prototype (`proto/editor.js`, `editor.css`) runs app.wasm on the page,
silent, to edit a shard's bytes, then draws the card again from them and stages each state. `scripts/shoot.mjs` starts
the River's service with a fresh database (Mira shares KATABATIC; Styves has ABYSS and three others), opens the cards,
stages the states and, where a card plays, plays the edited bytes for real.

    ./build.sh --host && ./build.sh --web && tools/website.sh
    node design/river-editor/scripts/shoot.mjs [30 ...]

## The design

- **No edit mode.** Every opened card turns, like the FM-1's knobs: drag a setting left or right (a vertical drag
  scrolls), or the arrow keys (Shift: by ten); a choice also opens its list on a click. Hovering shows a panel under
  the setting and its gauge brighter.
- **The hand is lamp.**
  - Turning: a lamp frame, and WAS beside the value.
  - Changed: the value lamp, a dot before the name, a slate tick on the gauge where it was (and "Was …" on hover).
  - A section holding a change: a dot after its name.
- **One line under the description:** EDITED, how many changes, and the promise: the shard stays as it was until it
  is saved as a new one.
- **The head's actions become the editor's:** UNDO, REVERT, SAVE AS…
- **SAVE AS:** the name as the FM-1 shows it (16 characters, its charset), the description (one's own shard's to
  change; empty for someone else's). Someone else's shard is credited (CC BY 4.0) through the service's origin (the
  columns it keeps for copies): "After KATABATIC, by Mira K.", shown where SHARED would be.
- **Signed out:** edit and listen; saving asks to sign in, the changes waiting on the page.
- **A card closed with changes:** EDITED · NOT SAVED under its name. The changes stay in the tab: a reload, or a page
  left and come back to, keeps them (built so instead of asking before leaving: signing in is leaving the page).

## To decide

1. Editing straight away, with no mode.
2. On one's own shards, also SAVE (replace; asked first when shared), beside SAVE AS?
3. Today's Edit (name and description) becomes RENAME.
4. Later: what plays (the notes, the arpeggio, the sequences) and the TEMPO.

## Styves' decisions (2026-10-25): yes to all four

## Built

- `website/assets/js/river-edit.js`: the editor (river-cards' hooks: a row's scene its draft, a row made, a card's
  body or section built). A silent app.wasm on the page is the model; `web_scene_edited` keeps the sound as STORE does
  (`kept_v`: a drone's CUSTOM settings and its preset's number), so a change to one setting changes that byte only.
  Drafts by shard id, kept in the tab (sessionStorage); renamed meanwhile, they follow; re-saved elsewhere, they go
  (said). UNDO and REVERT retune the sound playing from the kept values (the drone through CUSTOM, GLINT's preset
  first). Settings set while ▶ starts wait in the player (`RiverPlayer.param`).
- The SAVE AS sheet uses the River's own dialog fields (the serif title as its other dialogs), not the mockups'
  capitals.
- Server: `POST /api/shards {origin}` (someone else's public shard, or a favorite no longer shared: its first author
  credited; one's own: its credit passed on), `PATCH /api/shards/{id} {data, layout, firmware}` (every derived column
  read again; `share` takes no sound).
- Tests: `website/tests/river_editor.mjs`, `server/tests/test_editor.py`.
