# Changelog

## 1.1

- BASS has its own step sequencer (SEQ's two new pages, BASS SEQ and BASS SEQ II): 16 steps, each a NOTE, a CUT or a
  HOLD and as long as you want it (1/16 to 4 BARS); written step by step or live (REC with the keys on BASS),
  transposed by the keys; each scene keeps it with its SEQUENCE, STEPS and TRANSPOSE; PLAY starts and stops it with
  GLINT's, on the same grid. Its sequences are kept apart (a new store object at 0xD8000): the scenes stored before
  load as they are, their bass's sequence empty.
- PLAY restarts the beat: when it starts the transport, and when it is held (RELEASE ALL, then RESTORE), the grid
  starts again from its top there, as a MIDI Start does: OCT's blink, REC's flash and the metronome, the LFOs on the
  grid, PUMP, the arpeggio, GLINT's and the bass's sequences (from their first steps, at once) and PULSE all begin
  together. Following a MIDI clock, PLAY leaves its beats (the sequences start on its next step, as before). A tap that
  stops changes nothing.
- SCENES: SELECT scrolls the list as K1 does (NEW SHARD, then 01 to 16), and no longer changes KEYS there; a page
  button held with it still turns that button's pages.
- BASS ADSR (SEQ's third page, BASS III before) is the bass's envelope in ADSR's order: ATTACK, DECAY (new, 10 ms to
  10 s, while the note is held), SUSTAIN (new, the level held: 100 %, as it starts, keeps the bass as it was, DECAY
  doing nothing until it is lowered), RELEASE. Each note played rises again from where the bass is.
- PULSE (BASS II, K4: OFF, 1/8 BEAT to 4 BEATS): the held bass note let go a moment and played again on each division
  of the grid (MIDI clock, else TEMPO), from its ATTACK, the same note and velocity, no glide, its pressure kept. The
  gap: a quarter of the division, 8 to 60 ms. With the bass's sequence, inside its NOTEs only (never in a CUT; a step
  beginning with a NOTE is its own attack). Scenes and settings stored before come over at power-on (or restored from a
  backup) with SUSTAIN at 100 % and PULSE OFF, a CC learned on a setting kept on it; the River makes earlier shards over
  the same way.
- GLINT's pages, redone: GLINT II is OCTAVE, RATIO, PING, VIBRATO; GLINT III is GLINT ADSR, its envelope in ADSR's
  order: ATTACK, DECAY (to silence, or while the key is down to SUSTAIN's level), SUSTAIN and RELEASE (new, 20 ms to
  6 s: how long a glint rings on once its key is let go; DECAY set it before). Scenes and shards from before get the
  RELEASE their DECAY made (DECAY's time up to 0.6 s, else half of it, 0.6 s at least) and sound as they did; RELEASE
  starts at 600 ms, and the GLINT presets set it too.
- SCATTER is gone (it was on GLINT III): each glint's place in the stereo field and a slight detune, at random,
  inaudible on the FM-1's mono speaker. Each glint keeps its default spread, fixed: a little apart in the field and in
  tune. A CC learned on SCATTER is forgotten.
- BASS ADSR and GLINT ADSR draw the envelope in place of the river: ATTACK up to the top, DECAY down to SUSTAIN's
  level, SUSTAIN's plateau, RELEASE down to nothing, each part as wide as its knob is turned (the plateau what is
  left), its time across it, A D S R under it, the part being turned lit. Each note sounding is a glint riding it (the
  bass's in lamp, up to four of GLINT's in ice), waiting at the plateau's end while held, then down RELEASE once let go.
- SWELL is ENV's hold now (ENV is the drone's page, and SWELL swells the drone's voices): held 0.4 s, every drone voice
  swells together (1.5 s up, 1.5 s down once let go), ENV lit while it lasts. SEQ held no longer swells (with SELECT,
  it still turns its pages); the help says ENV HOLD: SWELL. GENERATIVE FX's SWELL and CC 11 as before.
- The bass's RECORD LIVE takes what you play as you play it: each note's start and length to the nearest sixteenth,
  over the time it covers and nothing else, the steps written again around it in as few as the STEP LENGTHs make it (a
  5/16, a NOTE of 1/4 and a HOLD of 1/16); a note sounds at once, the sequence's own steps silent under it, and the
  strip draws it as it is held. Over a sequence the take goes round its loop, the loop's length kept; on an empty one
  it makes the loop, from the first note's bar to the bar line nearest REC again (the metronome high on each bar line
  meanwhile), STEPS set. Sixteen steps at most: a note that doesn't fit goes in legato if that is enough, else not at
  all (BASS SEQ FULL).
- Recording LIVE, REC's metronome counts the sequence's own beats from its first step, the higher tick as it comes
  round to it (not the grid's bar any more), and REC flashes on them; on STEP, no metronome, REC lit. RECORD on STEP,
  as on LIVE, turns the keys to its track at once (RECORD OFF gives them back); the bass's STEP carries the STEP
  LENGTH on from note to note (K3 shows and sets it). While a sequencer records, its strip is ruled, faintly: a line
  each whole tone, a column each step, the one being written lit.
- Back up and restore over USB (fm1-base's backup, turned on): the 16 scenes and the settings to a file and back,
  never the panel's calibration. In the browser, the River (https://river.frasilsynth.com/, Chrome or Edge): each slot
  a card (what plays, GLINT's sequence or arpeggio, every page's settings as the screen writes them); download
  everything or the scenes you select; put a backup back whole, or scenes into the slots you choose, several at once. From a
  terminal, fm1-base's `fm1_backup.py`. A restore waits until nothing plays (hold PLAY; STOP FIRST otherwise), checks
  the whole file before anything changes, then asks on the screen: RESTORE FROM USB? SEL yes, FX no.
- A backup's start saves scenes and settings still kept in an earlier FRASIL's layout in this one.
- The store: the scenes' record is FRSl, a shard FRSm, the settings FRSJ; the earlier ones (1.0's too) are made over
  at power-on and at a restore, and the River and its server read and convert them.
- While a backup is sent, STORE doesn't happen (a pop-up says so); a metronome or a CC learned meanwhile is kept for
  after.
- After a restore from USB was tried, the screen comes back where it was.
- FRASIL River, redone: its own header (FRASIL River, the community shard library) and three parts,
  Your Private River, the Community River and Tools (Quick Backup), and a profile; shard names of sixteen; its words say
  what they do (SELECT, Download everything, Save to my River). The main site's header: Overview, Guide, River library
  (a page on what the River is), Play, Install on FM-1, the same on every page.
- Scenes have names of your own, up to sixteen characters: right after STORE, NAME IT (OCT−/OCT+ the cursor, K4
  letters, K3 digits; SAVE or SEL stores it, FX leaves the proposed one). Scenes kept in an earlier layout come over
  with their names.
- SCENES, redone: the whole screen a list of five slots around the one K1 picks, each with its name and what it holds;
  the river gone from it; its two actions, PLAY LOAD and SAVE + REC STORE, in a box of their own.
- NEW SHARD, SCENES' first line, above slot 01 (K1 below slot 1), framed in lamp: not a slot, a sound to start a new
  shard from, fresh. A calm drone in A minor, SEVENTH open (Am7 on A): CUSTOM, BODY and MOTION at half, the three LFOs
  on (slow sines, two settings each), GLINT's SINE dim, a SUB under it, no notes. PLAY loads it as any scene (the
  header then says NEW SHARD); nothing is stored into it (CHOOSE A SLOT): STORE from another page puts it in the first
  EMPTY slot, then NAME IT.
- Hold a page button (FX, SEL, ENV, LFO, EDIT, GLO, HOME, ARP, SEQ) and turn SELECT: its pages, both ways, round,
  the first detent from another section's page setting KEYS as a tap would; let go, the button does nothing more. Its
  hold, if it acted first (FREEZE, ARP LATCH, COMPARE, SWELL), is undone.
- KEYS follow the section, when you come from another section: ARP's pages put the keys on GLINT, SEQ's on BASS,
  EDIT's, ENV's and HOME's on DRONE, by a tap, a page button held and SELECT, or HOME; the others (FX, GLO, HARMONY,
  LFO's pages with DRONE MOTION, SCENES) leave them. Within a section they never change (ARP again, HOME's MIXER and
  logo, SELECT with the button held), so KEYS turned by hand stay until another section; SAVE back from SCENES comes
  back as it left. The keys' badge shows it, with no words over the page's name; a recording keeps them.
- GLINT's sequence, RECORD OFF: on GLINT SEQ's pages the keys play glints, as with SEQ OFF (try a line before you
  record it; the sequence goes on, TRANSPOSE untouched). On any other page a key transposes it as before and, while
  it plays, starts it again from its first step on the nearest beat: in a beat's first half from the beat just gone,
  transposed at once, catching up; in its second half on the next beat, transposed then. Stopped, it only transposes.
- ARP's first page is GLINT I (it was GLINT PARAMS).
- ARP held turning LATCH on puts GLINT's SEQ (GLINT I, K2) on ARP too, if it was OFF or on SEQ, as turning it there
  would (the header: ARP LATCH, ON, SEQ ON ARP); turning it off leaves SEQ as it is. SELECT turned with ARP held undoes
  that change with the LATCH (its sequence and RECORD as they were).
- GLINT I's TYPE is SEQ (K2: OFF, ARP, SEQ), the switch that puts GLINT's sequencer on.
- SCENES' UNDO is gone (K4 is empty): STORE, NAME IT and NEW SHARD stay as they were.
- MORPH is back on CUT at every power-on (LOAD is still kept).
- MIDI CC 120 (All Sound Off), on any channel, lets everything go and stops the sequences, as PLAY held (hold PLAY
  again: RESTORE); 123 stays the track's notes off. The River sends it before it restores: no more hold PLAY first.
- The play page's strata: their badges' digits inside the discs, the rows apart.
- While NAME IT or a pop-up is up, a turn of an encoder never reaches the page under it: on SCENES, K3 could turn
  LOAD.
- Clearer words: SCENES' KEEP is now LOAD, SND+NOTES (the sound and the scene's notes) or SND ONLY (the sound alone, what plays
  goes on); KEYBOARD is KEYS, FULLNESS is BODY, and GLOBAL's KEYS is KEY MODE; GLINT SEQ II's LENGTH is SEQ LENGTH,
  BASS SEQ's (a step's) STEP LENGTH; GLINT SEQ III's UNDO is UNDO TAKE.
- SINK on the screen: a clear blue veil from the top, smooth and see-through, fading at the sides, and as faint as the
  river behind a page (it was a dark block over it, as strong on every page).
- The logo's screen: its frasil no longer leaps ahead as a pad comes in, nor races back as it fades; the water under
  the logo a colour a row, without the flickering grain (mauve dots as its swells went through).
- The play page's header: the scene's name alone, as large as it fits (its number gone); beside a long name, FREEZE
  is a flake of ice. The four strata's badges no longer touch.
- Pop-ups: a warning (STOP FIRST, NOT RESTORED, NOT STORED, MIDI LEARN: TURN ONE FIRST) or a restore's result takes
  the whole screen until SEL (its one button, OK, and the only light left on), which only closes it; the keys
  and MIDI play meanwhile, the other buttons and the encoders wait. The gestures' words (STORED, FREEZE ON …) stay in the header.

## 1.0 (2026-10-23)

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
