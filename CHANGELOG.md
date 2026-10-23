# Changelog

## 1.0 (not released yet)

The first release.

- Drones: up to 8 voices, three layers each (CHOIR, PARTIALS, BREATH), latched by a tap, swelled by pressure;
  MOTION (a slow drift) and TIDE (a swell of the timbre over minutes).
- CLOUD: an 8-line reverb with in-loop diffusion, pre-delay, shimmer (its interval and brightness on CLOUD III) and
  FREEZE; tails up to 60 seconds.
- Harmony: 12 scales, 8 chords built in the scale and 7 fixed ones, three voicings; a single note over a chord.
- The layers' second pages (EDIT): CHOIR II (SHAPE saw to square to a narrow swaying pulse, the centre saw's
  INTERVAL, RESONANCE up to Q 20, the filter's TYPE: low, band, high), PARTIALS II (EVOLVE, BEAT pulsing each
  partial, a FORMANT and its PEAK down to one overtone alone), BREATH II (GRAIN, the resonators' PITCH, the noise's
  TONE, AIR).
- The drone's presets: PRESETS turns through 24 (MIST, CATHEDRAL, GLACIER, ABYSS, AURORA, EMBER, TIDE, DUST, WIND,
  ORGAN, HOLLOW, VOWEL, SHIMMER, CHANTER, ENSEMBLE, BELLS, NOCTURNE, HAZE, PULSE, RIME, DEEP, SPECTRAL, RUST, STILL)
  and CUSTOM, your own: a setting turned, the drone becomes CUSTOM, which another preset doesn't lose; a scene keeps
  it. DRONE PARAMS (EDIT's first page): TIMBRE, SPACE, MOTION and FULLNESS (the drone thinned without its settings
  changed, out of the presets; ALGORITHM turns it too).
- Three LFOs (on LFO, after DRONE MOTION): seven shapes, a rate free or on the grid (MIDI clock or TEMPO), depth,
  polarity, four targets each among 40 of the drones', GLINT's, the bass's, the mixer's and GENERATIVE FX's
  settings, each in its track's colour, or routed from any page (LFO held, an encoder turned); free running; the
  gauges show where they have a setting now; saved in scenes.
- GLINT: icy FM bells over the drones, and leads: 22 presets, SUSTAIN (held while the key is down), pressure
  (louder, longer, and VIBRATO: the finger's wiggle on the pitch). TYPE: each key its glint, an arpeggiator with
  latch and chance, on the RATE grid, following MIDI clock or TEMPO, or a 32-step sequencer (STEP and LIVE
  recording, REC straight into it, accents, ties, GATE, LENGTH, transposed along the scale by the keys, a NEW TAKE
  and UNDO, PLAY to stop and start it, one per scene); OCTAVE moving both as they play; a tempo-synced delay, mono or
  ping-pong.
- BASS: one deep held note under the drones: eight MODELs (SINE, TRIANGLE, SQUARE, SAW, REESE, SUB, ORGAN, FM),
  DRIVE, a resonant low-pass, glide; dry; its three pages on SEQ.
- GENERATIVE FX (FX's first page): SWELL (every voice swelled, held where it is turned), SPARKLE (glints by
  themselves on the drones' notes), BLUR (the drone dissolved into the cloud), SINK (a low-pass on the whole mix).
- SELECT chooses what the keys play: DRONE, GLINT, BASS (KEYBOARD). A press shows, a hold acts: the buttons act when
  let go, and held, HOME opens the help, ARP latches, SEQ swells, FX freezes, REC turns the metronome on or off, EDIT
  compares with the scene as saved, PLAY releases everything (and held again, restores it). REC records into GLINT's
  sequencer, its light blinking meanwhile. FREEZE is FX held, CLOUD's K4 and MIDI CC 80. The function buttons glow,
  bright when chosen; SAVE bright while the sound has changed; LFO dimmer while no LFO runs; ARP flashing on GLINT's
  steps; PLAY glows, bright while anything sounds; OCT− and OCT+ glow, a little brighter on the beat.
- MIXER (HOME's second screen): DRONE, BASS, GLINT and CLOUD faders with live meters. PUMP (FX): a sidechain duck
  on the beat for each track. MASTERING (GLO): a compressor on the output, its gain reduction on screen.
- Scenes: 16 slots (8 factory scenes), each with its sound, latched drones, arpeggio, sequence and bass. Stored with
  SAVE held and REC, the last STORE undone on SCENES; loaded with PLAY on SCENES, SAVE held and a white key, or MIDI
  Program Change; at once or MORPHed over up to two minutes, KEEP letting what plays go on if you like. COMPARE (EDIT
  held) plays the scene as saved. The SCENES page says CHANGED while the sound differs from the one saved.
- The screen: RIVER, the play page (the voices as currents of ice, the drone's mix and WARMTH as strata, what runs
  in its corner); the scene's name in the header, the page's dots under its title; a badge on each setting for its
  control; the living logo screen; on-screen help (hold HOME).
- MIDI over USB and TRS: a channel for each track (drones 1, GLINT 2, BASS 3 by default; MIDI page on GLO; an
  earlier FRASIL's one channel stays the drones', OMNI becoming 1), the
  drones' notes through the harmony, sustain, polyphonic and channel pressure, CCs and MIDI learn, Program Change for
  the scenes, clock and start.

## 0.9.13

A build for the FM-1, not a release: what changed since 0.9.9.

- The three left encoders: SELECT is KEYBOARD (what the keys play: DRONE, GLINT, BASS), PRESETS the drone's PRESET,
  ALGORITHM its FULLNESS. TIMBRE, SPACE and MOTION, theirs before, are on DRONE PARAMS; TEMPO stays on GLOBAL.
- The buttons: a press shows, a hold acts. Each acts when let go (OCT− and OCT+ at once); held 0.4 s (PLAY 0.6 s),
  its other thing: HOME the help (on the page with the page's button), ARP the arpeggio's latch, SEQ SWELL, FX FREEZE
  (the page staying), REC the metronome, EDIT COMPARE (the scene as saved, while held: COMPARE · AS SAVED), PLAY
  RELEASE ALL. Used with something else (SEL and a key, SAVE and a key or REC, LFO and an encoder, GLO and a CC), it
  does nothing more when let go.
- PLAY: a tap is GLINT's transport (the sequence stops, or starts from step 1; the arpeggio waits, its notes kept, or
  goes on from the top; with TYPE OFF, HOLD PLAY · RELEASE ALL), and on SCENES it loads. Held: RELEASE ALL (the
  drones, the bass, the glints, the arpeggio, the sequence), the header counting down from 200 ms, a release then doing
  nothing; held again with nothing playing, RESTORE: what it let go comes back.
- SAVE opens SCENES, and again goes back. STORE is SAVE held and REC: into K1's slot on SCENES, else into the current
  scene (STORED 03 ABYSS); a free slot takes the drone preset's name. SAVE held and a white key loads that scene (the
  16 white keys, F2's first; lit meanwhile, the current one bright). SCENES: K2 MORPH (CUT, 1 S to 2:00: the settings
  that glide go there, the choices at once, a hand turned out of it; on SOUND and SCENES the header's arrow and name,
  a line elsewhere), K3 KEEP (SCENE; PLAYING: what plays goes on, the sound alone changes), K4 UNDO (the last STORE,
  turned twice); MORPH and KEEP kept with the settings at once. SAVE lit while the sound differs from the scene's.
  MIDI Program Change 0–15 on the drones' channel loads scenes 1–16.
- GLOBAL: TEMPO on K1, KEYS on K4, changing on a second detent the same way within 2 s (KEYS · TURN AGAIN), since it
  lets every note go.
- The drone's presets: 24, and CUSTOM, your own (a drone's setting turned: CUSTOM, which another preset doesn't lose;
  kept in the scene). Each sets the three layers (their levels, both their pages), DRONE PARAMS, DRONE SHAPE's ATTACK
  and RELEASE, DRONE MOTION.
- DRONE PARAMS, EDIT's first page: TIMBRE, SPACE, MOTION, FULLNESS (100 % the drone as set; down, less detune,
  fewer partials, little breath, narrower, less warmth, its body high-passed). FULLNESS stays out of the presets.
- REC records into GLINT's sequencer: TYPE on SEQ, the keys on GLINT, RECORD on LIVE, GLINT SEQ on screen (the logo
  screen closes), the sequence started if it was stopped; again, RECORD OFF, the keys back to what they played before
  REC (RECORD OFF · DRONE), the page staying. Its light at the mid level, flashing on the beat (twice as quick as OCT)
  while RECORD is on (STEP or LIVE). A metronome ticks each beat while it
  records, the bar's first higher; REC held turns it off or on (kept with the settings, at once).
- GENERATIVE FX, FX's first page (then CLOUD, CLOUD II, CLOUD III, PUMP, PUMP II): SWELL (not kept), SPARKLE (from a
  few glints a minute to about six a second, on the drones' notes, GLINT's OCTAVE up, one in three an octave more),
  BLUR (SPACE toward 90 %, a longer decay, more diffusion and modulation, the glints further in), SINK (12 kHz down
  to about 120 Hz, a little peak). SPARKLE, BLUR and SINK are kept in the scene, and LFO targets.
- GLINT's pages: GLINT PARAMS (PRESET, TYPE, LEVEL, BRIGHT), GLINT II (ATTACK, DECAY, OCTAVE, RATIO), GLINT III (PING,
  SCATTER, VIBRATO, SUSTAIN), GLINT DELAY, always the fourth (TIME, FEEDBACK, MIX, SPACE), then GLINT ARP while TYPE is
  on ARP, or GLINT SEQ, SEQ II (LENGTH, RATE, GATE, CHANCE) and SEQ III (TRANSPOSE, CLEAR, NEW TAKE, UNDO) while it is
  on SEQ; four pages with TYPE OFF. A page hidden under you: back to GLINT PARAMS.
- GLINT SEQ III: NEW TAKE (turned twice: every step a rest, RECORD on LIVE, the sequence started) and UNDO (turned
  twice: the sequence before the last take or CLEAR; again, back). REC keeps the sequence before its take for UNDO.
- TYPE (OFF, ARP, SEQ) in place of the arpeggiator's MODE OFF and SEQ: MODE is UP, DOWN, UPDOWN, RANDOM, DRIFT. The
  delay's TIME has its 12 values mono, then ping-pong (PP; 1/8D PP by default), in place of its MODE.
- GLINT's OCTAVE moves a running arpeggio and sequence at once: their notes kept without it, played with it. NOTE
  shows a step as it sounds.
- LFO 3, with its targets. The targets: 40, FULLNESS, GL CHANCE, SEQ LEN, SPARKLE, BLUR and SINK added; written in
  their track's colour (GLINT's ice, BASS's orange, the drone's and the mix's white).
- LFO held and K1–K4 turned, on any page: the setting under it onto LFO 1, 2, 3, then none, into the first free
  TARGET, a DEPTH at 0 brought up to 25 % (FILTER > LFO 2, > NO LFO, LFO 2 FULL, NOT AN LFO TARGET).
- MIDI: CC 12 FULLNESS, 13 the drone's PRESET, 14 SPARKLE, 15 BLUR, 16 SINK, 17, 18 and 19 the DRONE, GLINT and BASS
  faders. MIDI learn: a setting turned, GLO held, a CC on the drones' channel (CC 21 · SINK); 8 at most, the oldest
  replaced, kept with the settings at once, before the built-in ones (but 64, 120, 123).
- The header: on the play page the scene's number and name, an amber dot while the sound has changed, then the key;
  FRASIL on the logo screen alone. Under every title, a dot for each of its button's pages, in place of 2/7. CHANGED
  beside SCENES. An amber dot by the keys' badge while the processor lightens the sound. The badge says what GLINT's
  keys do: GLINT, ARP, SEQ, or REC in orange.
- The play page: four strata, WARMTH's embers the fourth (K4 lights it, no large value). SINK darkens the river from
  the top, BLUR thickens its haze. At the river's top right, only what is on: SEQ or REC, ARP or ARP LATCH, LFO and
  its three dots, SWELL.
- The lights: SAVE lit while the sound differs from the scene's; LFO in the glow while no LFO runs, at the mid level
  when one does; ARP flashing on each of GLINT's steps, off its pages.
- Help: the buttons' and the encoders' pages as they are now (FX GENERATIVE FX, hold FREEZE; LFO, hold and a K to
  route; EDIT, hold COMPARE; GLO, hold and a CC to learn; SAVE, hold and a key to load, and REC to store; PLAY START,
  STOP, hold RELEASE ALL; REC RECORD LIVE, hold METRONOME; KEYBOARD, PRESET, FULLNESS); HOME held opens it on the page
  with the current page's button.
- The store: earlier scenes and settings made over (the arpeggiator's MODE into TYPE and MODE, the delay's TIME and
  MODE into one TIME, the arpeggio's and the sequence's notes without the OCTAVE they were kept with). The settings in
  a new record, FRSG (MORPH, KEEP, the learned CCs), FRSF's made over, its metronome kept; scenes unchanged.
