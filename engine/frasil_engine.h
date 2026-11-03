/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's engine, alone: the instrument whole (its sound, engine/dsp/; its brain, engine/core.c), for a platform
 * without the FM-1's shell (an iPad, a plug-in, tests). Built from frasil_engine.c and the DSP's units (dsp/:
 * frasil.c, glint.c, bass.c, nuage.c), -ffp-contract=off (no fused multiply-add: the FM-1's samples, bit for bit, at
 * 44.1 kHz).
 *
 * One engine a process: its state is static. frs_init opens it (a second open while it is open is refused),
 * frs_close closes it. Two plug-in instances in one process cannot both have it.
 *
 * It has no thread of its own. Call it from one thread at a time, the one that renders (an audio thread takes the
 * UI's calls from a queue of its own), or serialize the calls. From any thread, at any time (open or not, while the
 * audio thread renders): frs_param_count, frs_param_info, frs_param_kind, frs_param_text (but for GLINT SEQ's NOTE
 * and BASS SEQ's NOTE, whose words carry the OCTAVE the audio thread puts on them: those from the thread that
 * renders; and DRONE SEQ II's NOTES, whose words are the step's on STEP), frs_version, frs_scene_new, frs_scene_factory, frs_scene_over, frs_lfo_targets, frs_lfo_target and frs_tracks_bits,
 * which read the engine's constant tables alone (into what the caller gives them), frs_fx_response (the floats it is
 * given alone), and frs_snapshot and frs_snapshot_fx, the views a UI reads. Nothing else: frs_param_get and frs_dirty
 * too read what the audio thread changes (frs_snapshot has the dirty flag).
 *
 * Its time: it renders blocks of FRS_BLOCK frames and runs its brain every millisecond between them (the sequencers,
 * the LFOs, the MIDI), as the FM-1 does. A call between two renders acts at once; what it plays sounds from the next
 * block. MIDI is taken at its millisecond: frs_midi's at the next one, frs_midi_at's at the one its frame falls in (so
 * a clock's beats keep their time whatever the host's buffer). A host's tempo and transport (frs_host_tempo) run the
 * grid as a MIDI clock and Start would. What a message plays sounds from the first block rendered after its
 * millisecond: 0 to FRS_BLOCK frames (5.8 ms at 44.1 kHz) after its frame.
 *
 * Built with -DFRASIL_FS_CONST (as the FM-1's DSP is, for the bench): 44.1 kHz alone, frs_init a second time does not
 * start the sound from nothing, and frs_panic silences the voices, the bass and the queued notes alone (no echoes, no
 * cloud's tail).
 *
 * The settings are the FM-1's, a byte each, 0 .. their max, numbered as frasil_params.h has them (P_EQ_LOW ...;
 * frs_param_info; the River's schema.json names them: "ids", "params"): a platform's UI takes them by name, the
 * numbers past NSOUND move when a setting is added (2.5: 23 higher than 2.0's; 2.6: one more; 2.7's numbered last, none
 * moved, P_BQ_LEN the bass's one RATE, P_BQ_ON its PLAYS: KEYS, ARP, SEQ as GLINT's P_SQ_TYPE; 3.0's after them, the
 * drone's sequence's P_DQ_ON .. P_DQ_STEP, 197 .. 205; then P_DQ_NOTES .. P_COUNT_IN, 206 .. 224: the drone's step
 * editing and UNDO TAKE, ARP II's LATCH, KEYS, METRONOME and COUNT-IN; P_ARP_SNAP 225, P_SAVER 226, none moved). A scene,
 * a shard, is FRS_SCENE_BYTES bytes in the River's layout (scene_t: its name, its sound, what it plays, its sequences,
 * its TEMPO); what is kept beside it, the engine's state (frs_state_save). */
#pragma once
#include <stdint.h>
#include "frasil_params.h"
#ifdef __cplusplus
extern "C" {
#endif

#define FRS_VERSION "3.0"                /* the FRASIL this engine is (VERSION's: engine/tests/run.sh checks them alike) */
#define FRS_BLOCK 256u                   /* frames the engine renders at a time (frs_render takes any count) */
#define FRS_SCENE_BYTES 586u             /* a scene, a shard ("FRSz", 3.0's: the drone's sequence last; 324 in "FRSy",
                                          * before it; 323 in 2.7's "FRSx", 322 in its "FRSw", 319 in its "FRSv", 298 in
                                          * its first test builds, "FRSu"; 297 in 2.6, "FRSt": frs_scene_over makes them
                                          * over; 296 in 2.5, 273 till 2.0, the River makes the old over) */
#define FRS_STATE_BYTES 64u              /* the engine's state beside a scene, at most (frs_state_save; 3.0's: 45, KEYS'
                                          * twelve and ARP SNAP with it; 32 till then) */
#define FRS_LIVE 53u                     /* frs_live's floats */
#define FRS_LIVE_FX 40u                  /* frs_live_fx's floats */
#define FRS_KEYS 32u                     /* keys down at once (frs_key_*: 0 .. FRS_KEYS - 1) */
#define FRS_TEXT 32u                     /* a setting's words, at most (frs_param_text) */

/* ------------------------------------------------------------------ life --- */
int frs_init(float fs);                  /* open it at the rate fs (Hz: 44.1 kHz .. 96 kHz; the FM-1's build, 44.1 kHz
                                          * alone), everything from the start, NEW SHARD taken, nothing playing. 0; -1
                                          * a rate it can't run at; -2 open already (frs_close first) */
void frs_close(void);                    /* silent, closed (another rate: frs_close, then frs_init) */
const char *frs_version(void);           /* FRS_VERSION as the engine was built ("3.0"; any thread, open or not) */
float frs_rate(void);
void frs_render(float *left, float *right, uint32_t n);   /* n frames, -1 .. 1 (the left's and the right's) */
void frs_render_i24(int32_t *lr, uint32_t n);   /* the same, as the FM-1 renders it: interleaved, 24-bit */
void frs_gain(float g);                  /* the output's level, 0 .. 1 (the FM-1's MASTER; 1 at the start) */
void frs_panic(void);                    /* silence at once, all of it: every note let go (the drones', GLINT's, the
                                          * bass's, MIDI's, HOLD's), the arpeggio and the sequences stopped, the MIDI
                                          * waiting dropped, the echoes and the cloud's tail gone */
void frs_cut(void);                      /* CUT (the FM-1's OCT- held, PLAY): RELEASE ALL, then the echoes, the cloud's
                                          * lines and the tape's loop emptied over the next two blocks, the first faded
                                          * out (no click). 2.5.1's, the FM-1's too: some 40 ms on, what the cloud's
                                          * filters kept comes back faint (some 35 dB under) and dies away; a note given
                                          * before the second block is dropped by the sound (the brain has it) */
uint32_t frs_ms(void);                   /* the engine's clock: the millisecond it runs next */
/* a recipe played from any place (a stream, FRASIL STREAM's radio). frs_tick_hook: fn first in every millisecond of the
 * brain, inside frs_render and frs_skip_to, there to call what a replay calls; from the thread that renders, never
 * frs_close, frs_init or frs_render inside it; 0 takes it off, and frs_init does. frs_skip_to: the brain alone from here
 * to the millisecond ms: the hook, the sequences, the LFOs, the notes kept up, nothing rendered: what plays there is what
 * a render to there would play, but the drones and the bass start from their attacks, the cloud, the echoes and the tape
 * empty, a GLINT struck in the skip gone. Ask 5 s early and render them unheard: the sound fills in. Each millisecond
 * costs what a block of a few frames does, nothing like a render */
void frs_tick_hook(void (*fn)(void *), void *user);
void frs_skip_to(uint64_t ms);

/* -------------------------------------------------------------- settings --- */
uint32_t frs_param_count(void);
int frs_param_info(uint32_t p, const char **name, uint32_t *max, uint32_t *def);   /* 0, or -1: no setting p */
/* what a setting is (frs_param_kind): one of these */
#define FRS_P_SCENE 1u                   /* kept in a scene: the sound (MASTER EQ, MASTER FILTER, LOFI TAPE, ROUTING, the
                                          * pans, CHOIR III's SOFT too), TEMPO, the bass's PLAYS, SEQ LENGTH, RATE,
                                          * TRANSPOSE, its arpeggio's MODE, RANGE, CHANCE, its GATE; the drone's
                                          * sequence's ON, RATE, SEQ LENGTH, GATE, CHANCE, TRANSPOSE (3.0) */
#define FRS_P_STATE 2u                   /* kept beside it (frs_state_save): KEY MODE, the MIDI channels, LOAD, CLOCK; KEYS
                                          * (3.0: P_KY_DR_HARM .. P_KY_BS_STACK), ARP SNAP (3.0: P_ARP_SNAP, a new chord into
                                          * an arpeggio taken on its grid; 1/4 at the start) */
#define FRS_P_MOMENT 3u                  /* kept nowhere, what plays now: FREEZE, SWELL, KEYS, MORPH, RECORD, STEP, ARP
                                          * II's LATCH (P_GA_LATCH, P_BA_LATCH: the arpeggios' latch as it is) */
#define FRS_P_STEP 4u                    /* the step STEP points at: NOTE, ACCENT, LENGTH, the sequences' (it changes with
                                          * STEP; the drone's NOTES and LENGTH, P_DQ_NOTES, P_DQ_NLEN) */
#define FRS_P_ACTION 5u                  /* an action, not a value: turned to 2 it acts and comes back to 0 (CLEAR,
                                          * NEW TAKE, UNDO TAKE: the drone's P_DQ_UNDO too); not a plug-in's automation */
#define FRS_P_NONE 6u                    /* no setting (its number kept), or the FM-1's alone (SPK/JACK, ANIMATIONS,
                                          * SCREENSAVER; METRONOME and COUNT-IN, P_METRO and P_COUNT_IN:
                                          * frs_metronome, frs_record_metronome, frs_count_in here, which they show) */
uint32_t frs_param_kind(uint32_t p);
/* LFO TARGET's choices (P_L1_T1 .. P_L3_T4: 0 OFF, then 1 .. frs_lfo_targets()): the setting choice t moves (frs_param_*'s
 * number), or -1 (OFF, or none t). 2.5's: the 40 before, then MASTER FILTER's CUTOFF and RESONANCE, the four pans;
 * 2.6's 47th: CHOIR III's SOFT */
uint32_t frs_lfo_targets(void);
int frs_lfo_target(uint32_t t);
/* MASTER FILTER's and LOFI TAPE's TRACKS (P_FL_TRACKS, P_TP_TRACKS): the setting's values, and the tracks each is on
 * (frs_live_fx's bits: FRS_TRK_DRONE the drones and the cloud's return with them, unless ROUTING has the effect before
 * the cloud) */
#define FRS_TRACKS_ALL 0u
#define FRS_TRACKS_DRONE 1u
#define FRS_TRACKS_GLINT 2u
#define FRS_TRACKS_BASS 3u
#define FRS_TRACKS_DRONE_GLINT 4u
#define FRS_TRACKS_DRONE_BASS 5u
#define FRS_TRACKS_GLINT_BASS 6u
#define FRS_TRK_DRONE 1u
#define FRS_TRK_GLINT 2u
#define FRS_TRK_BASS 4u
uint32_t frs_tracks_bits(uint32_t tracks);   /* a TRACKS value's bits (FRS_TRK_*) */
uint32_t frs_param_get(uint32_t p);
void frs_param_set(uint32_t p, uint32_t x);   /* as the FM-1's encoder or a CC sets it: a preset's sound, CUSTOM from
                                               * a drone's setting turned, a layer's PLAYS; the sequences' steps: STEP,
                                               * then NOTE, ACCENT, the note's LENGTH (P_BQ_NLEN, P_SQ_NLEN: 1 .. 32 steps,
                                               * written as HOLDs or TIEs after it, to SEQ LENGTH's end); an action turned
                                               * twice (x 2) */
void frs_param_edit(uint32_t p, uint32_t x);  /* as the River's editor sets it: a sound setting or TEMPO, a sequence's
                                               * step (STEP: the step's own NOTE shown); what the shard plays edited
                                               * once a step changes (frs_scene_edited) */
void frs_param_text(uint32_t p, uint32_t x, char *out);   /* x as the FM-1's screen writes it (FRS_TEXT bytes) */
uint32_t frs_state_save(uint8_t *out);   /* the engine's state beside a scene (FRS_P_STATE's settings, the metronome):
                                          * FRS_STATE_BYTES at most, its length (its version 3: 2.6's numbers, 3.0's
                                          * after them; a 2.5 engine's, 2, and a 2.0 engine's, 1, are read with their
                                          * numbers made over; one before 3.0's, with KEY MODE and no KEYS: KEY MODE
                                          * read in as the drones' and the bass's LATCH) */
int frs_state_load(const uint8_t *in, uint32_t n);   /* back (what it doesn't know left as it is): 0, or -1 not one */

/* --------------------------------------------------------------- playing --- */
/* a key down (key: 0 .. FRS_KEYS - 1, its note as played, MIDI's numbers; vel 1 .. 127): what the keys play (KEYS:
 * the drones, GLINT, BASS) and KEY MODE (LATCH, HOLD) say; single: one note on the drones, whatever CHORD says. With
 * frs_keys_touch on, each layer's KEYS say it instead (below: the FM-1's keys, 3.0) */
void frs_key_down(uint32_t key, int note, uint32_t vel, int single);
void frs_key_up(uint32_t key);
void frs_key_pressure(uint32_t key, uint32_t p);   /* 0 .. 127 */
/* THE KEYS, as the River has them, the track's (0 the drones, 1 GLINT's arpeggio, 2 the bass), the note as it sounds:
 * latched, or let go if it is (1 latched, 0 let go, -1 nothing: full, or under the scale's lowest); held while down
 * (nothing kept); cleared. GLINT's on KEYS: the same notes, the last four to come in ringing, held: a latch with four
 * there or more retires the oldest to come in (frs_view's arp, low to high, has what stayed; a scene's come in low to
 * high).
 *   Several fingers: frs_hold counts them, a note held by two lets go at the last up (each down needs its up); the
 *   bass plays one note, the last finger's: its top one up, the last still down plays again, the last up, back to the
 *   note under them (latched before, or over them meanwhile), or silent. A latch, CLEAR or an edit (frs_param_edit)
 *   lets go what it acts on alone: what the fingers hold sounds on (the River's editor lets it go first); a note
 *   latched that a finger holds (GLINT's, the bass's) stays once the finger lets go. A scene stored has what is
 *   latched, not what is held. A scene taken (but LOAD SND ONLY), frs_scene_stop, frs_panic, frs_release_all and
 *   MIDI's All Sound Off forget the fingers: what they held let go, their ups do nothing (KEY MODE turned: the
 *   drones' and the bass's fingers; All Notes Off on the drones' channel: theirs). A finger held at the down plays the
 *   same note at its up, OCTAVE turned meanwhile or not. An up lost (a touch cancelled): frs_hold_off.
 *   RECORD on (P_SQ_REC, P_BQ_REC: LIVE or STEP, the layer on SEQ), a finger on GLINT or the bass writes into its
 *   sequence as the FM-1's keys do (LIVE: the step nearest the playhead, the sequence playing; the bass's note as long
 *   as it is held), sounding while down; no ACCENT (a finger has no velocity).
 *   chord, the drones' (the other tracks' ignore it): FRS_CHORD_HARMONY, the chord HARMONY's
 * CHORD makes; else that CHORD's (0 SINGLE: one note, VOICING CLOSE; 1 FIFTH .. 14 DIM, frs_param_text's words),
 * HARMONY's ROOT, SCALE and VOICING as they are, for this note alone: CHORD stays as it is set (nothing edited, no
 * MORPH's step). A latched chord keeps its own notes whatever HARMONY becomes */
#define FRS_CHORD_HARMONY (-1)
int frs_latch(uint32_t track, uint32_t note, int chord);
void frs_hold(uint32_t track, uint32_t note, int down, int chord);
void frs_hold_off(uint32_t track);       /* every finger on the track's notes up (a touch cancelled, ups lost) */
void frs_clear(uint32_t track);
/* a track stopped, or played again (on), as a moment: nothing of it kept in a scene (a scene stored meanwhile has what
 * the track would play), nothing edited (frs_dirty as it was; but the bass's LIVE take under way, closed and written
 * as PLAY closes it), nothing in frs_state_save.
 *   0 the drones: stopped, every latched chord lets go and rings out, kept as it was latched (frs_view's kept; MIDI's
 *     and HOLD's chords, held, sound on); played again, the kept chords latch again, each with its own notes, at
 *     velocity 100 (as a scene's). A chord latched while they are stopped plays them again with it alone: what was
 *     kept goes.
 *   1 GLINT: on ARP the arpeggio waits, its notes kept (a HOLD meanwhile sounds its glint alone); again, it goes on
 *     from its pattern's top. On KEYS its notes the same, ringing or not. On SEQ the sequence stops; again, it plays from the grid's next step, in phase with the
 *     bass's sequence if that one plays: its steps where they would be had the two begun together (where the bass's
 *     began), whatever their loops' lengths, so mid-pattern; else from its first step.
 *   2 the bass: on ARP, as GLINT's. On SEQ (with notes in it), stopped; again, from the grid's next step, in phase with
 *     GLINT's sequence if that one plays (as GLINT's with the bass's), else from its first step. Else its latched note lets
 *     go, kept (a note held, MIDI's or a key's in HOLD, sounds on and is not kept); again, it plays. A note latched
 *     while it is stopped plays it again with that note: the kept one goes.
 * A stopped track stays stopped through a MIDI Start and a host's start or locate (they start the others). A scene
 * taken (but SCENES' LOAD SND ONLY), frs_scene_stop, frs_panic and frs_release_all leave no track stopped by the
 * player and nothing kept (what they play, or let go, as they always have: RELEASE ALL stops the sequences); so do
 * MIDI's All Sound Off and All Notes Off. KEY MODE turned lets what a stopped track keeps go (as it lets the latched
 * notes go). frs_clear on a stopped track clears what it keeps (it stays stopped). A setting that starts a track
 * (a layer's PLAYS turned, RECORD) plays it again */
void frs_track_play(uint32_t track, int on);
/* every track at once: stopped (each as frs_track_play has it), or played again: with no sequence playing, the
 * sequences from their first steps together, the beat beginning there as PLAY begins it (following a MIDI clock or
 * a host, at the grid's next step), the arpeggio from its pattern's top, the drones' and the bass's kept notes;
 * with one playing (a sequence, the arpeggio), each track as frs_track_play starts it, the grid as it is */
void frs_play_all(int on);
/* a MIDI message in (notes on each track's channel, their pressure, sustain, the CCs, clock, Start, All Notes Off,
 * All Sound Off, Program Change: frs_callbacks): at the next millisecond; _at: at the frame `offset` of the next
 * frs_render (the host's time-stamp in its buffer). One or the other: they wait in one queue, in order */
void frs_midi(uint8_t status, uint8_t d1, uint8_t d2);
void frs_midi_at(uint32_t offset, uint8_t status, uint8_t d1, uint8_t d2);
void frs_midi_ms(uint64_t ms, uint8_t status, uint8_t d1, uint8_t d2);   /* at the engine's millisecond ms itself: for a
                                          * replay, from a frs_tick_hook, where no frame is at hand */
/* the host's tempo and transport at the first frame of the next frs_render (a plug-in's host: its BPM, its beat
 * position, playing or not): playing, the grid follows it as a MIDI clock (its tempo exactly, PUMP, the delay, the LFOs
 * on the grid), and it begins as a MIDI Start does (the grid's top where the host starts or locates, not on the
 * host's bar: FRASIL has no song position); stopped, the grid runs on TEMPO again. bpm 13 to 999, else (0) no host
 * tempo; the beat finite (|beat| up to 1e9; NaN: 0). CLOCK INTERNAL (P_CLOCK 1): the host's tempo not followed, the
 * grid on TEMPO; back to AUTO while the host plays, it begins there as a Start (the grid's top, the sequences' first
 * steps) */
void frs_host_tempo(double bpm, double beat, int playing);
int frs_play(void);                      /* PLAY tapped (2.5's): nothing playing at all, the scene given last's three
                                          * tracks (what RELEASE ALL let go, else its own notes, its sequences from their
                                          * first steps, the beat beginning there); what the transport stopped
                                          * (frs_track_play, frs_play_all) and nothing else playing: that, back
                                          * (frs_play_all(1)); else the sequences stop together, or start together from
                                          * the top, the arpeggio playing alone beside them starting again with them (an
                                          * arpeggio alone: it waits, or goes on). 1: something played as it was tapped
                                          * (the FM-1's: tapped again within FRS_PLAY_TWICE_MS, frs_release) */
#define FRS_PLAY_TWICE_MS 350u           /* PLAY tapped twice quick (the second within it of one that returned 1) */
void frs_release(void);                  /* PLAY tapped twice quick: RELEASE ALL (what it let go kept for RESTORE), never
                                          * RESTORE; the beat begins there */
void frs_release_all(void);              /* PLAY held: RELEASE ALL while anything plays; else RESTORE (what it let go,
                                          * else the scene given last's notes) */
int frs_record(void);                    /* REC, the layer the keys play (on the drones, GLINT): OFF, LIVE, STEP, OFF, its
                                          * PLAYS turned to SEQ as it begins (its arpeggio's notes kept): 1 GLINT's
                                          * recording, 2 the bass's, 0 off (P_SQ_REC, P_BQ_REC say LIVE or STEP); no
                                          * count-in (2.x's) */
int frs_record_layer(uint32_t layer);    /* REC on a layer (3.0's: 0 the drone, 1 GLINT, 2 the bass): OFF, LIVE, STEP,
                                          * OFF; LIVE turns its PLAYS to SEQ (the drone's: P_DQ_ON), its sequence playing
                                          * at once, or, COUNT-IN on (frs_count_in) and nothing playing (no sequence, no
                                          * arpeggio), after a bar counted in, everything from the downbeat with the
                                          * take (frs_view's counting); STEP on the drone: a chord from the keys and
                                          * fingers down, STEP on past it (its LENGTH, P_DQ_NLEN) as the last lets go.
                                          * Recording already, REC moves that layer's take on, else the one recording;
                                          * OFF ends a count under way. Each take begins with UNDO TAKE's copy (P_SQ_UNDO,
                                          * P_BQ_UNDO, P_DQ_UNDO). 1 GLINT's recording, 2 the bass's, 3 the drone's, 0
                                          * none (P_DQ_REC as P_SQ_REC) */
void frs_swell(int held);                /* the drone swells while held (the FM-1's ENV held) */
void frs_metronome(int on);              /* a tick on each of the grid's beats, recording or not (off: only a take's) */
void frs_record_metronome(int on);       /* REC's metronome: a take's ticks (on its sequence's beats from its first
                                          * step; on at the start, as the FM-1's; frs_state_save keeps it). P_METRO
                                          * shows the two (0 OFF, 1 REC: a take's, 2 ALWAYS: frs_metronome's), the FM-1's
                                          * GLO › RECORD K1; set, it sets them (2: both on) */
void frs_count_in(int on);               /* COUNT-IN: frs_play_all(1) with nothing playing and a LIVE take armed (P_SQ_REC
                                          * or P_BQ_REC 2, the layer on SEQ) ticks a bar first (four beats at the grid's
                                          * tempo, the one high, whatever the metronome), then starts on the downbeat
                                          * after it, the take with it (frs_view's counting, count_beat). A stop, CUT,
                                          * PANIC, RELEASE ALL, a track played or stopped, a scene taken or stopped: no
                                          * more count, nothing started. Off at the start (P_COUNT_IN shows it: the
                                          * FM-1's GLO › RECORD K2, on there at the start; frs_record_layer counts in
                                          * too) */
void frs_overdub(int on);                /* OVERDUB: a LIVE take adds its notes to the sequence (on at the start, as the
                                          * FM-1's). Off, its first turn replaces it: each step, as the playhead comes to
                                          * it, a rest (the bass's a CUT), what was there heard until then, unless the take
                                          * wrote it; a turn on, it adds */
void frs_step_stays(int on);             /* RECORD STEP (P_SQ_REC, P_BQ_REC, P_DQ_REC 1) without moving on: STEP stays
                                          * where it is, the UI moving it (P_SQ_STEP, P_BQ_STEP, P_DQ_STEP); a key on
                                          * GLINT's or the bass's step writes its one note there (the one before
                                          * replaced, LENGTH 1), the drone's adds beside its notes. Off at the start (the
                                          * FM-1's: STEP on past what was written) */
void frs_compare(int on);                /* the scene as taken sounds, not as it is now (the FM-1's EDIT held) */
void frs_trying(int on);                 /* the player tries a line on GLINT's sequence (RECORD OFF): its keys play
                                          * their glints, the sequence as it was (the FM-1's GLINT SEQ pages) */

/* ---------------------------------------------------------------- scenes --- */
void frs_scene_take(const uint8_t *scene);   /* as a load takes it: its sound, what it plays, its sequences from the
                                              * top (the FM-1's LOAD SND+NOTES) */
void frs_scene_play(const uint8_t *scene, int hold);   /* the River's: taken, its notes edited from here (frs_latch...,
                                                        * frs_scene_edited); hold: one that plays nothing holds a chord
                                                        * on its ROOT (▶: never saved) */
void frs_scene_go(const uint8_t *scene);     /* as the FM-1's SCENES has it: MORPH (its time), LOAD (SND ONLY: what
                                              * plays stays) */
void frs_scene_stop(void);               /* every note let go, the sequences stopped */
void frs_scene_store(uint8_t *out, const uint8_t *from);   /* the scene as STORE keeps it now (its sound, what plays,
                                                            * its sequences, its TEMPO; its name from `from`, else
                                                            * NEW SHARD's), and the sound marked as saved */
void frs_scene_edited(const uint8_t *played, uint8_t *out);   /* the River's: the shard played as it is now (its
                                                               * notes once edited) */
void frs_scene_new(uint8_t *out);        /* NEW SHARD */
void frs_keys_defaults(void);            /* KEYS back to their defaults, as NEW SHARD gives them on the FM-1 (a scene
                                          * keeps none: the runner's NEW SHARD calls this) */
int frs_scene_factory(uint32_t i, uint8_t *out);   /* the factory scenes: 0, or -1: none i */
int frs_scene_over(uint8_t *scene, uint32_t n);   /* a scene read from an earlier layout, its n bytes first in scene,
                                          * which holds FRS_SCENE_BYTES: every one with the drone's sequence none (off,
                                          * every step a rest, its settings their defaults): 3.0's first, 324 ("FRSy"),
                                          * that alone; 2.7's 323 ("FRSx": AUTO VIB at 0), 322 ("FRSw": the bass's SPACE too), 319 ("FRSv": BASS TONE too); 2.6's 297 ("FRSt": its bass's sixteen notes, then
                                          * their lengths, onto the one grid, the FM-1's own rule) or 2.7's first test
                                          * builds' 298 ("FRSu"), made over in place: PLAYS' 1 (SEQUENCE on) is 2, the
                                          * bass's arpeggio none, its settings and GATE their defaults, whatever bytes
                                          * were after n. FRS_SCENE_BYTES itself: as it is. 0, or -1 a size it does not
                                          * know, scene untouched */
int frs_dirty(void);                     /* the sound changed since the scene was taken or stored */

/* -------------------------------------------------- the drone's sequence --- */
/* DRONE SEQ (3.0): FRS_DQ_STEPS steps, each the notes it plays, FRS_DQ_NOTES at most: a chord as it was built (a chord
 * latched, single notes stacked over it), replayed as it was. Its settings, as GLINT's and the bass's: P_DQ_ON (OFF, ON;
 * no ARP), P_DQ_RATE (every step's, the bass's RATE's values: 1/16 .. 16 BARS, a beat, a bar, 2 and 4 BARS among them;
 * 1 BAR to begin with), P_DQ_LEN (SEQ LENGTH - 1: 1 .. 32 steps, 4 to begin with), P_DQ_GATE, P_DQ_CHANCE, P_DQ_TRANS
 * (along the scale, -14 .. +14 around 14), kept in the scene; P_DQ_REC (OFF, STEP, LIVE), P_DQ_CLEAR (turned twice: x
 * 2), P_DQ_STEP (the step RECORD STEP writes into), not kept.
 *   Playing: from the top with the other sequences (PLAY, a scene, MIDI Start; turned on, at the grid's next step), part
 * of the drones' track (frs_track_play 0 stops it with their chords, plays it again from its first step). A step's
 * notes sound on the drone as a held chord does, every layer, its own slow envelopes, let go at GATE's end in its last
 * step (a tie: on) or as the next step begins; a note a chord latched holds too is struck once and sounds while either
 * holds it. CHANCE leaves a step out (silent), on its own draws.
 *   RECORD: what the drones' keys sound goes into it, TRANSPOSE taken off (it plays as it was played): a chord latched
 * (frs_latch 0), a finger (frs_hold 0), a key (frs_key_down on the drones), a MIDI note on the drones' channel. LIVE:
 * into the step nearest the playhead, beside the notes it has (none twice, FRS_DQ_NOTES at most), its sequence playing
 * (stopped: they sound, nothing written; with frs_count_in, PLAY counts in for it as for the others; frs_overdub off, its
 * first turn replaces it as theirs do). STEP: into the step on P_DQ_STEP; it moves on once the last finger or key down
 * lets go (a latch adds to it, STEP staying: a chord built there, latch by latch). The keys play as they always do
 * meanwhile: a chord latched stays latched, over the sequence.
 *   Its steps in frs_view_t's dnote (and the step playing, dseq_pos); each step's bytes: its notes low to high, 1 .. 126,
 * as the step keeps them (TRANSPOSE off them), 0 after the last; all 0: a rest; FRS_DQ_TIE first (the rest 0): a tie,
 * the step before's notes held on.
 *   From knobs (3.0, the FM-1's DRONE SEQ II and III): P_DQ_NOTES, the step on P_DQ_STEP: 0 REST, 1 its notes, 2 TIE
 * (turned off its notes, they are kept; back to 1: those, with the drones' keys and fingers held now beside them; none at
 * all, HARMONY's chord on ROOT); P_DQ_NLEN, its chord's LENGTH in steps (its ties; on a tie, the chord it carries on;
 * RECORD STEP: the length the next chord takes, STEP on past it); P_DQ_UNDO turned twice, the sequence and its SEQ
 * LENGTH before the last take (RECORD on, from OFF) or CLEAR (again: back) */
#define FRS_DQ_STEPS 32u
#define FRS_DQ_NOTES 8u
#define FRS_DQ_TIE 127u
int frs_dseq_step(uint32_t step, const uint8_t *notes, uint32_t n);   /* step's notes set (n of them, as the step keeps
                                                                        * them: put low to high, none twice, the first
                                                                        * FRS_DQ_NOTES, past 1 .. 126 left out; n 0: a
                                                                        * rest; one FRS_DQ_TIE: a tie). 0, or -1 no step */
int frs_dseq_note(uint32_t step, uint32_t note, int on);   /* one note into the step (on: beside its others; a tie there
                                                            * becomes it; full: nothing) or out of it. 0, or -1 */
void frs_dseq_clear_step(uint32_t step);   /* a rest */

/* ------------------------------------------------------------------ KEYS --- */
/* KEYS (3.0; the iPad's TOUCH box, the FM-1's HOME › KEYS page): each layer's HARMONY (OFF: the keys chromatic, no
 * chord), NOTES (SINGLE NOTE, CHORD: HARMONY's chord, built on GLINT and the bass here too, each of its notes into their
 * arpeggio or sounding, as FrasilModel.press builds them; the bass on KEYS, GLINT on SEQ: one note), LATCH (OFF: HOLD,
 * the keys sound while down; GLINT's on KEYS: the ring of four, its last four notes ringing on, held), STACK (OFF: SWAP,
 * a new hand's key or chord in place of what the layer holds, keys down together stacking, as FrasilModel.latch's swap;
 * on, it adds, a tap on what is latched letting it go). P_KY_DR_HARM + 4 x layer + 0 .. 3 (0 the drone, 1 GLINT, 2 the
 * bass), FRS_P_STATE (frs_state_save: the device's, not a scene's). At the start: HARMONY ON, NOTES CHORD, LATCH ON
 * (the iPad's TOUCH defaults), STACK the drone's ON, GLINT's and the bass's OFF (the FM-1's ways). On ARP the
 * arpeggio's own LATCH says whether its notes stay (P_GA_LATCH, P_BA_LATCH: ARP II's, its latch as it is, ARP held
 * turning it); STACK says whether a new hand replaces them (latched).
 *   LATCH replaces KEY MODE (P_KEYS, GLOBAL's K4 till 3.0): KEY MODE set sets the drone's and the bass's LATCH (LATCH 0,
 * HOLD 1: off), every note let go as it always was; the drone's LATCH set shows in KEY MODE (a state of 2.x read in, an
 * FM-1 that still turns KEY MODE: the same). LATCH turned OFF lets that layer's latched notes go (the drones' chords,
 * GLINT's ring on KEYS, the bass's note on KEYS), what is held sounding on.
 *   Who honours them: the keys (frs_key_down, the FM-1's) once frs_keys_touch(1) (off at the start: the keys as 2.x's,
 *   KEY MODE's); MIDI's notes on the drones' channel always HARMONY and NOTES (as their defaults always did; GLINT's and
 *   the bass's MIDI notes come as they are played, LATCH and STACK not MIDI's: its note-offs, its sustain).
 *   frs_latch, frs_hold, frs_clear, THE KEYS', are as they were: the iPad's FrasilModel makes its TOUCH itself.
 * The iPad's way over (a later step): PlayChoices' latches, stacks, singles, harmonys read and written as these settings
 * (frs_param_get, frs_param_set; NOTES is !single), so that the iPad and the FM-1 tell one story; then either (a) keep
 * FrasilModel's latch/press as they are, reading these, or (b) hand the keys to the engine: frs_keys_touch(1), each
 * finger a key (frs_key_down with its note as played, the track the layer P_KB says, frs_key_up; a tap on a lit one lets
 * it go as on the FM-1), FrasilModel's SWAP and chord building gone. Its FM1Remote: the twelve as the FM-1's other
 * settings (P_KEYS no longer needed once it does) */
void frs_keys_touch(int on);

/* --------------------------------------------------------- a layer's preset --- */
/* A user's preset (3.0: the FM-1's and the iPad's UserPresets): a layer's sound under a name, apart from the scenes. Each
 * layer's settings (frs_preset_param: the iPad's UserPresets.sets, in its order): the drone's 33 a factory preset sets
 * (the River's DRONE_P), GLINT's ten (RATIO, BRIGHT, DECAY, SPACE, RELEASE, ATTACK, PING, SUSTAIN, VIBRATO, AUTO VIB), the
 * bass's fourteen (MODEL, DRIVE, FILTER, RESONANCE, GLIDE, ATTACK, DECAY, SUSTAIN, RELEASE, PULSE, WEIGHT, PUNCH, BITE,
 * SPACE); never LEVEL, OCTAVE, PRESET. Its bytes, FRS_PRESET_BYTES at most: "FRp", its version (1), its layer (0 the
 * drone, 1 GLINT, 2 the bass), n, its name (16 characters, NUL-padded, as a scene's), then n pairs: a setting's number,
 * its value. No storage here: the FM-1's store (the firmware's) and the iPad's presets.json keep them */
#define FRS_PRESET_HEAD 22u              /* its bytes before its pairs */
#define FRS_PRESET_BYTES 88u             /* a drone's, the longest */
uint32_t frs_preset_capture(uint32_t layer, const char *name, uint8_t *out);   /* the layer's sound as it sounds now,
                                                                                * named (NUL-ended, 16 at most; 0 none),
                                                                                * into out: its length, 0 no such layer */
int frs_preset_apply(const uint8_t *in, uint32_t n);   /* taken: its layer's settings each set as a hand sets them (the
                                                        * drone: CUSTOM, from them), another layer's or a setting it does
                                                        * not have left as they are: its layer, -1 not one */
int frs_preset_is(const uint8_t *in, uint32_t n);   /* the sound is it now: 1, else 0; -1 not one (the one lit) */
int frs_preset_param(uint32_t layer, uint32_t i);   /* the layer's setting i (0 ..), -1 past the last (any thread) */

/* ---------------------------------------------------------------- a UI's --- */
typedef struct {
    uint32_t latched[4];                 /* the drones' chords latched, by root: bit n % 32 of latched[n / 32] */
    uint32_t held[4];                    /* the notes the drones' chords hold */
    int8_t arp[16];                      /* GLINT's arpeggio's notes (without OCTAVE), low to high */
    uint8_t narp;
    uint8_t seq[32];                     /* GLINT's sequence: 0 a rest, 127 a tie, else the note; + 128 ACCENT */
    uint8_t bnote[32];                   /* the bass's sequence: 0 CUT, 127 HOLD (the note before goes on), else the
                                          * note; + 128 ACCENT */
    uint8_t bq_steps, bq_len;            /* its SEQ LENGTH - 1, its RATE (P_BQ_LEN's value, every step's) */
    int8_t bass;                         /* the bass's note held, as it sounds; -1 none */
    int8_t seq_pos, bseq_pos;            /* the steps playing; -1 none */
    uint8_t seq_run, bseq_run;           /* the sequences playing */
    uint8_t arp_latch, arp_paused;
    int8_t barp[16];                     /* the bass's arpeggio's notes (without OCTAVE), low to high, as GLINT's */
    uint8_t nbarp;
    uint8_t barp_latch, barp_paused;
    int8_t barp_note;                    /* the note its step sounds (its RANGE's octaves on it, without OCTAVE), -1 none:
                                          * what GLINT's has in frs_live's 39 */
    uint8_t dirty;                       /* frs_dirty */
    float grid;                          /* the grid: MIDI clocks since its top (24 a beat) */
    float tempo;                         /* the BPM the grid runs at (a MIDI clock's, a host's, else TEMPO) */
    uint32_t ms;                         /* the engine's clock then */
    uint32_t kept[4];                    /* the drones' chords kept while they are stopped, by root (as latched) */
    uint8_t playing;                     /* bit t: track t plays (frs_track_play; 0 the drones, 1 GLINT, 2 the bass): a
                                          * transport's. GLINT on SEQ: its sequence runs; on ARP: its arpeggio doesn't
                                          * wait; the bass on ARP or SEQ, the same; else: not stopped */
    int8_t bass_kept;                    /* the bass's note kept while it is stopped, as it sounds; -1 none */
    uint8_t counting;                    /* PLAY counting in (frs_count_in) */
    uint8_t count_beat;                  /* its beats ticked, 1 .. 4 (0: its first about to) */
    uint8_t dnote[32][8];                /* the drone's sequence's steps (FRS_DQ_STEPS of FRS_DQ_NOTES: each its notes low
                                          * to high, 0 after them; a rest all 0; FRS_DQ_TIE first, a tie) */
    uint8_t dq_len, dq_rate;             /* its SEQ LENGTH - 1, its RATE (P_DQ_LEN's, P_DQ_RATE's values) */
    int8_t dseq_pos;                     /* the step playing; -1 none */
    uint8_t dseq_run;                    /* playing */
    uint8_t dq_snd[8], dq_nsnd;          /* the notes its step sounds now (TRANSPOSE on them), how many */
} frs_view_t;
void frs_view(frs_view_t *v);            /* now (the thread that renders) */
void frs_live(float *out);               /* what sounds now, FRS_LIVE floats (engine/core.c eng_live: the voices, the
                                          * glints, the bass, the LFOs, the steps, the meters, the tide, LOFI TAPE's
                                          * drag) */
int frs_snapshot(frs_view_t *v, float *live);   /* both, as they were after the last block, from any thread (a UI's,
                                                 * while the audio thread renders): 0, or -1 none yet (or busy: again) */
/* the effects as they sound now, FRS_LIVE_FX floats (the thread that renders; frs_snapshot_fx from any thread, as they
 * were after the last block):
 *    0 LOFI TAPE's mix (0 off .. 1)    1 its speed (1 as it should run: the reels)   2 3 4 its wow's, drift's and
 *      flutter's phases (turns)        5 a drag's depth (its deepest, 0: none)       6 7 SATURATION, WOBBLE as they sound
 *    8 MASTER FILTER's mix   9 its type (0 off, 1 low pass, 2 high pass)   10 its cutoff now (Hz)   11 its Q now
 *   12 its tracks (FRS_TRK_* bits)   13 before the cloud (1)
 *   14 MASTER EQ's mix   15 .. 29 its three biquads now (low shelf, bell, high shelf), each b0 b1 b2 a1 a2 (a0 divided
 *      out; frs_fx_response draws them)
 *   30 MASTERING's gain reduction now (dB, 0 or less)   31 the mix's recent peak (0..1, the louder side; before
 *      frs_gain, the FM-1's meter's)   32 33 the output's peak, left and right (0..1, after frs_gain: frs_render's)
 *   34 35 its RMS, left and right (0..1; some 300 ms; after frs_gain)
 *   36 PUMP's duck now (0..1: each track's gain 1 - 0.95 x its DEPTH x it; 0: no duck, RATE OFF)
 *   37 .. 39 (none yet: 0) */
void frs_live_fx(float *out);
int frs_snapshot_fx(float *fx);
/* what MASTER FILTER (which 0) or MASTER EQ (which 1) does at hz, in dB, as fx (frs_live_fx's, a snapshot's) has it
 * now, at the engine's rate (fs): its curve for a screen, from any thread (it reads fx alone) */
float frs_fx_response(const float *fx, int which, float hz, float fs);
/* the engine's words ("SEQUENCE" "CLEARED"; a: up to 23 characters, b too) and a Program Change on the drones'
 * channel (n: 0 .. 127, the app's to take a scene for), as they happen, in the thread that renders. The words live
 * during the call alone: copy them to keep them */
void frs_callbacks(void (*say)(void *user, const char *a, const char *b), void (*program)(void *user, uint32_t n),
                   void *user);

#ifdef __cplusplus
}
#endif
