# FRASIL River — live view mockups

Phase 1 of `.omc/plans/frasil-river-ux-v2.md` §2 ("Plus tard"): while a shard plays (▶), its opened card follows the
sound. Open `index.html` (the films loop at the top, then the stills).

Nothing here is drawn by hand. The engine's state is real: `scripts/capture.mjs` runs FRASIL's browser engine
(`app/build/emu/app.wasm`, as the River's ▶ runs it) and reads its new export `web_live` (`app/web.c`: 48 floats, the
drones' voices, GLINT's, the bass, the three LFOs, both sequences' steps, the meters) 30 times a second while shards of
the example backup play (`data/live.json`). `scripts/shoot.mjs` starts the River's service with a fresh database,
signs Styves up, adds four shards, opens a card in Your Private River, puts the prototype in (`proto/live.js`,
`proto/live.css`) and feeds it those frames: the PNGs and, with ffmpeg, the films.

    ./build.sh --host && ./build.sh --web && tools/website.sh
    node design/river-live/scripts/capture.mjs '[["NORTH RIVER",-1,16],["GLACIER",9,16],["AURORA",10,16],["LUMIERE NOCTURNE",-2,16]]' design/river-live/data/live.json
    node design/river-live/scripts/shoot.mjs [01 ...]

## What it shows

- **The keys light with the sound**, each as bright as its voice's envelope: the drones fog and white (breathing with
  the drones' meter), a glint ice in the sky above its key (rising a little as it fades, as on the FM-1's play page),
  the bass lamp in the bed under its key. What is latched stays drawn, dimmed to a quarter.
- **The keyboard reaches what the shard plays**: C7 at least, up to C9 (NORTH RIVER's GLINT, OCTAVE +2 and RANGE 3 OCT,
  plays up to E8). Today's card leaves RANGE's octaves above C7 out.
- **The LFO column**, at the card's left (a band of three under GLINT below 1180 px): each LFO its shape over a turn
  (lfo_scope's: four turns for RANDOM and DRIFT, with the engine's own random values), as deep as DEPTH from POLARITY's
  side, a glint where it is now; under it what it moves, in its track's colour, with a gauge of its value now. Off:
  slate, OFF, NO TARGET.
- **The settings an LFO moves** (tagged LFO n in the card) show their value now as a tick on their gauge.
- **GLINT on SEQ**: the playhead on the step playing, the note sounding white. **On ARP**: the walk it really takes,
  flowing left from the right edge (a step CHANCE left out, a gap); at rest, the walk its MODE would take, as today.
- **BASS's sequence** (its section opened): the playhead along the line of time, the note sounding.
- The line of what plays ripples as today.

## Styves' decisions (2026-10-25): yes to all

1. The keyboard as wide as the shard plays, C7 at least, up to C9.
2. The LFO column stays when no LFO is on: three quiet OFF slots.
3. Also live: the MIXER's faders with each track's meter (an opened MIXER draws the FM-1's MIXER page, 48 dB meters),
   and the DRONE's strata breathing with the tide.

## Built

The prototype (`proto/`) made the mockups; the River's own code took it over (`data/live.json` and `proto/` read the
48 floats web_live had then; it has 52 now, each GLINT voice's time since its strike added). Only the first still and
the first film are kept in git (`.gitignore`): `shoot.mjs` makes the others again.

- `app/web/emu/app_worklet.js`: `{op: "live", on}`; while on, a frame `{type: "live", t: currentTime, v}` about 30
  times a second (the River only: `RiverPlayer.follow`; the landing's LISTEN does not ask).
- `river-player.js`: the frames kept a moment and given out as they are heard (`AudioContext.getOutputTimestamp()`),
  blended where a value glides: `RiverPlayer.live()`.
- `river-cards.js`: the live parts (`.lv`, each `_live(f, ctx)`), `follow()` (one requestAnimationFrame loop for the
  card playing; reduced motion: no ripple, the rest four times a second), `heard()` (a card at rest when it stops).
- Quick Backup: an opened row's slot number has its own column again (its name was pushed to the right).
- Tests: `river_audition.mjs` (web_live: GLACIER's drones and sequence, AURORA's bass and LFO, NORTH RIVER's LFOs,
  stopped), `river_flows.mjs` (NORTH RIVER opened and playing: the keyboard to C9, the LFO column, keys lit, LFOs
  moving, GLINT's walk, the MIXER's meters and the bass's playhead; at rest once stopped; the opened row's head).
