# Changelog

## 3.3.1 (2026-11-13)

**New**

- **MIDI OUT**: FRASIL now has a fifth track, MIDI. Its keys play the instruments in your DAW over USB, while FRASIL keeps playing underneath. Your drones and a Logic or Ableton synth, together, from one FM-1.
- **How it works**: turn SELECT one step past LEAD to MIDI (the header turns violet). On the SMK-37: KNOB BANK, or pad 13. The MIDI track makes no sound of its own. It sends notes to your computer over USB.
- **Velocity and pressure**: real velocity, with four curves: SOFT, NORMAL, HARD, FIXED. Pressure as aftertouch: off, channel, or per key.
- **The MIDI OUT page**: EDIT opens it: channel, octave, velocity, pressure. It shows the keys you hold, the last note sent, and whether a computer is listening. Notes never hang, even if you change channel or track while holding a key.
- **Arpeggiator and sequencer**: MIDI gets its own. Their notes go to your DAW, in time with FRASIL.
- **The arpeggio, for your DAW**: MIDI ARP II adds GATE (5 to 100 %, or LEGATO) and SWING (OFF, 51 to 75 %). MIDI ARP III sets VELOCITY: as played, or fixed from 1 to 127.
- **MIDI clock out**: optional, so your DAW can follow FRASIL's tempo.
- **Your DAW's sound, back in FRASIL**: if your DAW sends its audio back to the FM-1 over USB, MIXER shows a fifth fader, RETURN, for its volume. Turn it with ALGORITHM, or K7 on the SMK-37. With nothing coming back, MIXER keeps its four faders.
- **On the SMK-37**: the pitch wheel, mod wheel and sustain pedal go out too, and its MIDI OUT socket carries the same notes.
- **Guide**: a new MIDI OUT chapter, with step-by-step setup for Logic Pro and Ableton Live.
- **SMK-37 Elite**: FRASIL now runs on the M-VAVE SMK-37 Elite. It gets everything in FRASIL 3.3: the drones, GLINT, BASS, LEAD, the sequencers, the cloud, scenes and presets.
- **The SMK-37's panel**: every screen uses the names printed on its panel. Its 37 keys play the notes, with velocity. Faders 1 to 4 set the DRONE, BASS, GLINT and LEAD levels, and K8 is the master volume. The mod wheel moves the selected track's sound; the pitch wheel bends the LEAD.
- **Pads**: 12 of the 16 pads work as buttons. They pick a track, open the LFOs, and answer OK or CANCEL.
- **Guides**: the online guide, the cheat sheet and the four books now come in an SMK-37 version. Choose SMK-37 at the top of a guide page.
- **Back up first**: before the first install, back up your keyboard with the backup tool. Its calibration belongs to your keyboard alone, and no download can replace it. The install page on frasilsynth.com shows each step.
- **Install**: then install your model's package from a terminal. The Firmware Hub's SMK-37 page will link to the steps.
- **SMK-37 Pro and MKE-P37**: the same keyboard as the Elite. Their packages are included, but not tested yet. Tell us if they work.

**Fixes and changes**

- **FM-1 owners**: update to get MIDI OUT.
- **MIXER**: more room at the bottom of the page.
- **Backups**: a 3.3.1 backup holds MIDI's sequences and doesn't restore on 3.3 or earlier.
- Fixed: no sound in some FM-1 emulators (Harbor, fm1-emu's apps, RemoteFM1).
- **RemoteFM1**: the release now includes FRASIL as an .rfm1 file, for apps that run FRASIL on a computer or a phone.

## 3.3 (2026-11-11)

**New**

- **FRASIL Learn** (learn.frasilsynth.com): the easiest way to learn FRASIL. Interactive lessons that follow what you play: connect your FM-1 to your computer in Chrome or Edge, and the page sets it up for you. WELCOME TO LEARN MODE shows on the FM-1's screen, then each step you play on the FM-1 gets its tick on the page, as you play it. The first lesson, GETTING STARTED, takes you through the drone, harmony, GLINT, the pages and the new LEAD. If you want more lessons (the LFOs, the sequencers, recording), click the heart on Learn's page, and I'll create them as there is demand for them.
- **LEAD**: a fourth track, for melodies played live on top of everything (no arpeggio, no sequence). Turn SELECT one step past BASS: the header turns rose. It sounds only while you hold its keys, MONO with a glide from note to note, or POLY up to 6 notes. Each voice is built like an analog synth: two oscillators whose WAVE goes from a saw through a square to a narrow pulse, or WHISTLE, a pure tone with a breath of air; DETUNE spreads the two apart. BREATH adds noise and DRIVE adds warmth, then a 24 dB low-pass filter with RESONANCE opens with its own envelope. Each voice drifts a little in pitch, as an analog one does, and VIBRATO makes it sing. Its own stereo delay bounces left and right in time with the tempo, and it sends into the cloud's reverb like the other tracks.
- **LEAD pages**: LEAD I, LEAD II, LEAD III and LEAD DELAY on EDIT; LEAD ADSR and LEAD FILTER on ENV.
- **LEAD presets**: twenty sounds, different from each other, some dry and some deep in reverb and delay. SAVE › WHAT › LEAD PRESET keeps your own.
- **LFO TARGETS**: 22 new targets move the LEAD's sound.
- **MIXER**: K4 is now the LEAD's level. The cloud's level moves to CLOUD III, K4 (LEVEL); hold OCT− and turn it to pan the cloud.
- **CPU line**: a thin line along the bottom of the screen shows how busy the FM-1 is. Yellow from 85 %, red from 92 %.
- **Guides**: the guide has a new home with every way to learn FRASIL: Learn, the online guide, and books as PDF and ePub for tablets, Kindle or Kobo, small e-readers like the Xteink X4, and print.
- **The LEAD everywhere**: shards keep the LEAD in the River, the editor and STREAM. Perform shows it, and in LIVE your listeners hear it.
- **Screen reader support** for visually impaired players: the FM-1 tells fm1-reader what is on its screen.

**Fixes and changes**

- **GLINT keys**: on the KEYS page, GLINT's keys sound while you hold them, whatever LATCH says.
- **Changing track** keeps you on the page you are on. The large track name shows on the play page only.
- **MIXER**: DRONE, BASS and GLINT levels no longer step when you turn them fast.
- **TAPE SPEED**: no more crackle at slow speeds.
- **BASS PULSE**: no more click on a note that is still sounding.
- **Scenes**: 3.3 saves scenes with the LEAD. Scenes, shards and backups from earlier versions load as before, the LEAD on its first preset. An FM-1 on 3.1 or 3.2 still loads 3.3 shards, without the LEAD.

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
