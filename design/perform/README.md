# FRASIL Perform — mockups

perform.frasilsynth.com, later: the FM-1 plugged into a computer, the browser full screen (a projector on stage, a
screen in a filmed performance). The FM-1's river, very large, and what the hand does on the FM-1 shown as it happens
(a page opened, a knob turned, a scene loaded, keys played, a button), then gone, leaving room for the next.

Open the page served from the repo's root (fonts and logo come from `website/`):

    python3 -m http.server 8093 --bind 127.0.0.1 --directory .
    open http://127.0.0.1:8093/design/perform/

Keys: `1`–`7` the frames (CONNECT, LIVE, KNOB, SCENE, KEYS, FREEZE, FILM), `A` / `B` the direction, `F` full screen,
`H` hides the bar. FILM plays 72 s: three factory scenes and a hand on the FM-1.

Nothing in the river is drawn by hand: `data/live.json` is what FRASIL's engine played (`app/build/emu/app.wasm`,
web_live's 52 floats, 30 frames a second; `design/river-live/scripts/capture.mjs`), and `proto/river.js` draws it as
`app/ui.c` draws the play page's river (river, current, crystal, glints, bass_bed, haze), more detail rather than
bigger pixels. The pages, names and values written as the FM-1 writes them come from `server/schema.json`.

**Perform's version:** `VERSION` at the top of `live/live.js` (1.0 now), shown on the first screen; raise it with each change people will see.

## Live

`live/` plays for real. **CONNECT**: the FM-1 itself, over Web MIDI (Chrome, Edge), FRASIL with PERFORM (below): the
river drawn from what the FM-1 says sounds, each gesture as the hand makes it. **DEMO**: FRASIL's factory scenes in the
River's player, with sound (`website/assets/js/river-player.js`, `app/build/emu/app.wasm`), the river from what the
engine says sounds (web_live, as heard), the gestures from AUTO's hand or the computer's keys:

    open http://127.0.0.1:8093/design/perform/live/

`← →` the scene · `TAB` the page · `1`–`4` its knob · `↑ ↓` or the wheel turn it (`SHIFT` faster) · `K` the keys'
notes · `SPACE` AUTO · `A` / `B` the direction · `H` the header · `F` full screen · `ESC` back. Checked by
`node design/perform/scripts/live_check.mjs`.

## PERFORM over USB (app.c `pf_tick`, `app_sysex`)

SysEx `F0 7D 46 50 cmd … F7`. Nothing is built or sent until a page says HELLO; every frame goes whole or not at all,
never waiting (fm1-base's `plat_sysex_try`), from the main loop once a UI frame, never from the render.

| | cmd | bytes after cmd |
|---|---|---|
| page → FM-1 | 1 HELLO | version (1), flags (pf7 on; 1: COMPUTER AUDIO, the FM-1's own output quiet while the page plays its USB audio, as SPK/JACK says again at BYE or 5 s unheard); again every ~2 s, the FM-1 quiet once not heard for 5 s |
| | 2 BYE | |
| FM-1 → page | 1 HELLO | version, NPARAM (2), FRASIL's version (ASCII) |
| | 2 SCENE | slot (16: NEW SHARD), dirty, name (ASCII, ≤ 16) |
| | 3 VALUES | first (2), n, n values (2 each): every setting at HELLO and after a load |
| | 4 VALUE | parameter (2), value (2), encoder (plat.h EN_*, 127: not a knob's) |
| | 5 PAGE | page (PG_*, the schema's order), octave + 3, keys (0 DRONE, 1 GLINT, 2 BASS) |
| | 6 FRAME | ~18 a second: the drones' 8 (note, level), GLINT's 4 (note, level as heard, 1/20 s since struck), the bass (note, level), the LFOs on (bits), each LFO (phase, value, turns (3)), the tracks' 4 meters, the transport (bits: 1 the drones, 2 GLINT, 4 the bass playing; pf7 on); note 127: none, levels 0..127 |
| | 7 BUTTON | button (B_*), down |
| | 8 KEYS | the keys held, bit k: key k (F3 up), four 7-bit bytes, lowest first; sent as they change (pf8 on) |

Two bytes: 7-bit high, low. Its cost, measured on the FM-1 (`scripts/fm1_load_ab.py`, the scene playing, 2.5-pf2):
load 69 % off, 67-69 % on, the longest render 76 % both, no late half, no shedding. `scripts/fm1_listen.py` hears it
from a terminal; `scripts/fm1_check.mjs` drives the page with the FM-1 plugged in.

## The two directions

- **A · OVERLAY**: the FM-1's own overlay, large in the middle; the river dims under it.
- **B · CAPTION**: under the waterline, the page's four settings as the FM-1 lays them out (the one turned in lamp)
  and the FM-1 drawn small with what the hand touched lit; the river stays whole.

## Regenerate

    ./build.sh --host && ./build.sh --web
    node design/river-live/scripts/capture.mjs '[["NORTH RIVER",-1,24],["RESILIENCE",-3,24],["LONG NIGHT",-4,24]]' design/perform/data/live.json
    .venv/bin/python design/perform/scripts/data.py
    node design/perform/scripts/shoot.mjs        # png/, 1920 x 1080
    node design/perform/scripts/fps.mjs 2        # frames per second, Retina full screen
