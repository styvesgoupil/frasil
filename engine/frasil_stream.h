/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* SHARD STREAM: a performance as a recipe, not audio: the start (a scene, the state beside it, the moment's settings,
 * the tracks that played), then what the hands did, stamped in the engine's milliseconds. C on the engine's public API
 * alone, no platform in it, so that one reader serves the iPad and, later, the River and the FM-1 (it moves to engine/
 * then, git mv, never copied).
 *
 * A file is ".frstream", little-endian: "FSTR", version, flags, two bytes reserved, then chunks, each a four-letter tag, a
 * u32 length, its bytes; a reader skips the chunks it doesn't know. The last is CRC_, the CRC-32 of all before it.
 *   HEAD  rate u32, ms u32, events u32, engine[8], layout[4], nparam u16, tracks u8, lost u32, created i64
 *   SCN0  the scene at the start (its own copy: a later SAVE over the shard changes nothing here)
 *   STAT  frs_state_save's bytes
 *   MOVE  (setting, value) pairs: the FRS_P_MOMENT settings as they were
 *   DENS  FST_DENS bytes: how busy the hands were across its length, to draw without decoding
 *   SCNS  u16 count, then that many scenes, none twice
 *   EVTS  the events
 * An event: varint dt_ms, op, args. The op's top two bits give the args' length (0, 2, 4, or a length byte first), so a
 * reader skips an op it doesn't know, and counts it. Ops have their own numbers, stable for ever, never a platform's.
 *
 * Exactness. frs_ms() moves at block boundaries alone and a call acts at the next block whatever its frame, so what
 * fixes an event's effect is which two blocks it came between, and that is what its stamp names. Recording and replay
 * both start with fst_fresh; the first block is rendered inside it, so each later block has a millisecond of its own. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "frasil_engine.h"
#ifdef __cplusplus
extern "C" {
#endif

#define FST_VERSION 1u
#define FST_SCENE_LAYOUT "FRS}"          /* the scenes in it (the River's name): the engine's FRS_SCENE_BYTES. "FRS|"
                                          * (3.0.2-b5's test builds, never released: their AMOUNTs, in the scenes, the
                                          * state and the moves, in percent) is refused. The older
                                          * ones read too, their scenes made over as they are played (frasil_stream.c
                                          * OLD_LAYOUT: "FRS{" 590, 3.0.2's, before the name's rest, the LFOs' AMOUNTs and
                                          * SWELL; "FRSz" 586, 3.0's, before LOFI TAPE II too: frs_scene_over, and SWELL as
                                          * it is when one is played, as their engines left it); the stream itself is not
                                          * changed (its CRC stays) */
#define FST_BUDGET 2048u                 /* what a block may ask of a replay (fst_read's walk): an event 1, a scene, PANIC,
                                          * CUT or a state 16, an opening 512; a take can't reach it, a damaged or made-up file is refused */
#define FST_DENS 64u                     /* DENS's bytes */
#define FST_MOVES 48u                    /* FRS_P_MOMENT's settings, at most (12 in 3.0; fst_capture stops here) */
#define FST_JEVENT_MAX (5u + 3u + FRS_SCENE_BYTES + 8u)
#define FST_JHEAD_MAX (64u + FRS_SCENE_BYTES + FRS_STATE_BYTES + 2u * FST_MOVES)

/* the ops (stable numbers). Top bits: 00 no args, 01 two, 10 four, 11 a length byte, then that many */
enum {
    FST_CUT = 0x01, FST_PLAY = 0x02, FST_PANIC = 0x03,
    FST_CUT_STOP = 0x04,                 /* frs_cut_stop (the iPad's CUT since 2026-11-05): written just before an FST_CUT,
                                          * which a reader that knows it skips (one that doesn't skips this, plays that) */
    FST_RELEASE = 0x05,                  /* frs_release (the FM-1's PLAY tapped twice: RELEASE ALL) */
    FST_RELEASE_ALL = 0x06,              /* frs_release_all (PLAY held: RELEASE ALL, or RESTORE) */
    FST_PARAM = 0x40, FST_HOLD_OFF = 0x41, FST_CLEAR = 0x42, FST_TRACK_PLAY = 0x43, FST_PLAY_ALL = 0x44, FST_KEY_UP = 0x45,
    FST_SCENE = 0x46, FST_SCENE_NOW = 0x47,   /* (a u16 index into SCNS: frs_scene_go, frs_scene_take) */
    FST_KEY_PRESSURE = 0x48,             /* (the key, its pressure: frs_key_pressure; the iPad's number) */
    FST_SWELL = 0x49,                    /* (held, 0: frs_swell, the FM-1's ENV held) */
    FST_COMPARE = 0x4A,                  /* (on, 0: frs_compare, the FM-1's EDIT held) */
    FST_RECORD = 0x4B,                   /* (the layer, 0: frs_record_layer, REC; an FST_RECORDING just before it) */
    FST_LATCH = 0x80, FST_HOLD = 0x81,
    FST_KEY_DOWN = 0x82,                 /* (key, note, track: P_KB set first, velocity: 0 reads as 100, a touch's) */
    FST_KEY_SINGLE = 0x86,               /* (as FST_KEY_DOWN's: frs_key_down's single, one note on the drones, SEL held) */
    FST_MIDI = 0x83,                     /* (status, d1, d2, the milliseconds ahead of its stamp it is taken) */
    FST_STORE = 0x84,                    /* (a u16 index into SCNS, 0, 0: frs_scene_store, into nothing kept) */
    FST_RECORDING = 0x85,                /* (COUNT-IN, OVERDUB, a take's ticks, 0: forced off in a replay) */
    FST_ANCHOR_DROP = 0x20,              /* frs_anchor_drop */
    FST_ANCHOR = 0x60,                   /* (a u16 index into SCNS: the anchor set as it was, frs_anchor_put) */
    FST_ANCHOR_BACK = 0x61,              /* (a u16: the glide's ms, 0 at once: frs_anchor_back) */
    FST_DSTEP = 0xC0,                    /* (the step, its notes: frs_dseq_step) */
    FST_STATE = 0xC1,                    /* (frs_state_load's bytes) */
    FST_OPEN = 0xC2,                     /* (the engine opened again: a u16 index into SCNS, tracks, rate u32, state's length,
                                          * its bytes, moves' count, its pairs) */
    /* the journal's alone, never in a file */
    FSTJ_SCENE = 0xF0,                   /* (the op it stands for, a length u8, FRS_SCENE_BYTES bytes) */
    FSTJ_OPEN = 0xF1,                    /* (a start block, as the journal's head has it) */
    FSTJ_END = 0xF2,                     /* (lost u32) */
    FSTJ_START = 0xF3                    /* (the ring's: the start is in; the journal's head) */
};

/* an event as the audio thread hands it over: no pointers, a fixed size, a ring's slot */
typedef struct {
    uint32_t ms;
    uint8_t op, n;                       /* n: the args in a[]; FSTJ_SCENE: a[0] the op, a[1] a table's slot, a[2] a length */
    uint8_t a[14];
} fst_ev;

/* what a stream starts from */
typedef struct {
    uint8_t scene[FRS_SCENE_BYTES];
    uint8_t state[FRS_STATE_BYTES];
    uint8_t moves[2u * FST_MOVES];
    uint32_t state_n;
    uint8_t moves_n, tracks;
} fst_start;

/* the engine as it is now (the thread that renders): what fst_fresh starts from. It stores the scene (frs_scene_store), so
 * the sound reads as saved until the engine opens again */
void fst_capture(fst_start *s, const uint8_t *from);
/* a fresh engine at fs, as s says, the first block rendered; 0, or -1 it would not open (the thread that renders) */
int fst_fresh(const fst_start *s, float fs);
/* what the engine's frs_midi_at takes `offset` frames on from `frames`, in milliseconds ahead of its clock */
uint8_t fst_midi_ahead(uint64_t frames, uint32_t offset);

/* ---- the journal: an append-only record of a take, the recording itself until it is saved or discarded. Each piece is
 * whole or absent in what a crash leaves: fst_finish reads up to the last whole one */
uint32_t fst_jhead(const fst_start *s, uint32_t rate, int64_t created, uint8_t *out);   /* FST_JHEAD_MAX at most */
/* an event after the one stamped *last; scene: FSTJ_SCENE's FRS_SCENE_BYTES, FSTJ_OPEN's fst_start; else 0. FST_JEVENT_MAX
 * at most (FSTJ_OPEN: FST_JHEAD_MAX) */
uint32_t fst_jevent(const fst_ev *e, uint32_t *last, const void *scene, uint32_t lost, uint8_t *out);
/* the journal's whole pieces as a stream, malloc'd (the caller frees it); 0: none in it, or nothing was played */
uint8_t *fst_finish(const uint8_t *j, size_t n, size_t *out_n);

/* ---- a stream read */
typedef struct {
    uint32_t rate, ms, events, lost;
    int64_t created;
    uint16_t nparam;
    uint8_t tracks, flags;
    uint16_t sbytes;                     /* a scene's bytes in the file: FRS_SCENE_BYTES, or an older layout's (590 "FRS{",
                                          * 586 "FRSz") */
    char engine[9], layout[5];
    const uint8_t *scene, *state, *moves, *dens, *scns, *evts;
    uint32_t state_n, moves_n, nscns, evts_n;
} fst_file;
#define FST_F_FRESH 1u
#define FST_F_MOMENT 2u
#define FST_F_RATE 4u
#define FST_F_HANDS 8u

enum { FST_OK = 0, FST_BAD = -1, FST_CRC = -2, FST_VERSIONED = -3, FST_LAYOUT = -4 };
/* every byte checked: magic, version, chunks, CRC, sizes, each event walked: FST_OK, or what is wrong. A stream of another
 * layout reads (FST_LAYOUT): what it says about itself is there, it can't be played by this engine */
int fst_read(const uint8_t *b, size_t n, fst_file *f);
void fst_start_of(const fst_file *f, fst_start *s);

/* ---- a stream played, on the thread that renders; nothing here allocates, locks or opens a file */
typedef struct {
    uint32_t ms;
    uint8_t op, n;
    const uint8_t *a;                    /* its args, in the stream */
} fst_dec;
typedef struct {
    fst_file f;
    uint32_t at, t, missed;              /* the place in EVTS, the last event's stamp, ops not known */
    uint8_t cut_stop;                    /* the last event applied an FST_CUT_STOP: the FST_CUT after it skipped */
    int have;                            /* next, decoded and waiting */
    fst_dec next;
    uint8_t scratch[FRS_SCENE_BYTES];
    uint8_t conv[FRS_SCENE_BYTES];       /* a scene of an old layout, made over (scene_at's) */
} fst_player;
void fst_begin(fst_player *p, const fst_file *f);
/* the next event stamped at or before now, 1, or 0 none yet. Ops not known are skipped, counted */
int fst_next(fst_player *p, uint32_t now, fst_dec *e);
int fst_more(const fst_player *p);       /* events left */
/* what it says done; frames: rendered since the engine opened. 1: it opened the engine again (FST_OPEN: rebase the clock) */
int fst_apply(fst_player *p, const fst_dec *e, uint64_t frames);

/* ---- a stream played by the engine's own milliseconds (frs_tick_hook), from any place in it: a radio. Hook, skip and
 * render give the same notes as a replay at any buffer; the thread that renders calls these */
typedef struct {
    fst_player p;
    uint32_t t0, base;                   /* the stream's clock: frs_ms() at its start, and the stamp then */
    fst_dec open;                        /* an opening reached, put off to between two blocks */
    uint8_t due, opens;                  /* one is; the stream has any */
} fst_air;
int fst_air_open(fst_air *a, const fst_file *f, float fs);   /* a fresh engine at its start; 0, or -1 it would not open */
void fst_air_skip(fst_air *a, uint32_t ms);   /* the stream's millisecond ms, nothing rendered (frs_skip_to) */
void fst_air_after(fst_air *a);          /* after each frs_render: an opening the hook put off */
uint32_t fst_air_ms(const fst_air *a);   /* the stream's clock */

/* ---- a performance heard live, no file: each op applied by itself as it comes (Perform LIVE's listener,
 * design/perform/LIVE.md). fst_apply's own code: an op does what it does in a replay, a scene given by its bytes where a
 * file has its index. The thread that renders, between two renders (an op acts at the next block); nothing allocates */
#define FST_LQ 256u                      /* ops given ahead, waiting for their millisecond (fst_live_at) */
#define FST_LQ_SCN 8u                    /* scenes among them */
typedef struct {
    uint32_t ms;                         /* the engine's millisecond it is taken in (frs_ms's) */
    uint8_t op, n, kind, sc;             /* kind: 0 an op, 1 a scene's (sc: its place in scn), 2 the running state */
    uint8_t a[FRS_STATE_BYTES];          /* (the longest: FST_STATE's) */
} fst_lq;
typedef struct {
    fst_player p;                        /* (what fst_apply keeps from one op to the next: a CUT after a CUT_STOP) */
    uint32_t done, missed, refused;      /* ops applied; not known (ignored); refused (nothing done) */
    uint32_t late, loads, same, full;    /* ops taken after their millisecond; states loaded; states found the very
                                          * same (left); ops or states that found no room (dropped) */
    fst_lq q[FST_LQ];
    uint32_t qr, qw, scn_w, scn_n;
    uint8_t scn[FST_LQ_SCN][FRS_SCENE_BYTES];
    uint8_t st[FRS_LIVE_BYTES], st_scene[FRS_SCENE_BYTES], tmp[FRS_LIVE_BYTES];
    uint32_t st_n;
    uint8_t st_on, st_cmp;
} fst_live;
/* a fresh engine at s (fst_fresh), quiet as on air: no metronome, no take's ticks, no count-in (the artist's, not the
 * listener's); its queue's hook in every millisecond (fst_live_at); 0, or -1 it would not open */
int fst_live_open(fst_live *l, const fst_start *s, float fs);
/* the running state that goes with the start just opened (frs_live_load, the start's scene): 0, or -1 not this engine's
 * (then as the start alone opened it: open again to be sure) */
int fst_live_state(fst_live *l, const uint8_t *st, uint32_t n, const uint8_t *scene);
/* card #131: an op given ahead, taken first in the engine's millisecond ms (as on air: the same op the same millisecond
 * on every engine that has it), at once if it has gone by (late); in the order given. 0, or -1 no room (full) */
int fst_live_at(fst_live *l, uint32_t ms, uint8_t op, const uint8_t *a, uint32_t n, const uint8_t *scene);
/* a running state among them (a snap's: frs_live_save's with its start's scene), loaded at its place, in ms: the engine
 * runs on from it (frs_ms its own millisecond then). cmp: left as it is when this engine's own is the very same (same).
 * One waits at most: 0, or -1 one waits already or it is too long */
int fst_live_at_state(fst_live *l, uint32_t ms, const uint8_t *st, uint32_t n, const uint8_t *scene, int cmp);
uint32_t fst_live_state_ms(const fst_live *l);   /* the millisecond a waiting state is taken in, 0xFFFFFFFF none */
void fst_live_cut(fst_live *l);          /* the queue from its waiting state on dropped (an engine copied to fade out) */
/* op, its args a[n] as fst_ev has them (an op of a length byte: without it, n its length); scene: the FRS_SCENE_BYTES
 * FST_SCENE, FST_SCENE_NOW, FST_ANCHOR and FST_STORE name (in a file, a u16 index into SCNS: not given here), else 0.
 * 0 done; 1 an op not known, ignored (missed); -1 refused, nothing done (refused): its args too short, a scene's op
 * without its scene, FST_DSTEP of no step or more than 9 bytes, FST_STATE past FRS_STATE_BYTES, FST_OPEN (an engine
 * opened again: fst_live_open). FST_MIDI at the engine's millisecond its stamp says (frs_midi_ms, as on air); after
 * FST_STATE or METRONOME (P_METRO) set, quiet again */
int fst_op(fst_live *l, uint8_t op, const uint8_t *a, uint32_t n, const uint8_t *scene);

uint32_t fst_crc(const uint8_t *b, size_t n);

#ifdef __cplusplus
}
#endif
