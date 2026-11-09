# FRASIL's engine

FRASIL whole, its sound and its brain, in portable C with nothing of the FM-1's: no `plat.h`, no buttons, pages,
lights, screen or store. The FM-1's firmware is built on it (`app/`, its shell), so is the River in the browser
(`app/web.c`), and so can an iPad app or a plug-in be (`frasil_engine.h`).

| File | What |
| --- | --- |
| `frasil_engine.h`, `frasil_engine.c` | the engine alone: its public API (init at a rate, render, the keys, THE KEYS, MIDI, the settings, the scenes, a UI's view, the effects') |
| `frasil_params.h` | the settings' numbers by name (`P_EQ_LOW` ...), the public enum `params.c` and the API share |
| `core.c` | the brain: what a key means, the chords, GLINT's arpeggiator and step sequencer, the bass's sequence and its LIVE take, PULSE, the LFOs, SPARKLE, the metronome, the grid (MIDI clock or TEMPO), MORPH, the scenes taken and stored, RELEASE ALL and RESTORE, MIDI's meaning, THE KEYS |
| `params.c` | every setting (its range, default and words), the scene's layout (a shard, 619 bytes, "FRS}"), the factory scenes, the presets (the drone's, GLINT's, MASTER EQ's), LFO TARGET's choices, `params_to_engine` |
| `harmony.c` | the key (ROOT, SCALE) and what a played note becomes (CHORD, VOICING) |
| `dsp/` | the sound, compiled apart at -O2: `frasil.c` (the voices, the mix, MASTER FILTER, MASTER EQ, LOFI TAPE, ROUTING, the pans, MASTERING, CUT), `glint.c` (GLINT and its delay), `bass.c` (BASS), `nuage.c` (CLOUD, the reverb); `fastmath.h` |
| `frasil_stream.h`, `.c` | SHARD STREAM: a performance as a recipe. The file (`.frstream`), the journal, the replay, and `fst_air`: a stream played by the engine's own milliseconds, joined at any place (`frs_tick_hook`, `frs_skip_to`), and `fst_live`: a performance heard live, each op applied by itself as it comes (`fst_op`, fst_apply's own code, a scene by its bytes: Perform LIVE's listener). The API alone, no platform: the same reader for the iPad and the radio |
| `live.c` | part of `core.c`: the running state Perform LIVE tells (`frs_live_save`, `frs_live_load`; the FM-1's STATE), field by field, and the DSP's (`dsp/brlive.h`: each unit's `*_lv`) |
| `tests/` | the engine alone against the FM-1's simulator (`run.sh`; `anchor.c`: ANCHOR through the API and a stream; `live.c`: ops given one by one, a replay's every sample; `snap.c`: an engine opened on a snap goes on as the first) |

## Alone

```sh
cc -O2 -ffp-contract=off -Iengine -Iengine/dsp -c engine/frasil_engine.c engine/dsp/frasil.c engine/dsp/glint.c \
   engine/dsp/bass.c engine/dsp/nuage.c
```

`-ffp-contract=off`: no fused multiply-add, the FM-1's samples bit for bit. `-DFRASIL_FS_CONST` builds it as the
FM-1's DSP is built: at 44.1 kHz alone.

`engine/tests/run.sh` (after `./build.sh --host`) runs these checks:
- **fastmath.h:** the copy is fm1-base's, its guard aside.
- **The API and the inside**, also under AddressSanitizer and UBSan:
  - the same render whatever the frames asked at a time;
  - one open at a time;
  - PANIC;
  - time-stamped MIDI clock and a host's tempo followed; a MIDI clock read as unevenly as the FM-1 reads it (between
    its screen's frames, held up 300 ms) followed within 0.3 % (GLINT's delay still), a new tempo within a bar;
  - the state;
  - the snapshot;
  - the clock past 2^32 ms;
  - a note under the scale's lowest;
  - 2.5's: the settings by name, their kinds, CLOCK, LFO TARGET's choices, TRACKS, the effects' view and curves, CUT,
    PLAY from silence;
  - 2.6's: CHOIR III's SOFT (by name, in a scene, on the sound, an LFO's target), SHAPE's words, a 2.5 engine's state,
    LOFI TAPE's drag in `frs_live`.
- **Against the FM-1's simulator:** `app/tests/engine_alone.txt` (MIDI and settings) renders alone and in the
  simulator: every renderhash the same.
- **A stream joined anywhere** (`tests/skip.c`): takes through the real file, every factory scene, MIDI, keys, scene
  changes, CUT, an engine opened again: a skip's messages to the DSP and the brain's state at T are a render's, at any
  buffer. Controls that must differ, or the test proves nothing.
- **Old streams** (`tests/old_streams.c`): `tests/old-streams/`'s takes of "FRSz" (3.0, before ARP SNAP) and
  `tests/old-streams-3.0.2/`'s of "FRS{" (3.0.2-b5's, SWELL turned through scenes and an ANCHOR among them), each as
  its engine played it, bit for bit, from the top and joined half-way.
- **Heard live** (`tests/live.c`): a captured start, then each op by itself (`fst_op`, its scenes and its state by
  their bytes) between the blocks its milliseconds fall on: every sample, setting and note the replay of the same take's
  file has; ops not known ignored, ops too short refused, both counted and harmless; quiet (no metronome) as on air.
- **Opened on a snap** (`tests/snap.c`, card #131): the running state (`frs_live_save`, `frs_live_load`: `live.c`,
  design/perform/LIVE.md section 6) taken 30 s in, a second engine opened on it, the same ops after in their
  milliseconds (`fst_live_at`): its brain the first one's every 100 ms and bit for bit at the end, the sound's level
  within the buffers' (the cloud's room starts empty); without the state, the control, elsewhere; ops late by a few
  milliseconds, then the snap at its place in the queue (`fst_live_at_state`), the first one's again; its own state at
  its place, left (the very same).
- **At 48 kHz:** the same sound, second by second, and measured: the bass's pitch, GLINT's echo, CLOUD's RT60,
  LOFI TAPE's delay and flutter, MASTER FILTER's 3 dB point, MASTER EQ's MID FREQ.

- **The rate:** 44.1 kHz to 96 kHz (`frs_init`). What holds time is sized at the rate (the reverb's lines, GLINT's
  delay, LOFI TAPE's loop), and each step made for 44.1 kHz is made over for it (the filters' and the EQ's
  frequencies, the tape's wow and flutter, the glides). The FM-1 runs at 44.1 kHz alone: there the rate is a
  constant, and its DSP compiles as it always did.
- **Blocks and time:** the engine renders 256 frames at a time and runs its brain every millisecond between them, as
  the FM-1 does. `frs_render` takes any number of frames.
  - **A call** between two renders acts at once.
  - **MIDI** is taken at its millisecond: `frs_midi`'s at the next one, `frs_midi_at`'s at the one of its frame in the
    host's buffer, so a clock keeps its time whatever the buffer.
  - **A host's tempo and transport** (`frs_host_tempo`) run the grid as a MIDI clock and Start would, its tempo
    exactly, from 13 to 999 BPM. A locate is a Start again, with no burst of clocks. The grid's top is where the host
    starts or locates, not on its bars: FRASIL has no song position.
  - **Latency:** what a message plays sounds from the first block after its millisecond: 0 to 256 frames after its
    frame, 5.8 ms at most at 44.1 kHz.
- **Built with `-DFRASIL_FS_CONST`** (the bench's: the DSP as the FM-1 compiles it), the engine runs at 44.1 kHz
  alone. Opening it again does not start the sound from nothing, and PANIC silences the voices, the bass and the
  queued notes alone.
- **Its version** (`frs_version`, `FRS_VERSION`): the FRASIL it is, "3.0", as `VERSION` has it (`run.sh` checks
  them alike: a release changes both).
- **One engine a process:** its state is static. `frs_init` opens it, and a second open is refused until
  `frs_close`. Two plug-in instances in one process cannot both have it.
- **Threads:** none of its own. Call it from the thread that renders (an audio thread takes the UI's calls from a
  queue of its own), or serialize the calls. The callbacks' words live during the call alone.
  - **From any thread, at any time** (open or not, while the audio thread renders): `frs_param_count`,
    `frs_param_info`, `frs_param_kind`, `frs_param_text`, `frs_version`, `frs_scene_new`, `frs_scene_factory`,
    `frs_lfo_targets`, `frs_lfo_target` and `frs_tracks_bits`, which read constant tables alone, `frs_fx_response`
    (it reads the floats given alone), and `frs_snapshot` and `frs_snapshot_fx`. `engine/tests/threads.c` checks them
    under ThreadSanitizer, every value of every setting.
  - **The exception:** GLINT SEQ's NOTE and BASS SEQ's NOTE in words carry the OCTAVE the audio thread puts on them.
    Call `frs_param_text` for those two from the thread that renders.
  - **`frs_snapshot`, `frs_snapshot_fx`:** the view, the live floats and the effects', as they were after the last
    block (C11's atomics, else GCC's and Clang's builtins; MSVC needs C11 with `/experimental:c11atomics`).
  - **Not from another thread:** `frs_param_get` and `frs_dirty` read what the audio thread changes (the
    snapshot has the dirty flag).
- **The tracks' transport** (`frs_track_play`, `frs_play_all`): a track stopped or played again as a moment. It is
  not an edit: a scene stored meanwhile has what the track would play, and nothing goes into `frs_state_save`.
  - **The drones:** their chords let go and are kept (`frs_view`'s `kept`); played again, each with its own notes.
  - **GLINT:** the arpeggio waits; the sequence stops.
  - **The bass:** on ARP, as GLINT's: it waits, its notes kept; on SEQ, its sequence stops; else a held note lets go and is
    kept (`bass_kept`).
  - **A sequence played again** starts at the grid's next step. If the other track's sequence plays, it starts in
    phase with it, mid-pattern: its steps fall where they would had both begun together, where the other's began,
    whatever their loops' lengths. If not, it starts from its first step. `frs_view`'s `playing` has each track's
    state.
  - **Only what is latched is kept:** a chord or a bass note held (MIDI's, a key's in HOLD, THE KEYS' HOLD) sounds on.
  - **A stopped track stays stopped** through a MIDI Start or a host's start and locate.
- **ANCHOR** (`frs_anchor_set`, `frs_anchor_back`, `frs_anchor_drop`, `frs_anchor_state`, `frs_anchor_get`,
  `frs_anchor_put`): the scene as it is now kept in RAM, to come back to gliding or at once (the FM-1's OCT− and OCT+
  together again: pressed, gliding; held, at once). What a scene keeps of the sound and its tracks comes back, not
  what plays. Gliding, what SCENES' MORPH glides (and BASS TONE, the bass's SPACE, AUTO VIB) glides; the choices
  switch at its half. A stream replays both
  (`FST_ANCHOR`, `FST_ANCHOR_BACK`, `FST_ANCHOR_DROP`). `frs_param_heard`: a setting as it is heard while it glides
  back (`frs_param_get` has the anchor's at once), for a screen that moves with the sound (the iPad's).
- **THE KEYS' HOLD with several fingers** (`frs_hold`):
  - **Counting:** each note's fingers are counted, so a note two fingers hold lets go at the last up.
  - **The bass** plays the last finger's note. When the top finger lets go, the last one still down plays again.
  - **What a latch, CLEAR or an edit lets go:** only what it acts on; the fingers' notes sound on. The River's editor
    lets them go first, because its silent engine never had them.
  - **A scene stored** has what is latched, not what is held.
- **A chord's kind on THE KEYS** (`frs_latch`, `frs_hold`): HARMONY's CHORD, or one given for that note alone (SINGLE
  .. DIM). CHORD itself is not set: nothing edited, no MORPH's step.
- **The settings** are the FM-1's, a byte each, numbered as `frasil_params.h` has them (`P_EQ_LOW`, `P_CLOCK` ...).
  The River's `schema.json` names them (its `ids`, its `params` with each value's words). `frs_param_info` and
  `frs_param_text` give the same names and words.
  - **By name, not by number:** a setting a scene keeps is added before `NSOUND`, so the ones after it move. 2.5
    added 23 (MASTER EQ, MASTER FILTER, LOFI TAPE, ROUTING, the pans): `P_KEYS` went from 130 to 153, and every
    setting past it 23 higher. `SPK/JACK`, `ANIMATIONS` and `CLOCK` came after `P_BQ_TRANS`. 2.6 added one, CHOIR
    III's SOFT (`P_CH_SOFT`, 153): `P_KEYS` 154, `P_CLOCK` 182, `NPARAM` 183. 2.7 added LENGTH (`P_BQ_NLEN`, 183,
    numbered last: none moved) and `P_BQ_LEN` became the bass's one RATE; then each layer's PLAYS (`P_SQ_TYPE`, `P_BQ_ON`:
    0 KEYS, 1 ARP, 2 SEQ; `P_BQ_ON`'s 1 was SEQUENCE on till then), the bass's arpeggio and the sequences alike, numbered
    last too: `P_BA_MODE` 184, `P_BA_RANGE`, `P_BA_CHANCE`, `P_BQ_GATE`, `P_BQ_ACC`, `P_BQ_NEW`, `P_BQ_UNDO`, GLINT's LENGTH
    `P_SQ_NLEN` 191, `NPARAM` 192. The arpeggiator is one, its layers `arp` and `barp`; the grid and the clock `grid`.
    Since then, every one numbered last, none moved: BASS TONE 192 .. 194, the bass's SPACE 195, AUTO VIB 196, 3.0's
    drone's sequence 197 .. 208, ARP II's LATCH, KEYS, METRONOME, COUNT-IN, ARP SNAP, SCREENSAVER 209 .. 226, LOFI TAPE
    II's four 227 .. 230, the LFOs' AMOUNTs `P_L1_AMT1` .. `P_L3_AMT4` 231 .. 242 (LFO n's TARGET k's:
    `P_L1_AMT1 + 4 (n - 1) + k - 1`), `NPARAM` 243.
  - **The LFOs' AMOUNTs** (one per TARGET, a multiplier): 0 .. 200, x0.00 .. x5.00; 0 .. 100 is x0.00 .. x1.00 by
    0.01, 100 .. 200 x1.00 .. x5.00 by 0.04 (x3.00 at 150). 100 (x1.00, the default) the LFO as it moved its TARGET
    before, bit for bit (its factor 1 exactly). What the LFO adds to its TARGET is times the AMOUNT, the setting still
    in its range. Its words "×1.00" (UTF-8's ×).
  - **SWELL** (`P_SWELL`, GENERATIVE FX's K1) is the scene's (since "FRS}": stored, taken, ANCHOR's); ENV held
    (`frs_swell`) and MIDI's CC 11 swell the drone too, of the moment: the most of the three sounds.
  - **Their kind** (`frs_param_kind`): kept in a scene (2.5's effects and pans, 2.6's SOFT too, LOFI TAPE II, the
    AMOUNTs, SWELL), kept beside it (CLOCK too), of
    the moment, a step's, an action (turned to 2, it acts and comes back: not a plug-in's automation), or none (the
    FM-1's alone: SPK/JACK, ANIMATIONS).
  - **The engine's state beside a scene** (KEY MODE, the MIDI channels, LOAD, CLOCK, the metronome):
    `frs_state_save` and `frs_state_load`.
  - **The running state** (Perform LIVE, card #131): `frs_live_save`, `frs_live_load`, `frs_live_ms`, what a start
    (a scene, the state, the moves) does not hold, so that an engine opened on the same start goes on as this one does:
    the brain whole, bit for bit, the sound's drift, envelopes and phases at half precision, not the buffers.
    `FRS_LIVE_BYTES` at most (some 1.5 KB for a busy scene); another engine's (its version, NPARAM, the scene's length)
    refused. `frs_ms_after(n)`: the clock once n frames more are rendered (a host that acts between two blocks).
  - **CLOCK INTERNAL:** an incoming MIDI clock and a host's tempo (`frs_host_tempo`) are not followed: the grid runs
    on TEMPO.
  - **LFO TARGET's choices** (`frs_lfo_targets`, `frs_lfo_target`): the setting each choice moves. 2.5's 46: the 40
    before, then MASTER FILTER's CUTOFF and RESONANCE and the four pans; 2.6's 47th, SOFT (new ones at the end: a
    saved TARGET keeps its setting).
  - **TRACKS** (MASTER FILTER's and LOFI TAPE's): `FRS_TRACKS_*` are its values (ALL, DRONE, GLINT, BASS, then the
    pairs), `frs_tracks_bits` the tracks each is on (`FRS_TRK_*`; DRONE has its cloud's return with it, unless
    ROUTING puts the effect before the cloud).
- **A scene** (a shard) is 619 bytes in the River's layout (`FRS_SCENE_BYTES`, "FRS}"): a shard from the River
  plays as it does there. Its name: its first 16 characters at 0 (NUL-padded, all 16 used: no NUL), the rest, 17 ..
  32, at 590 (`name_more`, only after a first 16 all used). Its tail, after "FRS{"'s 590 bytes: the name's rest (590,
  16 bytes), the LFOs' AMOUNTs (606, 12: LFO 1's four TARGETs, LFO 2's, LFO 3's), SWELL (618). A scene a platform kept
  from before goes through `frs_scene_over(scene, n)` first, in a buffer of `FRS_SCENE_BYTES`: 3.0.2's 590 ("FRS{": no
  name past 16, the AMOUNTs x1.00, SWELL 0), 3.0's 586 ("FRSz": LOFI TAPE II's four as the tape was too, 64, 64,
  0, 0, not their defaults), 324 ("FRSy": no drone's sequence too), 2.7's 323, 322, 319, 2.6's 297 bytes ("FRSt", its bass's sequence made over as the FM-1
  does) or 2.7's first test builds' 298 ("FRSu"); `P_BQ_ON`'s 1 becomes 2, the bass's arpeggio none. The River makes
  the older shards over before it gives them (2.5's 296 bytes, SOFT at 0; 2.0's 273). A user's preset keeps a name of
  16 (`FRS_PRESET_HEAD`). "FRS|" (3.0.2-b5's test builds, never released) has the same 619 bytes, its AMOUNTs in
  percent (-100 % .. +100 %): `frs_scene_amt_over(scene)` makes them multipliers (|v - 100|: +100 % x1.00, -35 % x0.35).
- **A stream's scenes** (`frasil_stream.h`): "FRS}", or an older layout's ("FRS{", "FRSz") made over as they are
  played, SWELL as it is then (their engines left it through a scene and an ANCHOR's way back): an old stream plays
  as it did. The file itself is not changed (its CRC stays). An "FRS|" stream is refused (its AMOUNTs in percent in
  its scenes, its state and its moves; only 3.0.2-b5's test builds made any).
- **ANCHOR's way back and a scene's MORPH:** SWELL glides back on ANCHOR's MORPH (as BASS TONE, the bass's SPACE,
  AUTO VIB), the AMOUNTs switch at its half with the TARGETs; on a scene's MORPH, SWELL is the scene's at once, as
  BASS TONE is.
- **2.5's effects as they sound** (`frs_live_fx`, `FRS_LIVE_FX` floats, its layout in the header): LOFI TAPE's mix,
  speed, wow, drift and flutter phases and drag; MASTER FILTER's type, cutoff, Q and tracks now; MASTER EQ's three
  biquads now; MASTERING's gain reduction; the output's peak and RMS, left and right. `frs_fx_response` draws the
  filter's and the EQ's curves from them, at the engine's rate. The floats are copied after each block, outside the
  DSP's render; on the FM-1's build (`FRASIL_FS_CONST`) the effects' part is 0, the output's levels alone.
- **CUT** (`frs_cut`, the FM-1's OCT- held and PLAY): RELEASE ALL, then the next block faded out and the one after
  it emptied: the echoes, the cloud's lines and the tape's loop (its tail: below, Later).
- **PLAY** (`frs_play`, 2.5's, a tap):
  - **Nothing playing at all:** the scene given last's three tracks (what RELEASE ALL or CUT let go, else its own
    notes and sequences from their first steps).
  - **What the transport stopped** (`frs_track_play`, `frs_play_all`), nothing else playing: that, back, as
    `frs_play_all(1)`; not the scene's notes over the player's.
  - **Else** the transport.
  - **Tapped twice quick:** the app times it, as the FM-1 does. `frs_play` returns 1 when something played as it was
    tapped; a second tap within `FRS_PLAY_TWICE_MS` (350 ms) of one is `frs_release`: RELEASE ALL, never RESTORE.
    PLAY held is `frs_release_all` (RELEASE ALL, or RESTORE with nothing playing).
- **The state's version** (`frs_state_save`): 3, 2.6's numbers. A 2.5 engine's (2) is read with its settings past
  its NSOUND (153) made one higher, a 2.0 engine's (1) with those past its NSOUND (130) 24 higher.
- **What sounds now** (`frs_live`, `FRS_LIVE` floats, engine/core.c `eng_live`'s layout): 2.6 added the 53rd, LOFI
  TAPE's drag under way (how much slower, 0..1, 1 for 8 %; the River's `web_live` and the FM-1's PERFORM frame have
  it too).
- **The voice budget** (2.5's shedding of drone voices when the FM-1's DSP runs late) is the FM-1's alone: it needs
  the FM-1's cycle counter. The engine alone and the River never shed, as in the browser.

## Later

- **Denormals in the reverb's float allpasses:** harmless on Apple silicon (its FPU flushes them). A VST on x86 will
  want FTZ/DAZ set around the render, or a flush there.
- **fastmath.h's `n << 23` with a negative n:** undefined behaviour in C, harmless on the compilers used. To be fixed
  in fm1-base, then copied here again.
- **An instance struct:** the engine's state gathered in one struct, so that one process can have several engines.
  Until then, one engine a process.
- **The grid on the host's bars:**
  - **What it would take:** a Start under a host's tempo putting the grid at the host's beat (24 x the beat), so
    that the patterns sit on the DAW's bars.
  - **Why not yet:** that is a song position in the brain (core.c), and the FM-1's path. With the grid moved
    alone, GLINT's sequence would still begin on the Start's step, and the bass's sequence would strike every step
    up to there at once.
  - **For now:** the grid's top is where the host starts or locates.

- **The grid's wrap and GLINT's steps** (in core.c, the FM-1's too): GLINT's step number wraps with the grid, every
  9600 clocks (100 bars). A sequence whose loop doesn't divide them (12 eighths: 144 clocks) jumps there, and two
  sequences begun together are no longer in phase after it. The fix, an unwrapped step count, is a change in core.c,
  to make with the FM-1's renders checked.

- **CUT's tail** (in dsp/, the FM-1's too): CUT empties the cloud's lines but not their damping filters, and not
  the mix's DC blocker and filters. What those kept comes back into the empty lines some 40 ms later, about 35 dB
  under what played, and falls with the cloud's decay. Clearing them in the cut (as `frasil_silence` does for PANIC)
  makes it silent; it changes the FM-1's CUT, so it goes into the firmware first. With it, the notes given during
  CUT's two blocks: the second block's panic empties the sound's queue, so a chord latched then (the brain has it)
  does not sound. The fix there: the voices, the bass and the glints cleared without the queue dropped.
- **Short steps in blocks or chunks** (in dsp/): MASTER FILTER's settling (4 chunks) and CUT's fade (a block) are
  counted in samples, so at 96 kHz they take half the time (1.3 ms, 2.7 ms). Harmless; `br_n` would make them the
  same time.
- **The settings' names** (`frasil_params.h`) are not prefixed (`P_*`, `NSOUND`, `NPARAM`): a plug-in's code with
  names of its own like them would want `FRS_`-prefixed aliases.

## On the FM-1

`app/app.c` defines `FRASIL_SHELL` and the engine's hooks (its clock, its header's words, GLINT SEQ's pages, MIDI
learn, a Program Change's slot), then includes `core.c`. The shell may reach into the engine's state (the screen draws
it all); the engine never reaches into the shell.

## Licence

MPL-2.0, Copyright (C) 2026 Styves Goupil. `dsp/fastmath.h` is fm1-base's (MPL-2.0, Copyright (C) 2026 Charles
Vestal), copied with its header. A program built on the engine may be closed. The engine's files stay MPL-2.0: a
changed one is published as such.
