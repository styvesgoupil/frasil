# Changelog

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
