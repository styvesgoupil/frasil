# Changelog

## 3.2 (2026-11-08)

**New**

- **LIVE**: play the FM-1 live for listeners with Perform (perform.frasilsynth.com). Everyone hears the sound as the FM-1 plays it, with its LFOs, random arpeggios, sequences and clock in step, and someone who joins late is in step at once. You can plan the live at a later time and if you made it public it will be visible in the home page of River.

- **River**: completed redesign of River, with featured songs (new, most liked, most played will have a role determining which one to feature at each visit).

- **LFO TARGETS**: all the remaining parameters of the tape FX join them.

- **TEMPO (EXT)**: when an external MIDI clock comes in, GLO > TEMPO says so and shows the tempo it receives (your DAW sends 80, you read 80). The value is greyed and K1 leaves it alone. When the clock stops, or CLOCK is set to INTERNAL, TEMPO is yours again, at the value you had set.

- **BATTERY**: level indicator on the screen.

- **SCREEN**: darker background, more contrast, easier to read.

**Fixes and changes**

- **Newer FM-1s**: FM-1s sold since autumn 2026 have a different memory chip, and 3.1 did not recognise it. On those, scenes and settings were lost at power-off, backup and restore were refused, and updates from the Firmware Hub stopped. 3.2 recognises the new chip, so scenes and settings are kept, and backup, restore and updates work. If the update to 3.2 stops on such an FM-1, hold OCT− and OCT+ for 10 seconds to enter UPDATE MODE and install it from there.

- **DELAY** stays steady when the tempo comes from an external MIDI clock.

## 3.1 (2026-11-06)

**New**

- **ANCHOR**: like Elektron's temp save and reload, with a glide back, as the Octatrack's scenes morph. Press OCT− and OCT+ together to hold the sound as it is. Change anything, have fun performing and stretching the sound to its limits, then press OCT− and OCT+ again to go back smoothly, automatically, in an 8-second morph (or hold them to jump back at once).
- **TAPE SPEED**: the whole sound slowed or sped up like a tape, from ×0.25 to ×2.00: pitch, tempo, LFOs and envelopes together (reverb and delay stay as they are). A hundredth a detent, and between the tape's reels the screen shows the change in tones whenever you land on a half-tone (-1.5 TONES, +1 TONE). ×1.00 plays as before.
- **LOFI TAPE II**: a new FX page after LOFI TAPE, there while TAPE is on: TAPE SPEED, TONE, NOISE (hiss) and DROPOUT. Kept in scenes; scenes from before keep their sound.
- **LFO AMOUNT**: every LFO target has its own amount, an attenuator that can also boost: from ×0.00 to ×5.00 (×1.00 is the LFO as before). Hold SEL and turn K1–K4: the amount shows large in the header, over a bar that fills from its middle. Kept in scenes.
- **LFO TARGETS**: TAPE SAT and WOBBLE join them, 57 in all.
- **SCENE NAMES**: support for 32 characters, kept whole on the FM-1 and in the River, the editor and STREAM.
- **TUNE A4**: the pitch the whole FM-1 is tuned to, from 415 to 466 Hz (440 as before, 432 one turn away). GLO > RECORD, kept on the device.
- **HEADER**: GLO > SCREEN chooses the header's look: INVERT (the band in the track's colour, as before) or DARK (the band dark, its words in the track's colour). Kept.

**Fixes and changes**

- **Scenes**: 3.1 saves them in a new layout. Scenes, shards and backups from earlier versions load as before, the new settings at their defaults. A 3.1 backup does not restore on 3.0: `tools/backup_for_3_0.py` makes a copy that does.
- **MASTER EQ** turns smoothly: no more crackle while HIGH (or MID, LOW, a preset) moves.
- **CUT then PLAY** brings every track back: the drone's held chords and GLINT's arpeggio too, not the bass alone.
- **SWELL** is kept in the scene: STORE keeps it, a scene loaded sets it, ANCHOR glides it back.

## 3.0.1 (2026-11-02)

- **The screensaver's corner, celebrated.**
- **IDLE TIME**: how long before the screensaver, 1 to 60 minutes (GLO, SCREEN).
- **The screensaver's logo drifts twice as fast.**
- **Scene names up to 32 characters** on the FM-1, and a name too long for the header rolls slowly along and back. The River and the iPad still show the first 16. A 3.0.1 backup no longer restores on 3.0: `tools/backup_for_3_0.py` makes a copy that does.

## 3.0 (2026-11-02)

**A complete controls redesign, more intuitive.** Three tracks, each with its own keys, sound, arpeggio and sequence.

**The basics**

- **SEL**: pick the track: DRONE, GLINT or BASS. The keys, the header's colour and every page below follow it.
- **HOME II (KEYS)**: how the keys play the track: SINGLE NOTE or CHORD, in the scale or not, LATCH, STACK.
- **EDIT**: shape the selected track's sound.
- **ARP**: the selected track's arpeggiator.
- **SEQ**: the selected track's sequencer.

**New**

- **Your own presets**: save your DRONE, GLINT and BASS sounds, sixteen of each; they go into backups too.
- **Recording, far simpler**: REC from any page, realtime (LIVE) or step by step (STEP), a bar counted in from silence.
- **The drone and the bass can be sequenced**: the drone's chords a step at a time, the bass's notes, up to 32 steps each.
- **The bass has the arpeggiator**, not just GLINT.
- **BASS TONE**: DRIVE, WEIGHT, PUNCH, BITE.
- **ARP SNAP**: a new chord joins an arpeggio on the beat, the hand gathered first (GLO, GLOBAL; 1/4 at first, OFF as before).
- **STACK**: hold SEQ, and new notes and chords join what is held instead of replacing it.
- **Better UX to browse long lists**: presets, an LFO's targets and scales on one screen while you turn, heard as you stop on one; SEL keeps, FX goes back.
- **CHORD in the header**: CHORD · GLINT while a track's keys play chords.
- **AUTO VIB**: a vibrato of its own on every glint.
- **EDIT held**: three black keys jump to the track's pages.
- **CLOUD SEND**: each track's reverb send on one page.
- **SCREENSAVER**: after ten minutes untouched the screen goes black and the logo drifts; any control wakes it (GLO, SCREEN).

**Fixes and changes**

- **KEY MODE**: now each track's LATCH, on KEYS.
- **DRONE MOTION**: now EDIT's second page; LFO opens the three LFOs.
- **MASTER FILTER's CUTOFF**: now GLO held and ALGORITHM turned.
- **CLEAR**: hold OCT-, press REC on a SEQ page; no longer a knob.
- **SAVE**: LOAD or SAVE first, SEL to go on, SAVE twice to store; SAVE + REC still stores the scene.
- **RECORD STEP**: STEP stays where it is; K1 moves it.
- **The octave**: shown large when you change it, no longer in the header.
- **The metronome**: hold REC 0.8 s.
- **The River**: shards go into an FM-1 on 3.0; update first.

Scenes and backups from earlier versions convert on their own. A backup from before 3.0 holds no presets: yours stay.

**FRASIL River**

- [river.frasilsynth.com](https://river.frasilsynth.com/): back up your FM-1 and keep your scenes, called shards, in Your Private River; share them in the Community River.
- A shard plays in the browser with its LUCID and DREAM pictures; SEND TO FM-1 puts it into an FM-1 on 3.0.

**FRASIL Editor, new**

- [editor.frasilsynth.com](https://editor.frasilsynth.com/): shape a scene track by track in the browser: open it from your FM-1, the River or a file, save it back to any of them.

**FRASIL Perform**

- [perform.frasilsynth.com](https://perform.frasilsynth.com/): a realtime second screen for your FM-1 on 3.0: what plays, drawn large, over Web MIDI.

## 2.6 (2026-10-31)

A softer choir, its wave on the screen, and PERFORM's DREAM.

**New**

- **SOFT** (CHOIR III, a new EDIT page): rounds the choir's wave off, to a triangle, then near a sine, at the same level. Older scenes sound as before.
- **CHOIR II and III**: the choir's wave drawn above the page, as you hear it.
- **PERFORM's DREAM** (perform.frasilsynth.com): a second view, light only, full screen, moved by what you play.

**Fixes**

- **SHAPE**: the screen says SQUARE in the middle, where the square is, and PULSE at the end.
- **A voice gone wrong** is started again at once: it can no longer break every scene until a restart.

## 2.5.1 (2026-10-30)

A fix for heavy scenes.

**Fixes**

- **Chord changes**: on a heavy scene, a new chord taken while the last one rang out glitched for a moment. The engine now makes room first: the old chord's tail ends sooner, the sound never breaks.

## 2.5 (2026-10-30)

Three new effects, clean recording over USB, and new ways to start and stop a scene live.

**New**

- **MASTER FILTER**: a low-pass or high-pass with RESONANCE, on the tracks you choose.
- **LOFI TAPE**: tape saturation and wobble on the tracks you choose, drawn as a turning cassette.
- **MASTER EQ**: LOW, MID and HIGH bands on the output, with ready-made presets.
- **ROUTING**: put FILTER and TAPE before or after the cloud; turn CLOUD PUMP off.
- **PAN**: on MIXER, hold OCT- and turn a knob to pan that track.
- **PLAY and CUT**: from silence PLAY plays the whole scene; tap twice to release all; hold OCT-, tap PLAY to cut every sound.
- **Silent preset picking**: hold OCT- and turn a preset; let go of OCT- to hear it.
- **SPK/JACK**: turn off the speaker and headphones while the USB audio keeps going.
- **CLOCK**: INTERNAL keeps your TEMPO and ignores an incoming MIDI clock.
- **ANIMATIONS**: LOW keeps the screen quick in heavy scenes.
- **PIANO**: a new lofi upright piano preset for GLINT.
- **PERFORM**: a full-screen web page that shows your playing live, for a stage or a video.

**Fixes and changes**

- **USB audio**: recording over USB no longer crackles ([#1](https://github.com/styvesgoupil/frasil/issues/1)).
- **Power-on**: nothing plays by itself any more; tap PLAY to start the scene.
- **Performance**: a big optimisation pass keeps complex scenes stable, without thinning out or glitching.
- **No clicks**: when changing an effect's type or routing, or turning the metronome on or off.
- **COMPARE**: now hold OCT-, then EDIT.
- **GLINT's delay**: always ping-pong.
- **Lights**: PLAY follows the transport, REC glows faintly at rest, the top keys stay dark.
- **Screen**: the keys' badge always says DRONE, GLINT or BASS; DRONE PARAMS is now DRONE TONE.

Scenes and backups from earlier versions convert on their own.

## 2.0.1 (2026-10-24)

Two timing fixes for the bass.

**Fixes and changes**

- **PULSE**: strikes right on the beat instead of dragging behind it.
- **Startup**: a scene loaded right at power-on now starts on the beat.

## 2.0 (2026-10-24)

A major release since 1.0: a bass sequencer, rethought buttons, named scenes with their own tempo, and the River.

**New**

- **Bass sequencer**: 16 steps of NOTEs, CUTs and HOLDs, written step by step or played live.
- **PULSE**: strikes the held bass note again on the beat.
- **OCTAVE**: BASS's and GLINT's OCTAVE move every note the voice plays, at once.
- **Envelopes on screen**: BASS ADSR and GLINT ADSR are drawn as you turn them.
- **Named scenes**: up to sixteen characters, with NAME IT right after STORE.
- **TEMPO per scene**: each scene keeps its own TEMPO.
- **NEW SHARD**: a fresh sound at the top of SCENES to start from.
- **Seven factory scenes**: all new, made on the FM-1, each at its own tempo.
- **The River**: back up, restore and share your scenes at river.frasilsynth.com.
- **LISTEN**: the site's front page plays the factory scenes live.

**Fixes and changes**

- **Page buttons**: hold one and turn SELECT to walk its pages; SWELL is now ENV held.
- **KEYS follow the section**: ARP's pages play GLINT, SEQ's play BASS, EDIT's play DRONE.
- **Pop-ups**: SEL answers yes, FX answers no; only those buttons light.
- **Plainer words**: LOAD, KEYS, BODY, KEY MODE, SEQ STEPS.
- **Sequences start together**: from their first step when PLAY starts or a scene loads.
- **PLAY held from silence**: brings the whole scene back, sequences included.
- **PULSE keeps the beat**: a bass note changed by hand waits for the next beat.
- **Empty sequences**: the keys play their sound instead of nothing.

Scenes and backups from earlier versions convert on their own.

## 1.1

The bass gets its own sequencer, scenes get names, and the River backs them up.

**New**

- **Bass sequencer**: 16 steps of NOTEs, CUTs and HOLDs, written step by step or recorded live.
- **BASS ADSR**: the bass's envelope, with a new DECAY and SUSTAIN.
- **PULSE**: strikes the held bass note again on each beat division.
- **GLINT ADSR**: GLINT's envelope, with a new RELEASE.
- **Envelopes on screen**: both ADSR pages draw the curve, each note riding it.
- **Backup and restore**: all 16 scenes and the settings to a file over USB, and back.
- **The River**: river.frasilsynth.com shows, downloads and restores your scenes from the browser.
- **NAME IT**: name a scene right after STORE, up to sixteen characters.
- **NEW SHARD**: a fresh sound at the top of SCENES to start from.
- **Page buttons**: hold one and turn SELECT to walk its pages.

**Fixes and changes**

- **PLAY restarts the beat**: sequences, LFOs, PUMP and the arpeggio start together.
- **SWELL**: now ENV held instead of SEQ held.
- **KEYS follow the section**: ARP's pages play GLINT, SEQ's play BASS, EDIT's play DRONE.
- **SCENES**: a full-screen list of slots with their names; UNDO removed.
- **MIDI CC 120**: releases everything and stops the sequences, like PLAY held.
- **Plainer words**: KEEP is LOAD, KEYBOARD is KEYS, FULLNESS is BODY.
- **Pop-ups**: warnings fill the screen until SEL; the knobs no longer reach the page beneath.
- **SCATTER**: removed from GLINT.

Scenes and backups from earlier versions convert on their own.

## 1.0 (2026-10-23)

The first release.

**New**

- **Drones**: up to 8 voices of CHOIR, PARTIALS and BREATH, latched by a tap, swelled by pressure.
- **CLOUD**: a long reverb with shimmer and FREEZE.
- **Harmony**: scales, chords and voicings; a single note plays a chord.
- **Drone presets**: 24 presets, plus CUSTOM for your own sound.
- **LFOs**: three of them; hold LFO and turn a knob to route it.
- **GLINT**: icy FM bells and leads, with an arpeggiator and a 32-step sequencer.
- **BASS**: one deep held note under the drones, with eight MODELs.
- **GENERATIVE FX**: SWELL, SPARKLE, BLUR and SINK.
- **MIXER, PUMP, MASTERING**: faders with meters, ducking on the beat, a compressor on the output.
- **Scenes**: 16 slots, loaded at once or MORPHed, by PLAY, a white key or Program Change.
- **The screen**: RIVER, the play page; a badge on each setting for its knob; help with HOME held.
- **MIDI**: over USB and TRS, a channel per track, MIDI learn and clock.
