/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Copyright (C) 2026 Charles Vestal (from fm1-base's template, as app/app.c: eng_midi's message unpacking) */
/* FRASIL's engine: the instrument's brain (dsp/ is its sound). What a key means (a chord latched or held, GLINT's notes,
 * the bass), the chords and notes that sound, GLINT's arpeggiator and step sequencer, the bass's sequence and its LIVE
 * take, PULSE, the LFOs, SPARKLE, the metronome, the grid (MIDI clock or TEMPO), PUMP's and the delay's beat, a scene
 * taken and its notes, MORPH, RELEASE ALL and RESTORE, MIDI's meaning. Nothing plays by itself (SPARKLE's glints answer
 * what the drones hold).
 * No platform. Whoever runs it (the FM-1's shell, app/app.c; frasil_engine.c alone) defines the hooks below, includes
 * this file, and drives it: eng_init, then every millisecond eng_steps, the MIDI that came in (eng_midi), eng_moves,
 * eng_swell, and engine_push while st.engine_dirty; the keys (eng_key_down, key_up, key_pressure), the settings
 * (param_set), the scenes (scene_take, eng_scene_go) as they come. Its sound renders apart (frasil_render: dsp/). Part
 * of a unity build: FRASIL_SHELL, a platform's (its libc, its clamp), FRS_NKEYS its keys. */
#include <stddef.h>
#include <stdint.h>
#ifndef FRASIL_SHELL                     /* (alone: the C library's; on the FM-1, the platform's) */
#include <string.h>
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
#endif
#include "fastmath.h"
#include "frasil.h"
#include "params.c"

#ifndef FRS_NKEYS
#define FRS_NKEYS 32                     /* keys down at once the engine follows (each its number, below it) */
#endif
#define TAP_MS 350u                      /* a lit key let go sooner than this, without pressing in: it goes out */

/* what it asks of whoever runs it */
static uint32_t eng_ms(void);            /* milliseconds, the main loop's clock */
static void eng_say(const char *a, const char *b);   /* a gesture's words ("SEQUENCE", "CLEARED") */
static int eng_trying(uint32_t track);   /* the player tries a line where a track's sequence is edited (1 GLINT, 2 the bass;
                                          * RECORD OFF): its keys play, the sequence as it was (else they transpose it) */
static void eng_type_changed(void);      /* GLINT's SEQ changed, or a scene taken (its pages come and go) */
static int eng_cc(uint32_t cc, uint32_t v);   /* a CC on the drones' channel, the runner's first (1: taken) */
static void eng_program(uint32_t n);     /* a Program Change on the drones' channel */
static void eng_setting(int p, int32_t x);   /* the runner's own settings turned (SPK/JACK, ANIMATIONS, CLOCK: its
                                              * output, its screen; kept with its settings) */

/* --------------------------------------------------------------- state --- */
static struct {
    uint8_t v[NPARAM];                   /* every parameter now */
    uint8_t glint;                       /* the keys play GLINT */
    uint8_t bass;                        /* the keys play BASS */
#if FRS_LEAD
    uint8_t lead;                        /* the keys play LEAD (card #144: the app's builds, FRS_LEAD) */
#endif
    uint8_t dirty;                       /* the sound changed since the scene was loaded or saved */
    uint8_t sustain;                     /* MIDI CC64 held */
    float swell_btn, swell_midi, swell_sent;
    uint8_t kb_rec;                      /* KEYS before RECORD turned them to its track (RECORD OFF gives them back) */
    uint8_t rec_took;                    /* a recording under way has the keys (KEYS to give back: rec_keys) */
    uint8_t loading_morph;               /* scene_go loading a scene to MORPH to */
    uint8_t compare;                     /* EDIT held: the scene as saved sounds, not as it is now */
    uint8_t metro;                       /* REC's metronome: a tick on each beat while a sequencer records LIVE */
    uint32_t clicks, hiclicks;           /* its ticks since RECORD went on, and the higher ones (the tests) */
    uint8_t engine_dirty;
} st;

/* the drones by chord: how many chords hold each note (a note shared by two goes out with the last), the latched
 * chords by root, and MIDI's by the note that came in */
static uint8_t held_by[128];
static chord_t lat[128], mid[128];
static uint32_t lit[4], mid_on[4], sus[4];
static int8_t bass_note = -1;            /* the bass's note held (latched, or its key down), -1: none */
static uint32_t mgl[4];                  /* GLINT's notes held from MIDI */
static uint32_t mgs[4];                  /* of them, those that sounded their glint themselves (as a key's gsound) */
static int8_t mgl_base[128];             /* each one's note without GLINT's OCTAVE, as the arpeggio has it */
static int8_t mbass[8];                  /* BASS's notes held from MIDI, the last on top (it sounds) */
static uint32_t mb_n;

static int bit_get(const uint32_t *b, int n) { return n >= 0 && n < 128 && (b[n >> 5] >> (n & 31) & 1u); }
static void bit_put(uint32_t *b, int n, int on)
{
    if (n < 0 || n > 127)
        return;
    if (on)
        b[n >> 5] |= 1u << (n & 31);
    else
        b[n >> 5] &= ~(1u << (n & 31));
}

enum { K_NONE, K_LATCH, K_HOLD, K_GLINT, K_BASS, K_BSEQ, K_BARP };   /* (K_BSEQ: BASS's, into its sequence; K_BARP: its
                                                                      * arpeggio) */
static struct {                          /* each key while it is down */
    uint8_t kind;                        /* what it started as (the mode may change while it is held) */
    int8_t root;                         /* K_LATCH: its chord's root; K_GLINT: the glint's note; K_BASS, K_BSEQ: the
                                          * bass's */
    int8_t gbase;                        /* K_GLINT: its note without GLINT's OCTAVE (the arpeggio's, the sequence's);
                                          * K_BSEQ, K_BARP: without BASS's (the bass's sequence's, its arpeggio's) */
    uint8_t gsound;                      /* K_GLINT: it sounded its glint itself (SEQ OFF, or RECORD on): its release
                                          * lets it go, whatever SEQ and RECORD are by then */
    uint8_t hold;                        /* K_BASS: held (LATCH off: KEY MODE HOLD), its up lets it go */
    chord_t chord;                       /* K_HOLD: the chord it holds; K_GLINT, K_BARP: its notes (NOTES CHORD:
                                          * HARMONY's chord; else the one, gbase), without OCTAVE */
    uint8_t was_lit;                     /* it was latched already when pressed: a tap lets it go */
    uint8_t pressed_in;                  /* it was pressed into: no tap */
    uint8_t prs;                         /* its pressure last sent */
    uint8_t drec;                        /* its chord went into the drone's sequence on STEP (its up moves STEP on) */
    uint32_t t;
} key[FRS_NKEYS];

/* an arpeggiator's state (its code below, after the MIDI), GLINT's (arp) and the bass's (barp): here for the scenes.
 * Each its notes and its pattern; the grid they run on, one */
typedef struct {
    int8_t held[16];                     /* the notes the keys (or the latch, a scene) ask for, low to high */
    uint16_t in[16];                     /* and when each came in, ins its count (a scene's: low to high) */
    uint16_t ins;
    float prs[16];                       /* and how hard each is pressed */
    int8_t play[16];                     /* the notes playing: held's, taken at the next step (on RATE's grid) */
    float pprs[16];
    uint8_t pn, dirty;                   /* how many play; held changed since the last step */
    int32_t step_n;                      /* the step of the grid at the last run */
    int8_t src;                          /* the held note the sounding step comes from */
    uint8_t n, vel;
    int32_t idx, dir;
    int16_t sounding;                    /* the step sounding, -1: none */
    uint32_t off_ms;
    uint32_t steps;                      /* steps played (the tests) */
    uint8_t latch;                       /* the notes stay when the keys let go (ARP held) */
    uint8_t paused;                      /* PLAY tapped: it waits, its notes kept (PLAY again: from the top) */
    uint32_t rng;                        /* (the bass's own: what it draws changes nothing of GLINT's) */
    /* ARP SNAP (P_ARP_SNAP, arp_turn): a new set taken on its grid, the hand gathered first */
    uint32_t chg_ms;                     /* when held last changed (a key, a MIDI note, a finger) */
    uint32_t hand_ms;                    /* when a key or a note last went into it (the hand's latest) */
    uint32_t b_ms;                       /* the last point a set could be taken on (a snap point; from silence any step) */
    int32_t b_step;                      /* and its step */
    uint16_t b_late;                     /* how long after it a new set still lands on it, ms */
    uint8_t fresh;                       /* from silence (off ARP, waiting, nothing to play): it starts on any step */
    uint8_t gap;                         /* its set let go to nothing since the last snap point: a new one is a change */
    uint8_t owe;                         /* that point's step held back, the hand still coming */
} arp_t;
static arp_t arp = {.sounding = -1, .rng = 0x9E3779B9u, .latch = 1, .step_n = -1},   /* latched from the start */
             barp = {.sounding = -1, .rng = 0x7F4A7C15u, .latch = 1, .step_n = -1};
#ifndef ARP_TRACE
#define ARP_TRACE(a, what, note)         /* (a runner's log of each step and each set taken: the simulator's arplog) */
#endif
/* GLINT's notes latched (THE KEYS' LATCH, a scene's): on KEYS (SEQUENCE off) the last RING notes in its arpeggio's list
 * ring on, held, in place of its steps (ring_run; on ARP they step, on SEQ they wait, the list the same): those sounding
 * so. The keys never put notes in it: on KEYS they sound while down, whatever KEYS' LATCH says, and a GLINT key there
 * lets it go first (card #143) */
#define RING 4u                          /* (GLINT's voices) */
static struct {
    int8_t note[RING];
    uint8_t n;
} rung;
static uint32_t hold_k[4];               /* GLINT's notes a finger sounds itself (THE KEYS' HOLD, off its arpeggio) */
static uint32_t hold_a[4], hold_ba[4];   /* GLINT's notes held into the arpeggio by fingers (THE KEYS' HOLD: without
                                          * OCTAVE, as it keeps them), the bass's */
static chord_t hold_c[128];             /* the drones' chords fingers hold (THE KEYS' HOLD), by the note */
static int8_t hold_b = -1, hold_bn = -1; /* the bass's note before the one a finger holds, the one held (as played) */
static uint8_t set_was;                  /* (param_set: the setting's value before it was turned) */
static uint8_t keymode_gone;             /* (KEY MODE turned: every note let go, release_all; the engine alone's to see) */
static uint8_t latch_gone;               /* (KEYS' LATCH turned off: bit t, layer t's latched notes let go, keys_set) */
/* the metronome of its own (frs_metronome, METRONOME ALWAYS), a take under way or not: while no sequence records, a
 * tick on each of the grid's beats, each fourth high */
static uint8_t metro_free;

/* KEYS (P_KY_*: the iPad's TOUCH, each layer's HARMONY, NOTES, LATCH, STACK), honoured by the keys (eng_key_down) once
 * the runner says so (eng_keys_touch: the FM-1's 3.0 hands; the engine alone's frs_keys_touch); till then the keys as
 * they were, KEY MODE's (P_KEYS: LATCH or HOLD, the drones' and the bass's), GLINT's sounding while down, single notes
 * in the scale on GLINT and the bass, the drones' chords stacked. MIDI's notes on the drones' channel honour HARMONY and
 * NOTES whatever it says (their defaults: as ever) */
static uint8_t keys_touch;
typedef struct {
    uint8_t harm, chord, latch, stack;
} touch_t;
static touch_t touch_set(uint32_t layer) /* the layer's KEYS as set */
{
    const uint8_t *v = &st.v[P_KY_DR_HARM + 4u * (layer % 3u)];
    touch_t t;
    t.harm = v[0];
    t.chord = v[1];
    t.latch = v[2];
    t.stack = v[3];
    return t;
}
static touch_t touch_of(uint32_t layer)  /* the layer's KEYS as the keys honour them now */
{
    touch_t t;
    if (keys_touch)
        return touch_set(layer);
    t.harm = 1;                          /* (KEY MODE's: the drones' chords, GLINT's and the bass's one note) */
    t.chord = layer == 0u;
    t.latch = layer != 1u && !st.v[P_KEYS];
    t.stack = layer == 0u;
    return t;
}
/* the bass's note held, not latched, as the scene keeps it (none of it): KEY MODE HOLD, or (KEYS) the bass's LATCH off */
static int bass_holds(void) { return keys_touch ? !st.v[P_KY_BS_LATCH] : st.v[P_KEYS] != 0u; }
static int ring_has(int n)
{
    uint32_t i;
    for (i = 0; i < rung.n; i++)
        if (rung.note[i] == n)
            return 1;
    return 0;
}
static int arp_plays(void);              /* (its notes sound: on ARP, or on KEYS with some) */
static int glint_taken(int n);
static struct {
    float tick;                          /* the grid: MIDI clocks (24 a beat) since the start, from the clock or
                                          * from TEMPO; wraps at 9600 (GLINT's every step size divides it) */
    uint32_t wraps;                      /* how often, since its top (the bass's 8 and 16 BARS do not divide it) */
    uint32_t last_ms, clk_new;           /* the last run; MIDI clocks come in since it */
    uint32_t clk, clk_at;                /* MIDI clocks since the last step; when the last came */
    float clk_ms;                        /* ms between clocks: the tempo as followed (clock_in; the delay's, PUMP's) */
    float clk_due;                       /* when the next is due, ms after the last (clock_in) */
    uint32_t clk_n, clk_beat;            /* the clocks since this clock began (clock_in); when the beat measured began */
    int32_t clk_far;                     /* that beat's last: far over the tempo (1), far under (-1), else 0 */
} grid;

/* LOFI TAPE II's TAPE SPEED as it plays (tape_run): the octaves it moves (params.c tape_oct), gliding to its setting's as
 * a tape slows or speeds up; tape_tk its factor on the grid's pace and the voices' times, 1 while a clock leads (a MIDI
 * clock, the host's tempo: the pitch alone then) */
static float tape_l2, tape_sent, tape_tk = 1.0f;
static uint32_t tape_at;
static uint8_t tape_clocked, tape_fresh = 1;   /* (fresh: the engine opened, its first run there at once, no glide) */

static int grid_clocked(uint32_t now) { return now - grid.clk_at < 500u && grid.clk_ms > 1.0f; }

/* a MIDI clock's milliseconds on the grid: the clock's coming in, else TEMPO's at TAPE SPEED */
static float grid_ms_tick(uint32_t now)
{
    return grid_clocked(now) ? grid.clk_ms : 2500.0f / ((float)(st.v[P_TEMPO] + 30u) * tape_tk);
}

/* a MIDI clock come in, now: its tempo followed (grid.clk_ms). A clock's time is when the brain reads it: on the FM-1
 * between two frames of its screen, a few together and up to a frame's drawing late; in a browser or on the iPad, at an
 * audio block. Each clock's own gap is too uneven to take (card #123: the tempo off by up to 2 %, GLINT's delay moved
 * at nearly every clock and its echoes slid in pitch), so an alpha-beta filter follows the clocks' times instead (when
 * the next is due, the period): as the clock begins, a straight line through all its clocks so far (least squares);
 * then gains of a bar or two (CLK_A, CLK_B: Benedict-Bordner's pair). Linear, so a lateness that comes and goes
 * averages out (cut short, it would pull the tempo: a frame drawn in 45 ms made it 1.3 % slow). Not measured, only
 * counted: a clock read with the one before (the same millisecond), and one off its time by a clock and a half
 * (CLK_LATE): late, the brain was held up (a STORE: the clocks held back with it come together after it) or the clock
 * paused or lost some (late again, none together: its time is the clock's from there, the tempo kept). A beat (24
 * clocks) off the tempo by 8 % twice in a row is a new tempo: that beat's, learnt again from there (a jump followed
 * within a bar, not over four; a tempo the filter follows alone, under 8 % off, is never more than a clock off) */
#define CLK_A 0.04f
#define CLK_B (CLK_A * CLK_A / (2.0f - CLK_A))
#define CLK_LATE(ms) ((ms) * 1.5f > 10.0f ? (ms) * 1.5f : 10.0f)   /* (ms: a clock and a half, at least 10) */
static void clock_in(uint32_t now)
{
    uint32_t dt = now - grid.clk_at;
    float e, a = CLK_A, b = CLK_B;
    if (dt >= 500u) {                    /* it begins (again): its tempo learnt from here */
        grid.clk_at = grid.clk_beat = now;
        grid.clk_n = 0u;
        grid.clk_far = 0;
        grid.clk_due = grid.clk_ms;
        return;
    }
    if (++grid.clk_n >= 0x60000000u)     /* (kept large, a whole number of beats: clk_n % 24 the beat still) */
        grid.clk_n -= 0x30000000u;
    if (grid.clk_n % 24u == 0u) {        /* a beat: far off the tempo twice in a row, the tempo is that beat's */
        float pb = (float)(now - grid.clk_beat) * (1.0f / 24.0f), d = pb - grid.clk_ms;
        int32_t far = d > grid.clk_ms * 0.08f ? 1 : d < grid.clk_ms * -0.08f ? -1 : 0;
        grid.clk_beat = now;
        if (far && far == grid.clk_far && grid.clk_n >= 48u) {
            grid.clk_ms = grid.clk_due = pb;
            grid.clk_at = now;
            grid.clk_n = 24u;
            grid.clk_far = 0;
            return;
        }
        grid.clk_far = far;
    }
    if (grid.clk_n == 1u || grid.clk_ms <= 1.0f) {   /* its first gap: the tempo, for now (read together: the next) */
        if (dt) {
            grid.clk_at = now;
            grid.clk_ms = grid.clk_due = (float)dt;
        } else {
            grid.clk_n = 0u;
        }
        return;
    }
    e = (float)dt - grid.clk_due;
    grid.clk_at = now;
    if (!dt) {                           /* read with the one before: counted, the next due a period later */
        grid.clk_due += grid.clk_ms;
        return;
    }
    if (grid.clk_n >= 24u && (e > CLK_LATE(grid.clk_ms) || e < -CLK_LATE(grid.clk_ms))) {   /* far off its time,
                                          * the tempo learnt (a beat in; before, its first gap may be far off itself):
                                          * counted, not measured */
        if (grid.clk_due < 0.0f || e < 0.0f)   /* late again, nothing read together since (clocks lost, a pause), or
                                                * early after those (their times lost): its time the clock's from here */
            grid.clk_due = grid.clk_ms;
        else                             /* late: those held back with it may come together, the next due on time */
            grid.clk_due += grid.clk_ms - (float)dt;
        return;
    }
    if (grid.clk_n < 100u) {             /* the least squares' gains over its clk_n + 1 clocks, until they are CLK_A's */
        float m = (float)(grid.clk_n + 1u), am = 2.0f * (2.0f * m - 1.0f) / (m * (m + 1.0f)), bm = 6.0f / (m * (m + 1.0f));
        a = am > a ? am : a;
        b = bm > b ? bm : b;
    }
    grid.clk_ms += b * e;
    grid.clk_due = grid.clk_ms + (a - 1.0f) * e;
}

/* GLINT's step sequencer (GLINT I's PLAYS on SEQ): SC_SEQ steps, a byte each, its note (SQ_REST, a note, or SQ_TIE: the
 * last one held on) and SQ_ACC; SEQ LENGTH of them, played on the arpeggiator's grid (RATE, CHANCE) from the step it
 * started on; the keys transpose it along the scale (RECORD OFF), or write into it (STEP: at STEP, then on; LIVE: on
 * the step nearest the playhead) */
#define SQ_REST 0u
#define SQ_TIE 127u
#define SQ_ACC 0x80u
#define SQT_ARP 1u                       /* (PLAYS' values, P_SQ_TYPE's and P_BQ_ON's: 0 KEYS) */
#define SQT_SEQ 2u
static uint8_t seq[SC_SEQ];

/* GLINT's sequence and the bass's alike, b its steps: a note's LENGTH is its step and the TIEs (the bass's HOLDs) after
 * it. The NOTE step c is, or carries on through its TIEs: its step (-1: none, a rest there, or TIEs after one or from
 * the first step) */
static int32_t step_start(const uint8_t *b, uint32_t c)
{
    int32_t i;
    for (i = (int32_t)(c % SC_SEQ); i >= 0 && (b[i] & 127u) == SQ_TIE; i--)
        ;
    return i >= 0 && (b[i] & 127u) != SQ_REST ? i : -1;
}

/* how far a note from step s goes at most, of `steps` played: to their end (one past it, that does not play yet: to the
 * last step) */
static uint32_t step_lim(uint32_t s, uint32_t steps) { return s < steps ? steps : SC_SEQ; }

static uint32_t step_len(const uint8_t *b, uint32_t s, uint32_t steps)   /* the NOTE at step s, its LENGTH */
{
    uint32_t e, n = step_lim(s, steps);
    for (e = s + 1u; e < n && (b[e] & 127u) == SQ_TIE; e++)
        ;
    return e - s;
}

/* the NOTE at step s, l steps long (one at least, step_lim's at most): TIEs after it, rests where it reaches no more;
 * what it covers now does not go on after it (its TIEs rests, till the next NOTE or rest) */
static void step_stretch(uint8_t *b, uint32_t s, uint32_t l, uint32_t steps)
{
    uint32_t n = step_lim(s, steps), was = step_len(b, s, steps), i;
    l = l < 1u ? 1u : l > n - s ? n - s : l;
    for (i = s + 1u; i < s + l; i++)
        b[i] = SQ_TIE;
    for (i = s + l; i < s + was; i++)
        b[i] = SQ_REST;
    for (i = s + l; i < SC_SEQ && (b[i] & 127u) == SQ_TIE; i++)
        b[i] = SQ_REST;
}

/* a note over the loop's n steps: its NOTE at step s, on through d of them (round the loop's end). What it cuts short
 * does not go on after it from its middle: rests till the next NOTE or rest there was */
static void step_lay(uint8_t *b, uint32_t n, uint32_t s, uint32_t d, uint8_t note)
{
    uint32_t i, j;
    for (i = 0; i < d; i++)
        b[(s + i) % n] = i ? (uint8_t)SQ_TIE : note;
    for (j = (s + d) % n; d < n && (b[j] & 127u) == SQ_TIE; j = (j + 1u) % n, d++)
        b[j] = SQ_REST;
}

static struct {
    uint8_t run;                         /* playing */
    uint8_t restart;                     /* MIDI Start: from its first step, at the grid's top */
    uint8_t off;                         /* its steps this many clocks after the grid's (a key's restart on a beat) */
    uint8_t at_trans;                    /* TRANSPOSE from that restart */
    uint8_t len;                         /* STEP: the LENGTH a note written takes, as the bass's bq.len */
    float at;                            /* a key's restart, due on the grid's beat at this clock (-1: none) */
    int32_t start;                       /* its step its first step fell on (its steps: seq_stepn) */
    int32_t n;                           /* its step at the last run */
    int32_t pos;                         /* the step played last (-1: none yet) */
    int32_t skip;                        /* a step LIVE has just written ahead of the playhead: it sounded already */
    uint8_t near;                        /* its restart keeps skip: step 0 written on the count's downbeat (count_near) */
    int16_t sounding;                    /* its note sounding, -1: none */
    uint32_t off_ms;
    uint32_t steps;                      /* notes played (the tests) */
} sq = {.at = -1.0f, .pos = -1, .skip = -1, .sounding = -1};

/* the grid as GLINT's sequence has it, its steps sq.off clocks after the grid's (wrapping at 9600 as it does), and
 * its step at grid g */
static float seq_grid(float g)
{
    g -= (float)sq.off;
    return g < 0.0f ? g + 9600.0f : g;
}
static int32_t seq_stepn(float g, uint32_t ticks) { return (int32_t)(seq_grid(g) / (float)ticks); }

/* the bass's sequence (PLAYS on SEQ): SC_BSEQ steps on one grid, each RATE long (P_BQ_LEN: BQ_TICKS' index), each a
 * byte (bnote: SQ_REST a CUT, the bass stops; SQ_TIE a HOLD, the note before goes on; else a NOTE, as BASS's keys have
 * it, without its OCTAVE, and SQ_ACC): a note's LENGTH is its step and the HOLDs after it. SEQ LENGTH of them in turn on
 * the arpeggiator's grid (MIDI clock, else TEMPO), from the top with GLINT's sequence. A NOTE sounds until the next NOTE
 * or CUT, or as far into its last step as GATE says; CHANCE may leave it out (the step silent, as a CUT). The keys
 * transpose it along the scale (RECORD OFF), or write into it (STEP: at STEP, the LENGTH set last, then on past it;
 * LIVE: a take, each note as it was played, to the step, over what was there: bseq_take). Till 2.7 each of sixteen
 * steps had a length of its own (bseq_over) */
static uint8_t bnote[SC_BSEQ];

static int bseq_has_notes(void)          /* the bass's sequence has a NOTE in its SEQ LENGTH */
{
    uint32_t i, n = st.v[P_BQ_STEPS] + 1u;
    for (i = 0; i < n && i < SC_BSEQ; i++)
        if ((bnote[i] & 127u) != SQ_REST && (bnote[i] & 127u) != SQ_TIE)
            return 1;
    return 0;
}

static struct {
    uint8_t run;                         /* playing */
    uint8_t len;                         /* STEP: the LENGTH a note written takes, in steps (K3's meanwhile), carried
                                          * on: the note's on STEP as STEP began (else 1), then K3's as it turns */
    uint8_t wait;                        /* begun: its first step at the grid's next step (1: GLINT's, else a
                                          * sixteenth; 2: at once, MIDI Start's top) */
    int8_t pos;                          /* the step playing, -1: none yet */
    float t;                             /* MIDI clocks into it */
    float g;                             /* the grid at the last run */
    int8_t tn;                           /* LIVE: the take's note down (as the sequence keeps it), -1: none; its
                                          * steps silent under it */
    uint8_t ta;                          /* and its ACCENT: SQ_ACC or 0 */
    uint8_t tc;                          /* struck in the count before its last half beat: the take's only if still down on
                                          * the downbeat */
    float tp, te;                        /* where in the loop it went down (clocks from its first step; before it
                                          * began: 0), and the clocks since */
    int8_t skip;                         /* a step the take has just written ahead of the playhead: it sounded
                                          * already, let go (-1: none) */
    int8_t sounding;                     /* the NOTE it struck, as bass_note has it, -1: none (GATE lets that one go) */
    uint8_t acc;                         /* the last it struck had ACCENT: its pressure on */
    uint32_t steps;                      /* NOTEs played (the tests) */
} bq = {.pos = -1, .tn = -1, .skip = -1, .sounding = -1};

/* the drone's sequence (DRONE SEQ on: P_DQ_ON): SC_DSEQ steps on one grid, each RATE long (P_DQ_RATE: BQ_TICKS' index, as
 * the bass's), each the notes it plays (dnote[i]: DQ_NOTES at most, low to high, 0 after the last; none, a rest; DQ_TIE
 * first, a tie: the step before's notes held on), as they were played on the drones (MIDI's numbers, TRANSPOSE off them).
 * SEQ LENGTH of them in turn on the arpeggiator's grid (MIDI clock, else TEMPO), from the top with the others. A step's
 * notes sound as a chord latched does (every layer of the drone, its own envelopes; a note a chord holds too struck
 * once: held_by's), let go as far into its last step as GATE says (a tie: on), or as the next step begins; CHANCE may
 * leave a step out (silent, on its own draws: nothing of GLINT's or the bass's changes). TRANSPOSE moves them along the
 * scale. RECORD (dseq_key): LIVE, what the drones' keys sound (a key, a chord latched or held, a finger, a MIDI note) goes
 * into the step nearest the playhead (the FM-1's, a step of a bar or more: the one under it), beside what it has (none
 * twice, DQ_NOTES at most; the FM-1's, STACK off: a new hand in place of the take's own, dtake_hand), TRANSPOSE off it (it
 * plays as it was played); STEP, into the step on STEP, which moves on once the last key or finger down lets go (a latch
 * adds to it, STEP staying; the FM-1's stays and latches nothing). The keys play as they always do meanwhile (a chord
 * latched stays latched; the FM-1's, till RECORD off lets go of what the take latched: dtake_let_go) */
static uint8_t dnote[SC_DSEQ][DQ_NOTES];
static struct {
    uint8_t run;                         /* playing */
    uint8_t wait;                        /* begun: its first step at the grid's next step (1: GLINT's, else a sixteenth;
                                          * 2: at once, MIDI Start's top) */
    int8_t pos;                          /* the step playing, -1: none yet */
    float t;                             /* MIDI clocks into it */
    float g;                             /* the grid at the last run */
    uint8_t snd[DQ_NOTES], nsnd;         /* the notes it holds now (as they sound: TRANSPOSE on them) */
    uint8_t keys;                        /* RECORDing: the keys and fingers down that wrote (LIVE: the FM-1's alone) */
    uint8_t adds;                        /* the key about to write: SEL held, over what is there, whatever STACK says */
    uint32_t turn;                       /* the turns of its loop begun (a LIVE take's hand: dtake_hand) */
    uint8_t len;                         /* RECORD STEP: the LENGTH a chord written takes, in steps (its TIEs; LENGTH's
                                          * meanwhile), as GLINT's sq.len */
    uint32_t rng;                        /* (its CHANCE's own draws) */
    uint32_t steps;                      /* steps struck (the tests) */
} dq = {.pos = -1, .rng = 0x2545F491u};
static uint32_t drec_f[4];               /* the drones' notes a finger's down wrote into STEP's step (frs_hold) */
static uint32_t dt_lat[4];               /* the FM-1's: the chords (their roots) a LIVE take latched or wrote again: RECORD off
                                          * lets them go (what is heard then is the sequence) */
static void dseq_view(void);            /* (DRONE SEQ II's NOTES and LENGTH, below) */

/* OVERDUB (the iPad's frs_overdub; the FM-1's always on): a LIVE take adds its notes to what was there. Off, its first
 * turn replaces it: each step, as the playhead comes to it, goes (a rest; the bass's, a CUT), what was there heard until
 * then, unless the take has written it; a turn on, it adds. GLINT's take and the bass's, each its own (SC_SEQ and
 * SC_BSEQ: 32 steps, a bit each) */
static uint8_t overdub = 1;
static uint8_t step_hand;                /* the FM-1's way of playing a sequence: RECORD STEP on the drone, a new hand in place
                                          * of the step's, STACK off; its keys never silent after a take (seq_keys, bseq_keys);
                                          * the engine alone, an iPad's: as it was */
static uint8_t step_stays;               /* RECORD STEP: STEP stays where it is (a UI moves it), one note on GLINT's and
                                          * the bass's steps (frs_step_stays: the iPad's; the FM-1's too since 3.0.0-b6) */

/* REC on the FM-1 (eng_record_on): what it turned, to put back when the take comes to nothing (eng_record_off) */
static struct {
    uint8_t on, layer, plays;            /* armed, its track (0 the drone, 1 GLINT, 2 the bass), its PLAYS before */
    uint8_t wrote;                       /* a key has written since */
} rec_was;

/* COUNT-IN's state (its code below, after the MIDI): here for the takes */
static struct {
    uint8_t run;                         /* counting */
    uint8_t beat;                        /* the beats ticked, 0 .. 4 */
    uint8_t glint;                       /* a key near the downbeat wrote GLINT's first step: it sounded then */
    float t;                             /* beats since it began */
    uint32_t at;                         /* the clock when it last moved on (count_run may come at any pace: the FM-1's
                                          * main loop is free-running) */
} cnt;

/* the count's last half beat, on the FM-1: a key here belongs to the downbeat, the take's first step (hands land either
 * side of a beat) */
static int count_near(void) { return step_hand && cnt.run && cnt.t >= 3.5f; }

typedef struct {
    uint8_t on;                          /* the take under way: RECORD LIVE, its sequence playing */
    uint8_t left;                        /* its first turn's steps still to come, to replace (0: over, or OVERDUB) */
    uint32_t wrote;                      /* the steps it has written (bit i: step i) */
} take_t;
static take_t gtk, btk, dtk;             /* GLINT's, the bass's, the drone's */

/* a take as it goes (on: RECORD LIVE, its sequence playing; n: its steps): begun now, OVERDUB off, its turn to replace;
 * else it goes on, or there is none */
static void take_run(take_t *t, int on, uint32_t n)
{
    if (!on) {
        t->on = t->left = 0;
    } else if (!t->on) {
        t->on = 1;
        t->left = (uint8_t)(overdub ? 0u : n);
        t->wrote = 0;
    }
}

/* its sequence begun again (PLAY, a scene, MIDI Start, played in phase with the other): a new take from there, its
 * own first turn; or its steps put back or cleared (UNDO, CLEAR, NEW TAKE): from there it adds */
static void take_new(take_t *t) { t->on = t->left = 0; }
static void take_adds(take_t *t) { t->left = 0; }

/* step i reached, the take's first turn (b: its steps): as it was, gone (to `rest`), unless the take wrote it. 1: it
 * went. OVERDUB turned on meanwhile: it adds from there */
static int take_reach(take_t *t, uint8_t *b, uint32_t i, uint8_t rest)
{
    int gone = 0;
    if (overdub)
        t->left = 0;
    if (!t->left)
        return 0;
    t->left--;
    if (!(t->wrote >> (i & 31u) & 1u) && b[i] != rest) {
        b[i] = rest;
        gone = 1;
    }
    return gone;
}

/* GLINT's keys into its sequencer (seq_key): the sequencer on, recording (from any page), or not where the player tries
 * a line (eng_trying: the FM-1's GLINT SEQ pages; elsewhere they transpose it). There, RECORD OFF, each key plays its
 * glint, as with SEQ OFF: a line tried before it is recorded, the sequence going on as it was */
static uint32_t seq_len(void);           /* (below) */

static int seq_has_notes(void)           /* GLINT's sequence has a note in the steps it plays */
{
    uint32_t i, n = seq_len();
    for (i = 0; i < n && i < SC_SEQ; i++)
        if ((seq[i] & 127u) != SQ_REST && (seq[i] & 127u) != SQ_TIE)
            return 1;
    return 0;
}

/* GLINT's keys (and notes) into its sequencer: SEQ on SEQ, RECORDing; RECORD OFF, not trying a line, a sequence with
 * notes playing, to transpose (stopped on the FM-1, or empty: the keys play their glints, as with SEQ OFF) */
static int seq_keys(void)
{
    return st.v[P_SQ_TYPE] == SQT_SEQ &&
           (st.v[P_SQ_REC] || ((sq.run || !step_hand) && !eng_trying(1u) && seq_has_notes()));
}

/* the bass is its sequence's: PLAYS on SEQ, RECORDing, or with notes playing (the engine alone's: stopped too). Not: a
 * bass note there is the keys' */
static int bseq_owns(void)
{
    return st.v[P_BQ_ON] == SQT_SEQ && (st.v[P_BQ_REC] || ((bq.run || !step_hand) && bseq_has_notes()));
}

/* BASS's keys (and notes) into its sequence: it owns the bass, and on the FM-1 not where the player tries a line: there,
 * RECORD OFF, they play the bass, its steps silent while one is down (bseq_step) */
static int bseq_keys(void) { return bseq_owns() && (st.v[P_BQ_REC] || !step_hand || !eng_trying(2u)); }

/* -------------------------------------------------------------- engine --- */
static float delay_s_now = 0.25f;        /* GLINT's delay time from the tempo (delay_sync) */
#if FRS_LEAD
static float ld_delay_s = 0.25f;         /* LEAD's, the same way (card #144) */
#endif

static uint8_t saved_bsp;                /* (the bass's SPACE, as saved_bq has the rest: scene_t's last) */
static uint8_t saved_gv;                 /* (GLINT's AUTO VIB, the same) */
static uint8_t saved_tp[4];              /* (LOFI TAPE II's four, the same: scene_t's tp_flutter ..) */
static uint8_t saved_amt[SC_LFO_AMT], saved_sw;   /* (the LFOs' AMOUNTs, SWELL, the same: scene_t's lfo_amt, swell) */
#if FRS_LEAD
#define NLDS (P_LD_FRELEASE - P_LD_WAVE + 1)   /* LEAD's settings a scene keeps (card #144, app.c ldp_*: its own object) */
static uint8_t saved_ld[NLDS];           /* (LEAD's, the same: P_LD_WAVE .. P_LD_FRELEASE, its MIXER level, PRESET with
                                          * them, the filter's ADSR last) */
#endif
static uint8_t saved_v[NSOUND], saved_seq[SC_SEQ], saved_bnote[SC_BSEQ], saved_bq[NBQ];   /* (the sound as last loaded or
                                                                                     * saved; saved_bq: as scene_t has
                                                                                     * them from bq_on) */
static uint8_t saved_dnote[SC_DSEQ][DQ_NOTES], saved_dq[NDQ];   /* (the drone's sequence, the same; saved_dq: from dq_on) */
static void drone_resolve(uint8_t *v);
/* TUNE A4, the platform's and not the scene's: log2 of A4 over 440 Hz, every voice's pitch (the notes as they are); 0,
 * 440 Hz, but where the FM-1's shell sets it (its settings, app.c a4_take) */
static float a4_l2;

static void engine_push(void)
{
    frasil_params_t b;
    seq_shift = 12 * (int32_t)st.v[P_GL_OCTAVE];   /* (NOTE shows the steps as they sound) */
    bseq_shift = 12 * ((int32_t)st.v[P_BS_OCTAVE] - 2);
    if (st.compare) {                    /* EDIT held: the scene as it was saved, the rest as now */
        static uint8_t v[NPARAM];
        uint32_t i;
        for (i = 0; i < NPARAM; i++)
            v[i] = i < NSOUND ? saved_v[i] : st.v[i];
        drone_resolve(v);
        params_to_engine(v, st.v[P_FREEZE], &b);
    } else {
        params_to_engine(st.v, st.v[P_FREEZE], &b);
    }
    b.a4_l2 = a4_l2;
    b.glint.delay_s = delay_s_now;
#if FRS_LEAD
    b.lead.dtime_s = ld_delay_s;
#endif
    b.tape_l2 = tape_l2;                 /* TAPE SPEED as it glides (tape_run): the pitch, and the times unless clocked */
    b.time_l2 = tape_clocked ? 0.0f : tape_l2;
    tape_sent = tape_l2;
    frasil_set(&b);
    st.engine_dirty = 0;
}

static void chord_on(const chord_t *c, uint32_t vel)
{
    uint32_t i;
    for (i = 0; i < c->k; i++)
        if (held_by[(uint8_t)c->n[i]]++ == 0)
            frasil_note_on(c->n[i], (float)vel * (1.0f / 127.0f));
}

static void chord_off(const chord_t *c)
{
    uint32_t i;
    for (i = 0; i < c->k; i++)
        if (held_by[(uint8_t)c->n[i]] && --held_by[(uint8_t)c->n[i]] == 0)
            frasil_note_off(c->n[i]);
}

static void chord_press(const chord_t *c, float p)
{
    uint32_t i;
    for (i = 0; i < c->k; i++)
        frasil_pressure(c->n[i], p);
}

/* a bass note as it sounds: OCTAVE on it (the engine puts it there: bass.c), C1 at the lowest (under it, nothing a
 * speaker plays) */
static int bass_sounding(int note)
{
    note += 12 * ((int)st.v[P_BS_OCTAVE] - 2);
    while (note < 24)
        note += 12;
    return note > 127 ? 127 : note;
}

/* BASS II's PULSE (pulse_run): the grid at its last run; where on it the bass's note was played last, and whether that
 * was a moment ago (three clocks at most: fresh); the notes played again, and the division the last fell on (the
 * tests) */
static struct {
    float g, ng, at;
    uint8_t fresh;
    uint32_t n;
#ifdef PLAT_HOST
    uint32_t seen;                       /* (the tests: the engine's strikes seen, and how late the last fell: ms after
                                          * its division) */
    int32_t late;
#endif
} bp;

static void bass_play(int note, uint32_t vel)   /* note: as played (the engine puts OCTAVE on it) */
{
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (note != bass_note && bass_note >= 0)
        frasil_bass_pressure(0.0f);      /* another note: its own key's pressure, from nothing */
    bass_note = (int8_t)note;
    frasil_bass_on(bass_note, (float)vel * (1.0f / 127.0f));
    bp.ng = grid.tick;
    bp.fresh = 1;
}

/* a note played by hand (a key, a MIDI note): with PULSE on, a note sounding (or in its gap) goes to it, not struck:
 * PULSE strikes it on its next division, the beat kept (the same note: nothing); else bass_play's */
static void bass_live(int note, uint32_t vel)
{
    if (!(st.v[P_BS_PULSE] % NPULSE) || bass_note < 0) {
        bass_play(note, vel);
        return;
    }
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (note == bass_note)
        return;
    frasil_bass_pressure(0.0f);          /* (another note: its own key's pressure, from nothing) */
    bass_note = (int8_t)note;
    frasil_bass_retune(bass_note, (float)vel * (1.0f / 127.0f));
}

static void bass_stop(void)
{
    if (bass_note >= 0)
        frasil_bass_off();
    bass_note = -1;
}

/* the bass's settings a scene keeps, as it has them from bq_on */
static void bq_now(uint8_t *q)
{
    static const uint8_t P[NBQ] = {P_BQ_ON, P_BQ_STEPS, P_BQ_TRANS, P_TEMPO, P_BQ_LEN, P_BA_MODE, P_BA_RANGE, P_BA_CHANCE,
                                   P_BQ_GATE, P_BS_WEIGHT, P_BS_PUNCH, P_BS_BITE};
    uint32_t i;
    for (i = 0; i < NBQ; i++)
        q[i] = st.v[P[i]];
}

/* the drone sequence's settings a scene keeps, as it has them from dq_on */
static void dq_now(uint8_t *q)
{
    static const uint8_t P[NDQ] = {P_DQ_ON, P_DQ_RATE, P_DQ_LEN, P_DQ_GATE, P_DQ_CHANCE, P_DQ_TRANS};
    uint32_t i;
    for (i = 0; i < NDQ; i++)
        q[i] = st.v[P[i]];
}

static void midi_glints_off(void);       /* (MIDI, below) */

static void release_drones(void)
{
    uint32_t k;
    frasil_all_off();
    for (k = 0; k < 128u; k++)
        held_by[k] = 0;
    dq.nsnd = 0;                         /* (the drone's sequence's notes too: its next step strikes its own) */
    for (k = 0; k < 4u; k++)
        lit[k] = mid_on[k] = sus[k] = 0;
    for (k = 0; k < FRS_NKEYS; k++)          /* keys still down hold nothing now */
        if (key[k].kind == K_LATCH || key[k].kind == K_HOLD)
            key[k].kind = K_NONE;
}

static void release_all(void)
{
    uint32_t k;
    release_drones();
    bass_stop();
    mb_n = 0;
    midi_glints_off();
    frasil_glint_off(-1);                /* (and every glint still ringing: keys', SPARKLE's) */
    rung.n = 0;
    for (k = 0; k < FRS_NKEYS; k++)
        if (key[k].kind == K_BASS || key[k].kind == K_BSEQ)
            key[k].kind = K_NONE;
}

/* the sound as it was last loaded or saved: SAVE lights while the sound differs from it (back to it, it goes out) */
/* the drone's CUSTOM: its settings as they were last turned, kept while a preset plays. A sound as it is kept (the
 * scenes, the settings) has CUSTOM's settings for the drone's and the preset's number: a preset untouched is its table's */
static uint8_t custom[NDRONE];
static int8_t drone_of[NPARAM];          /* a setting's place in DRONE_P, -1: not the drone's (drone_init) */

static void drone_init(void)
{
    uint32_t i;
    for (i = 0; i < NPARAM; i++)
        drone_of[i] = -1;
    for (i = 0; i < NDRONE; i++)
        drone_of[DRONE_P[i]] = (int8_t)i;
}

static void drone_apply(uint32_t k)      /* preset k (0: CUSTOM) into the settings */
{
    uint32_t i;
    for (i = 0; i < NDRONE; i++)
        st.v[DRONE_P[i]] = k ? DRONE_PRESET[(k - 1u) % NDPRESET][i] : custom[i];
    st.engine_dirty = 1;
}

static void drone_take(void)             /* a sound just loaded into st.v: its drone's settings CUSTOM's, then its
                                          * preset's on top */
{
    uint32_t i;
    for (i = 0; i < NDRONE; i++)
        custom[i] = st.v[DRONE_P[i]];
    if (st.v[P_DR_PRESET])
        drone_apply(st.v[P_DR_PRESET]);
}

static uint8_t kept_v(uint32_t p) { return drone_of[p] >= 0 ? custom[drone_of[p]] : st.v[p]; }   /* as it is kept */

static void drone_resolve(uint8_t *v)    /* a sound as it is kept, its drone's preset (if any) onto its settings */
{
    uint32_t i;
    if (v[P_DR_PRESET])
        for (i = 0; i < NDRONE; i++)
            v[DRONE_P[i]] = DRONE_PRESET[(v[P_DR_PRESET] - 1u) % NDPRESET][i];
}

static void tp_now(uint8_t *o)           /* LOFI TAPE II's four as they are: TAPE SPEED, TONE, NOISE, DROPOUT */
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        o[i] = st.v[P_TP_FLUTTER + i];
}

static void sound_mark(void)
{
    uint32_t i;
    for (i = 0; i < NSOUND; i++)
        saved_v[i] = kept_v(i);
    for (i = 0; i < SC_SEQ; i++)
        saved_seq[i] = seq[i];
    memcpy(saved_bnote, bnote, sizeof bnote);
    bq_now(saved_bq);                    /* (the scene's TEMPO too: scene_t's, after bq_trans) */
    saved_bsp = st.v[P_BS_SPACE];
    saved_gv = st.v[P_GL_VIB_AUTO];
    tp_now(saved_tp);
    memcpy(saved_amt, &st.v[P_L1_AMT1], sizeof saved_amt);
    saved_sw = st.v[P_SWELL];
    memcpy(saved_dnote, dnote, sizeof dnote);
    dq_now(saved_dq);
#if FRS_LEAD
    memcpy(saved_ld, &st.v[P_LD_WAVE], sizeof saved_ld);
#endif
    st.dirty = 0;
}

static int sound_changed(void)
{
    uint32_t i;
    uint8_t q[NBQ], d[NDQ], t[4];
    for (i = 0; i < NSOUND; i++)
        if (kept_v(i) != saved_v[i])
            return 1;
    for (i = 0; i < SC_SEQ; i++)
        if (seq[i] != saved_seq[i])
            return 1;
    bq_now(q);
    dq_now(d);
    tp_now(t);
#if FRS_LEAD
    if (memcmp(&st.v[P_LD_WAVE], saved_ld, sizeof saved_ld))
        return 1;
#endif
    return memcmp(bnote, saved_bnote, sizeof bnote) || memcmp(q, saved_bq, sizeof q) || st.v[P_BS_SPACE] != saved_bsp ||
           st.v[P_GL_VIB_AUTO] != saved_gv || memcmp(t, saved_tp, sizeof t) || memcmp(dnote, saved_dnote, sizeof dnote) || memcmp(d, saved_dq, sizeof d) ||
           memcmp(&st.v[P_L1_AMT1], saved_amt, sizeof saved_amt) || st.v[P_SWELL] != saved_sw;
}

/* the sequencer's settings that are its steps' (NOTE, ACCENT and LENGTH show the step on STEP, as the bass's: bseq_view),
 * CLEAR, MODE */
static void seq_view(void)
{
    uint32_t c = st.v[P_SQ_STEP] % SC_SEQ;
    int32_t s = step_start(seq, c);
    uint8_t b = seq[c];
    st.v[P_SQ_NOTE] = b & 127u;
    st.v[P_SQ_ACC] = b >> 7;
    st.v[P_SQ_NLEN] = (uint8_t)(st.v[P_SQ_REC] == 1u ? sq.len : s >= 0 ? step_len(seq, (uint32_t)s, st.v[P_SQ_LEN] + 1u) : 0u);
}

static void seq_note_off(void)
{
    if (sq.sounding >= 0)
        frasil_glint_off(sq.sounding);
    sq.sounding = -1;
}

static void seq_begin(void)              /* from its first step, at the grid's next (its steps the grid's again) */
{
    seq_note_off();
    sq.run = 1;
    sq.off = 0;
    sq.at = -1.0f;
    sq.n = arp.step_n;
    sq.start = arp.step_n + 1;
    sq.pos = sq.skip = -1;
    take_new(&gtk);
}

static void rec_keys(int p);
static uint8_t seq_prev[SC_SEQ], seq_had;   /* the sequence before the last take or CLEAR (UNDO) */
static uint8_t undone[3];                /* each track (0 the drone, 1 GLINT, 2 the bass): UNDO TAKE has just gone back */
static void seq_keep(void)
{
    memcpy(seq_prev, seq, sizeof seq_prev);
    seq_had = 1;
    undone[1] = 0;
}

/* UNDO TAKE's word: turned again it brings the take back (the FM-1's say so; the engine alone's as it was) */
static const char *undo_word(uint32_t t)
{
    undone[t] ^= 1u;
    return step_hand && !undone[t] ? "REDONE" : "UNDONE";
}

static void seq_set(int p, int32_t x)
{
    uint32_t c = st.v[P_SQ_STEP] % SC_SEQ, n = st.v[P_SQ_LEN] + 1u;
    if (p == P_SQ_STEP || p == P_SQ_LEN) {
        seq_view();
    } else if (p == P_SQ_NOTE) {
        seq[c] = (uint8_t)((seq[c] & SQ_ACC) | (uint32_t)x);
        seq_view();
    } else if (p == P_SQ_ACC) {
        seq[c] = (uint8_t)((seq[c] & 127u) | (x ? SQ_ACC : 0u));
    } else if (p == P_SQ_NLEN) {         /* LENGTH: the note on STEP's (on a TIE, the one it carries on, as far as STEP at
                                          * least; on a rest, none); recording on STEP, the length carried on, and the
                                          * NOTE on STEP's at once: as the bass's */
        int32_t s = step_start(seq, c);
        if (st.v[P_SQ_REC] == 1u) {
            sq.len = (uint8_t)(x < 1 ? 1 : x);
            if (s == (int32_t)c)
                step_stretch(seq, c, sq.len, n);
        } else if (s >= 0) {
            step_stretch(seq, (uint32_t)s, (uint32_t)(x > (int32_t)c - s + 1 ? x : (int32_t)c - s + 1), n);
        }
        seq_view();
    } else if (p == P_SQ_REC) {          /* STEP: the length carried on, the note on STEP's to begin with (else 1) */
        if (x == 1)
            sq.len = (uint8_t)(step_start(seq, c) == (int32_t)c ? step_len(seq, c, n) : 1u);
        seq_view();
    } else if (p == P_SQ_CLEAR && x >= 2) {   /* turned twice: every step a rest */
        seq_keep();
        memset(seq, 0, sizeof seq);
        take_adds(&gtk);
        st.v[P_SQ_CLEAR] = 0;
        seq_view();
        eng_say("SEQUENCE", "CLEARED");
    } else if (p == P_SQ_NEW && x >= 2) {   /* turned twice: a new take, recording LIVE from rests */
        seq_keep();
        memset(seq, 0, sizeof seq);
        take_adds(&gtk);
        st.v[P_SQ_NEW] = 0;
        st.v[P_SQ_REC] = 2;
        rec_keys(P_SQ_REC);
        if (!sq.run)
            seq_begin();
        seq_view();
        eng_say("NEW TAKE", "RECORDING");
    } else if (p == P_SQ_UNDO && x >= 2) {   /* turned twice: the sequence before the last take or CLEAR (again:
                                              * back) */
        st.v[P_SQ_UNDO] = 0;
        take_adds(&gtk);
        if (seq_had) {
            uint8_t t[SC_SEQ];
            memcpy(t, seq, sizeof t);
            memcpy(seq, seq_prev, sizeof seq);
            memcpy(seq_prev, t, sizeof t);
            seq_view();
            eng_say("SEQUENCE", undo_word(1u));
        } else {
            eng_say("UNDO TAKE", "NOTHING YET");
        }
    } else if (p == P_SQ_TYPE) {
        if (x == (int32_t)SQT_SEQ)
            seq_begin();
        else {
            seq_note_off();
            sq.run = 0;
            if (st.v[P_SQ_REC]) {        /* (no sequencer, nothing to record into) */
                st.v[P_SQ_REC] = 0;
                rec_keys(P_SQ_REC);
            }
        }
        arp.paused = 0;
        arp.dirty = 1;                   /* (ARP again: its notes from the next step) */
        eng_type_changed();              /* (its pages, the FM-1's, gone) */
    }
    st.dirty = (uint8_t)sound_changed();
}

/* NOTE turned up from a REST at step c of n: the last note before it, round the loop (else the ROOT in the third octave,
 * as a chord asked of NOTES has it), not the range's bottom */
static int step_near(const uint8_t *b, uint32_t c, uint32_t n)
{
    uint32_t i;
    for (i = 1; i < n; i++) {
        uint32_t v = b[(c + n - i) % n] & 127u;
        if (v != SQ_REST && v != SQ_TIE)
            return (int)v;
    }
    return 48 + (int)(st.v[P_ROOT] % 12u);
}

/* --------------------------------------------------- the bass's sequence --- */
static void param_set(int p, int32_t x);   /* (below) */
static uint32_t bseq_ticks(void) { return BQ_TICKS[st.v[P_BQ_LEN] % NBQLEN]; }   /* a step, in MIDI clocks */

/* NOTE, ACCENT and LENGTH show the step on STEP: its note, and the LENGTH of the NOTE it is or carries on (-: none).
 * Recording on STEP, LENGTH shows the length carried on instead (bq.len), so that it always says what the next key will
 * write */
static void bseq_view(void)
{
    uint32_t c = st.v[P_BQ_STEP] % SC_BSEQ;
    int32_t s = step_start(bnote, c);
    st.v[P_BQ_NOTE] = bnote[c] & 127u;
    st.v[P_BQ_ACC] = bnote[c] >> 7;
    st.v[P_BQ_NLEN] = (uint8_t)(st.v[P_BQ_REC] == 1u ? bq.len
                                : s >= 0 ? step_len(bnote, (uint32_t)s, st.v[P_BQ_STEPS] + 1u) : 0u);
}

/* a step's note as it plays: TRANSPOSE's steps along the scale (OCTAVE on it in the engine, as on every note) */
static int bseq_note(uint32_t b)
{
    return scale_shift((int)(b & 127u), (int)st.v[P_BQ_TRANS] - 14, st.v);
}

/* the newest BASS key down into the sequence (else MIDI's last note), as the sequence keeps it; -1: none */
static int bseq_held(void)
{
    int32_t k, last = -1;
    for (k = 0; k < (int32_t)FRS_NKEYS; k++)
        if (key[k].kind == K_BSEQ && (last < 0 || key[k].t - key[last].t < 0x80000000u))
            last = k;
    if (last >= 0)
        return key[last].gbase;
    return mb_n ? mbass[mb_n - 1] : -1;
}

/* a step's NOTE plays (byte b): ACCENT struck at full velocity and pressed in as a key is, brighter and louder (the
 * bass's velocity alone moves it under a decibel); without it, as a NOTE always was */
#define BQ_ACCENT 1.0f
static void bseq_strike(uint32_t b)
{
    int acc = (b & SQ_ACC) != 0u;
    bass_play(bseq_note(b), acc ? 127u : 100u);
    if (acc || bq.acc)
        frasil_bass_pressure(acc ? BQ_ACCENT : 0.0f);
    bq.acc = (uint8_t)acc;
    bq.sounding = (int8_t)bass_note;
    bq.steps++;
}

/* the playhead in the loop, in MIDI clocks from its first step */
static float bseq_at(void) { return bq.t + (float)bseq_ticks() * (float)(bq.pos > 0 ? bq.pos : 0); }

/* (frasil_engine.c's bass_join: the playhead put at clock c, whatever step it falls in) */
static void bseq_seek(float c)
{
    float t = (float)bseq_ticks();
    int32_t i;
    for (i = 0; i < (int32_t)st.v[P_BQ_STEPS] && c >= t; i++)
        c -= t;
    bq.pos = (int8_t)i;
    bq.t = c;
}

static int bseq_playing(void) { return bq.run && !bq.wait && bq.pos >= 0; }   /* (begun: a playhead) */

/* ------------------------------------------------------- a LIVE take --- */
/* A take keeps each note as it was played, its start and its length to the step (one at least), laid over the loop
 * once it ends (let go, or another key taking over): its NOTE, then HOLDs as long as it was held, over what was there
 * (step_lay) */

/* a take's note into the loop: from step s, d long, round its end, SEQ LENGTH as it is. Ahead (let go before its start
 * came): its step sounded already (bq.skip) */
static void bseq_take(uint32_t s, uint32_t d, uint8_t note, int ahead)
{
    uint32_t n = st.v[P_BQ_STEPS] + 1u, i;
    s %= n;
    d = d < n ? d : n;
    step_lay(bnote, n, s, d, note);
    rec_was.wrote = 1;
    for (i = 0; i < d; i++)              /* (the take's own: its first turn replaces them no more) */
        btk.wrote |= 1u << ((s + i) % n & 31u);
    bseq_view();
    st.dirty = (uint8_t)sound_changed();
    bq.skip = ahead ? (int8_t)s : -1;
}

/* the step the playhead began a moment ago (at most `late` clocks, half a step at most), silent under a key: its NOTE
 * now */
static void bseq_catch(float late)
{
    uint32_t b;
    if (!bseq_playing() || bq.t > late || bq.t >= 0.5f * (float)bseq_ticks())
        return;
    b = bnote[bq.pos] & 127u;
    if (b != SQ_REST && b != SQ_TIE)
        bseq_strike(bnote[bq.pos]);
}

/* the take's note ends (let go; legato: another taking over, this one to where that one starts, to the step), into
 * the loop. Let go after its end (by less than half a step), the step begun there since sounds now */
static void bseq_end(int legato)
{
    float t = (float)bseq_ticks(), e = bq.tp + bq.te, late;
    int32_t s = (int32_t)fm_floorf(bq.tp / t + 0.5f);
    int32_t d = (int32_t)fm_floorf((legato ? e : bq.te) / t + 0.5f) - (legato ? s : 0);
    uint8_t note = (uint8_t)((uint32_t)bq.tn | bq.ta);
    bq.tn = -1;
    if (legato && d < 1)                 /* (the next one starts where it did: it replaces it) */
        return;
    d = d < 1 ? 1 : d;
    bseq_take((uint32_t)(s < 0 ? 0 : s), (uint32_t)d, note, e <= t * (float)s);
    late = e - t * (float)(s + d);
    if (!legato && late >= 0.0f)
        bseq_catch(late);
}

/* the take ends: RECORD off LIVE, the sequence stopped or begun again. Its note down ends now */
static void bseq_close(void)
{
    if (bseq_playing() && bq.tn >= 0)
        bseq_end(0);
    bq.tn = -1;
}

static void bseq_begin(void)             /* from its first step, at the grid's next (a take under way ended) */
{
    bseq_close();
    bq.run = bq.wait = 1;
    bq.pos = bq.skip = -1;
    take_new(&btk);
}

static void bseq_stop(void)              /* its note let go (a take under way ended; an empty one's: the keys') */
{
    bseq_close();
    if (bq.run && bseq_has_notes())
        bass_stop();
    bq.run = 0;
    bq.pos = -1;
}

static int keys_down(uint32_t kind, uint32_t except);   /* (below) */

/* step i begins: its NOTE plays (GLIDE from the last), unless CHANCE leaves it out (silent, as a CUT, on its own RNG:
 * nothing of GLINT's changes), its CUT stops the bass, a HOLD changes nothing. LIVE, none of it while the take's note is
 * down (it has the bass), nor on the step a note just let go has written ahead; OVERDUB off, the take's first turn,
 * what was there gone first (a CUT) */
#ifdef PLAT_HOST
static float bq_beat;
#endif
static void bseq_step(int32_t i)
{
    uint32_t b;
    int quiet = bq.tn >= 0 || i == bq.skip || (step_hand && keys_down(K_BASS, FRS_NKEYS));   /* (a line tried over it) */
    take_run(&btk, st.v[P_BQ_REC] == 2u && st.v[P_BQ_ON] == SQT_SEQ, st.v[P_BQ_STEPS] + 1u);
    if (take_reach(&btk, bnote, (uint32_t)i, SQ_REST)) {
        bseq_view();
        st.dirty = (uint8_t)sound_changed();
    }
    b = bnote[i] & 127u;
    bq.pos = (int8_t)i;
    bq.skip = -1;
    if (quiet)
        return;
    if (b == SQ_REST) {
        if (bseq_has_notes())            /* (an empty one cuts nothing: the bass is the keys') */
            bass_stop();
    } else if (b != SQ_TIE) {
        barp.rng ^= barp.rng << 13;
        barp.rng ^= barp.rng >> 17;
        barp.rng ^= barp.rng << 5;
        if (barp.rng % 127u >= st.v[P_BA_CHANCE]) {
            bass_stop();
            return;
        }
        bseq_strike(bnote[i]);
#ifdef PLAT_HOST
        bq_beat = grid.tick;              /* (the tests: where on the grid the NOTE began) */
#endif
    }
}

/* on the grid, after the arpeggiator has moved it (grid.tick): the steps that began since the last run, at the loop's
 * end round */
static void bseq_run(void)
{
    float g = grid.tick, dg = g - bq.g, b;
    if (dg < -4800.0f)                   /* (the grid wraps at 9600) */
        dg += 9600.0f;
    else if (dg < 0.0f)
        dg = 0.0f;
    bq.g = g;
    if (!bq.run)
        return;
    if (bq.wait == 1) {                  /* begun: on the grid's next step, GLINT's while its sequence runs (the two
                                          * from the top together), else the next sixteenth */
        float h = g;
        b = 6.0f;
        if (st.v[P_SQ_TYPE] == SQT_SEQ && sq.run) {
            b = (float)ARP_TICKS[st.v[P_ARP_RATE] % 10u];
            h = seq_grid(g);             /* (its steps where GLINT's sequence has them) */
        }
        if (fm_floorf(h / b) == fm_floorf((h - dg) / b))
            return;
        bq.t = h - b * fm_floorf(h / b);
    } else if (bq.wait == 2) {           /* MIDI Start: from the grid's top */
        bq.t = g;
    } else {
        bq.t += dg;
    }
    if (bq.tn >= 0)                      /* (the take's note down: its time, from the first step if it came first) */
        bq.te = bq.wait ? bq.t : bq.te + dg;
    if (bq.wait) {
        bq.wait = 0;
        bseq_step(0);
    }
    while (bq.t >= (float)bseq_ticks()) {
        bq.t -= (float)bseq_ticks();
        bseq_step(bq.pos < (int32_t)st.v[P_BQ_STEPS] ? bq.pos + 1 : 0);
    }
    if (st.v[P_BQ_GATE] < 127u && bq.sounding >= 0 && bq.sounding == bass_note && bq.pos >= 0 &&   /* GATE: its NOTE let go
                                                                                              * in its last step (a
                                                                                              * HOLD next: on) */
        (bnote[bq.pos < (int32_t)st.v[P_BQ_STEPS] ? bq.pos + 1 : 0] & 127u) != SQ_TIE &&
        bq.t >= (float)bseq_ticks() * (0.05f + 0.95f * (float)st.v[P_BQ_GATE] * (1.0f / 127.0f))) {
        bass_stop();
        bq.sounding = -1;
    }
}

/* BASS II's PULSE: while the bass's note sounds (a key held or latched, a MIDI note, a NOTE of its sequence: bass_note),
 * on each of PULSE's divisions of the grid (MIDI clock, else TEMPO: grid.tick, from its beats), the note played again
 * from its ATTACK right on the division (frasil_bass_pulse: the same note and velocity, its pressure on, no glide), let
 * go a moment before it: the gap, a quarter of the division, 8 ms at least, 60 ms at most (a short breath, whatever the
 * tempo: at 120 BPM 16 ms on an eighth of a beat, 31 ms on a quarter, 60 ms from half a beat on). So the strikes fall
 * on the beat, as the sequence's NOTEs do. A step of the bass's sequence beginning on the division is its own: a NOTE
 * strikes there itself, a CUT ends the note (none then; a HOLD, the note going on, has its strike). A note played less
 * than a clock before the gap begins, or in it (a key, a MIDI note), is its attack already: none then */
/* the step of the bass's sequence that begins on the grid at `at` (within half a clock; g: the grid now), or -1 */
static int32_t bseq_step_at(float g, float at)
{
    float d;
    if (!bq.run || bq.wait || bq.tn >= 0)
        return -1;
    d = g + (float)bseq_ticks() - bq.t - at;
    if (d < -0.5f || d > 0.5f)
        return -1;
    return bq.pos < (int32_t)st.v[P_BQ_STEPS] ? bq.pos + 1 : 0;
}

static void pulse_run(void)
{
    uint32_t r = st.v[P_BS_PULSE] % NPULSE;
    float g = grid.tick, was = bp.g, per = (float)PULSE_TICKS[r], at, ms_tick, gap, lead, left;
    int fresh = bp.fresh;
    bp.g = g;
    if (bp.fresh) {
        float d = g - bp.ng;
        bp.fresh = (d < 0.0f ? d + 9600.0f : d) <= 3.0f;   /* (the grid wraps at 9600) */
    }
    ms_tick = grid_ms_tick(eng_ms());
#ifdef PLAT_HOST
    if (r) {
        bass_view_t bv;
        frasil_bass_view(&bv);
        if (bv.pulses != bp.seen) {      /* a strike: where it fell, from its division */
            bp.seen = bv.pulses;
            bp.late = (int32_t)fm_floorf((g - per * fm_floorf(g / per + 0.5f)) * ms_tick + 0.5f);
        }
    }
#endif
    if (!r || bass_note < 0)
        return;
    gap = 0.25f * per * ms_tick;
    gap = gap < 8.0f ? 8.0f : gap > 60.0f ? 60.0f : gap;
    lead = gap / ms_tick;                /* the gap in clocks: the grid that far ahead, the division about to come */
    if (g < was - 4800.0f)
        was -= 9600.0f;                  /* (wrapped: 9600 is every division's multiple) */
    if (fm_floorf((g + lead) / per) == fm_floorf((was + lead) / per))
        return;
    at = per * fm_floorf((g + lead) / per);
    if (st.v[P_BQ_ON] == SQT_ARP && (uint32_t)at % bseq_ticks() == 0u)   /* (the bass's arpeggio's step there: its own) */
        return;
    {
        int32_t nx = bseq_step_at(g, at);
        uint32_t b = nx >= 0 ? bnote[nx] & 127u : SQ_TIE;
        if (b != SQ_TIE && (b != SQ_REST || bseq_has_notes()))
            return;
    }
    if (fresh) {
        float d = g - bp.ng;
        d = d > 4800.0f ? d - 9600.0f : d < -4800.0f ? d + 9600.0f : d;
        if (d <= 1.0f)
            return;
    }
    left = (at - g) * ms_tick;           /* (to the division: the strike there, whatever the clock's steps) */
    frasil_bass_pulse((left < 8.0f ? 8.0f : left > 60.0f ? 60.0f : left) * 0.001f);
    bp.n++;
    bp.at = at >= 9600.0f ? at - 9600.0f : at;
}

/* a BASS key or note while the sequence is on (n: as the sequence keeps it, without OCTAVE; sound: as it sounds; vel 100
 * or more: ACCENT, as GLINT's). RECORD OFF: the sequence transposed from its first NOTE to this one along the scale;
 * STEP: written at STEP, the LENGTH carried on (bq.len: HOLDs after it, to SEQ LENGTH's end at most), then on past it (it
 * sounds while down); LIVE:
 * it sounds at once and, the sequence running, goes into the take from where it went down (the one down before it ends
 * there, legato) */
static void bseq_key(int n, int sound, uint32_t vel)
{
    uint32_t len = st.v[P_BQ_STEPS] + 1u, i;
    int w = st.v[P_BQ_REC] ? scale_shift(n, 14 - (int)st.v[P_BQ_TRANS], st.v) : n;   /* (RECORDing: written so that,
                                                                                      * TRANSPOSE on it, it plays as played) */
    uint8_t b = (uint8_t)((w < 1 ? 1 : w > 126 ? 126 : w) | (vel >= 100u ? SQ_ACC : 0u));
    if (st.v[P_BQ_REC] == 1u) {
        uint32_t c = st.v[P_BQ_STEP] % len, l = step_stays ? 1u : bq.len ? bq.len : 1u;
        l = l < len - c ? l : len - c;
        step_lay(bnote, len, c, l, b);
        if (!step_stays)
            st.v[P_BQ_STEP] = (uint8_t)((c + l) % len);
        rec_was.wrote = 1;
        bseq_view();
        bass_play(sound, vel);
        bq.sounding = -1;
    } else if (st.v[P_BQ_REC] == 2u) {
        bass_play(sound, vel);
        bq.sounding = -1;
        if (bq.run || (step_hand && cnt.run)) {
            if (bq.tn >= 0)
                bseq_end(1);
            bq.tn = (int8_t)(b & 127u);
            bq.ta = b & SQ_ACC;
            bq.tp = bseq_playing() ? bseq_at() : 0.0f;
            bq.te = 0.0f;
            bq.tc = (uint8_t)(!bq.run && !count_near());
        }
    } else {
        for (i = 0; i < len; i++)        /* from the first NOTE (not a CUT, not a HOLD) */
            if ((bnote[i] & 127u) != SQ_REST && (bnote[i] & 127u) != SQ_TIE)
                break;
        if (i < len)
            st.v[P_BQ_TRANS] = (uint8_t)clamp(14 + scale_steps(bnote[i] & 127, n, st.v), 0, 28);
    }
    st.dirty = (uint8_t)sound_changed();
}

/* its key or note let go, the one sounding (RECORD OFF, the sequence running: the bass is its own): its sound stops;
 * LIVE, another still down takes over, legato, else the take's note ends there (bseq_end) */
static void bseq_up(int sound)
{
    int h;
    if (bass_note != sound || (!st.v[P_BQ_REC] && bq.run))
        return;
    if (st.v[P_BQ_REC] == 2u && (h = bseq_held()) >= 0) {
        bseq_key(h, h, 100);
        bq.ta = 0;                       /* (no key struck it) */
        return;
    }
    bass_stop();
    if (bq.tn >= 0 && (bq.run || !bq.tc))
        bseq_end(0);
    bq.tn = -1;
}

static uint8_t bq_prev[SC_BSEQ + 1u], bq_had;   /* the sequence and its SEQ LENGTH before the last take or CLEAR (UNDO) */
static void bseq_keep(void)
{
    memcpy(bq_prev, bnote, SC_BSEQ);
    bq_prev[SC_BSEQ] = st.v[P_BQ_STEPS];
    bq_had = 1;
    undone[2] = 0;
}

static void bseq_set(int p, int32_t x)
{
    uint32_t c = st.v[P_BQ_STEP] % SC_BSEQ;
    if (p == P_BQ_STEP || p == P_BQ_STEPS) {
        bseq_view();
    } else if (p == P_BQ_NOTE) {
        bnote[c] = (uint8_t)((bnote[c] & SQ_ACC) | (uint32_t)x);
        bseq_view();
    } else if (p == P_BQ_ACC) {
        bnote[c] = (uint8_t)((bnote[c] & 127u) | (x ? SQ_ACC : 0u));
    } else if (p == P_BQ_NLEN) {         /* LENGTH: the note on STEP's (on a HOLD, the one it carries on, as far as STEP
                                          * at least; on a CUT, none); recording on STEP, the length carried on, and the
                                          * NOTE on STEP's at once */
        int32_t s = step_start(bnote, c);
        if (st.v[P_BQ_REC] == 1u) {
            bq.len = (uint8_t)(x < 1 ? 1 : x);
            if (s == (int32_t)c)
                step_stretch(bnote, c, bq.len, st.v[P_BQ_STEPS] + 1u);
        } else if (s >= 0) {
            step_stretch(bnote, (uint32_t)s, (uint32_t)(x > (int32_t)c - s + 1 ? x : (int32_t)c - s + 1),
                         st.v[P_BQ_STEPS] + 1u);
        }
        bseq_view();
    } else if (p == P_BQ_LEN) {          /* RATE: the step playing goes on, as far into it (less than a step) */
        float t = (float)bseq_ticks();
        if (bq.t >= t)
            bq.t -= t * fm_floorf(bq.t / t);
    } else if (p == P_BQ_CLEAR && x >= 2) {   /* turned twice: every step a CUT */
        bseq_keep();
        memset(bnote, SQ_REST, sizeof bnote);
        bq.tn = -1;
        take_adds(&btk);
        if (step_hand && bq.sounding >= 0 && bq.sounding == bass_note)
            bass_stop();                 /* (its note: no step comes to let it go now) */
        st.v[P_BQ_CLEAR] = 0;
        bseq_view();
        eng_say("BASS SEQUENCE", "CLEARED");
    } else if (p == P_BQ_NEW && x >= 2) {   /* turned twice: a new take, recording LIVE from CUTs */
        bseq_keep();
        memset(bnote, SQ_REST, sizeof bnote);
        bq.tn = -1;
        take_adds(&btk);
        st.v[P_BQ_NEW] = 0;
        param_set(P_BQ_REC, 2);
        if (!bq.run)
            bseq_begin();
        bseq_view();
        eng_say("NEW TAKE", "RECORDING");
    } else if (p == P_BQ_UNDO && x >= 2) {   /* turned twice: the sequence before the last take or CLEAR (again: back) */
        st.v[P_BQ_UNDO] = 0;
        take_adds(&btk);
        if (bq_had) {
            uint8_t t[sizeof bq_prev];
            bq.tn = -1;
            memcpy(t, bnote, SC_BSEQ);
            t[SC_BSEQ] = st.v[P_BQ_STEPS];
            memcpy(bnote, bq_prev, SC_BSEQ);
            st.v[P_BQ_STEPS] = bq_prev[SC_BSEQ];
            memcpy(bq_prev, t, sizeof t);
            bseq_view();
            eng_say("BASS SEQUENCE", undo_word(2u));
        } else {
            eng_say("UNDO TAKE", "NOTHING YET");
        }
    } else if (p == P_BQ_REC) {
        if (x && st.v[P_BQ_ON] != SQT_SEQ)   /* (RECORD: into the sequence, on) */
            param_set(P_BQ_ON, (int32_t)SQT_SEQ);
        if (x != 2)                      /* (off LIVE: the take ends) */
            bseq_close();
        if (x == 1)                      /* STEP: the length carried on, the note on STEP's to begin with (else 1) */
            bq.len = (uint8_t)(step_start(bnote, c) == (int32_t)c ? step_len(bnote, c, st.v[P_BQ_STEPS] + 1u) : 1u);
        bseq_view();
    } else if (p == P_BQ_ON) {
        if (x == (int32_t)SQT_SEQ) {
            bseq_begin();
        } else {
            bseq_stop();
            if (st.v[P_BQ_REC]) {        /* (nothing to record into) */
                st.v[P_BQ_REC] = 0;
                bseq_view();
                rec_keys(P_BQ_REC);
            }
            if (x == (int32_t)SQT_ARP)   /* (a note the keys latched: the arpeggio has the bass now) */
                bass_stop();
        }
        barp.paused = 0;
        barp.dirty = 1;                  /* (ARP again: its notes from the next step) */
        eng_type_changed();
    }
    st.dirty = (uint8_t)sound_changed();
}

/* a bass's sequence as the layouts before 2.7's kept it (FRBU's and before, FRSM's and before, FRSt's shards and
 * before), made over in place: b, its sixteen notes then their lengths (BQ_OLD's indices), onto the one grid. Its SEQ
 * LENGTH steps (steps: SEQ LENGTH - 1), on the longest RATE that divides each one's length, each its steps: a NOTE
 * then HOLDs, a CUT CUTs, a HOLD HOLDs. More than SC_BSEQ of them: the grid twice as long, till they fit, each step
 * from where it began to where it ended rounded to it, the half up (the loop as long as it was, as near as the grid
 * has it; a NOTE one step at least, a CUT or a HOLD that comes to none gone). The steps after SEQ STEPS after them,
 * as long as they fit the grid whole. A HOLD after a CUT a CUT (silent as it was), HOLDs from the first step CUTs too
 * unless the loop's end sounds a note (it carries on round). Its SEQ STEPS and STEP LENGTH (len) set */
static void bseq_over(uint8_t *b, uint8_t *steps, uint8_t *len)
{
    uint8_t note[OLD_BSEQ], out[SC_BSEQ];
    uint32_t t[OLD_BSEQ], n = *steps < OLD_BSEQ ? *steps + 1u : OLD_BSEQ, i, g, c = 0, pos, k, j, quiet;
    for (i = 0; i < OLD_BSEQ; i++) {
        note[i] = b[i] & 127u;
        t[i] = BQ_OLD[b[OLD_BSEQ + i] % NBQOLD];
    }
    for (g = NBQLEN - 1u; g; g--) {      /* (1/16 divides every one) */
        for (i = 0; i < n && t[i] % BQ_TICKS[g] == 0u; i++)
            ;
        if (i == n)
            break;
    }
    for (; g < NBQLEN; g++) {
        uint32_t w = BQ_TICKS[g];
        for (i = 0, c = 0, pos = 0; i < n && c <= SC_BSEQ; i++) {
            uint32_t e = (pos + t[i] + w / 2u) / w;
            k = e > c ? e - c : 0u;
            if (!k && note[i] != SQ_REST && note[i] != SQ_TIE)
                k = 1;
            for (j = 0; j < k && c < SC_BSEQ; j++)
                out[c++] = (uint8_t)(j && note[i] == SQ_REST ? SQ_REST : j ? SQ_TIE : note[i]);
            c += j < k ? k - j : 0u;     /* (past SC_BSEQ: counted, not written) */
            pos += t[i];
        }
        if (c <= SC_BSEQ)
            break;
    }
    if (g == NBQLEN) {                   /* (none fits: cannot be, sixteen steps of 16 BARS at most are sixteen) */
        g = NBQLEN - 1u;
        c = SC_BSEQ;
    }
    *steps = (uint8_t)((c ? c : 1u) - 1u);
    *len = (uint8_t)g;
    for (i = n; i < OLD_BSEQ && t[i] % BQ_TICKS[g] == 0u && c + t[i] / BQ_TICKS[g] <= SC_BSEQ; i++)
        for (j = 0, k = t[i] / BQ_TICKS[g]; j < k; j++)
            out[c++] = (uint8_t)(j && note[i] == SQ_REST ? SQ_REST : j ? SQ_TIE : note[i]);
    memset(out + c, SQ_REST, SC_BSEQ - c);
    for (i = 0, quiet = 1; i <= *steps; i++)   /* (what the loop's end leaves sounding, round to its first steps) */
        if (out[i] != SQ_TIE)
            quiet = out[i] == SQ_REST;
    for (i = 0; i < SC_BSEQ; i++) {
        if (out[i] != SQ_TIE)
            quiet = out[i] == SQ_REST;
        else if (quiet)
            out[i] = SQ_REST;
    }
    memcpy(b, out, SC_BSEQ);
}

/* --------------------------------------------------- the drone's sequence --- */
static uint32_t dseq_ticks(void) { return BQ_TICKS[st.v[P_DQ_RATE] % NBQLEN]; }   /* a step, in MIDI clocks */
static uint32_t dseq_len(void) { return st.v[P_DQ_LEN] + 1u; }
static int dstep_set(const uint8_t *s) { return s[0] != 0u && s[0] != DQ_TIE; }   /* a step with notes */

static int dseq_has_notes(void)          /* a step with notes in its SEQ LENGTH */
{
    uint32_t i, n = dseq_len();
    for (i = 0; i < n && i < SC_DSEQ; i++)
        if (dstep_set(dnote[i]))
            return 1;
    return 0;
}

/* a step as it should be: a tie alone, or notes 1 .. 126 low to high, none twice, 0 after them (a shard from outside) */
static void dstep_sane(uint8_t *s)
{
    uint8_t t[DQ_NOTES];
    uint32_t i, i2, j, k = 0;
    if (s[0] == DQ_TIE) {
        memset(s + 1, 0, DQ_NOTES - 1u);
        return;
    }
    for (i = 0; i < DQ_NOTES; i++) {
        uint8_t n = s[i];
        if (!n || n >= DQ_TIE)
            continue;
        for (j = k; j && t[j - 1u] > n; j--)
            ;
        if (j && t[j - 1u] == n)
            continue;
        for (i2 = k; i2 > j; i2--)       /* (the FM-1's libc has no memmove) */
            t[i2] = t[i2 - 1u];
        t[j] = n;
        k++;
    }
    memset(s, 0, DQ_NOTES);
    memcpy(s, t, k);
}

/* note n (1 .. 126) into step s's notes, in its place (none twice; full: not); a tie there becomes these notes. 1: in */
static int dstep_add(uint8_t *s, int n)
{
    uint32_t i, j;
    if (n < 1 || n >= (int)DQ_TIE)
        return 0;
    if (s[0] == DQ_TIE)
        memset(s, 0, DQ_NOTES);
    for (i = 0; i < DQ_NOTES && s[i]; i++)
        if (s[i] == (uint8_t)n)
            return 0;
    if (i == DQ_NOTES)
        return 0;
    for (j = i; j && s[j - 1u] > (uint8_t)n; j--)
        s[j] = s[j - 1u];
    s[j] = (uint8_t)n;
    return 1;
}

static void dseq_off(void)               /* the notes it holds let go (a chord holding one too: that one sounds on) */
{
    uint32_t i;
    for (i = 0; i < dq.nsnd; i++)
        if (held_by[dq.snd[i]] && --held_by[dq.snd[i]] == 0)
            frasil_note_off(dq.snd[i]);
    dq.nsnd = 0;
}

/* a step's notes struck, TRANSPOSE's steps along the scale on each, at 100 as a scene's chords */
static void dseq_strike(const uint8_t *s)
{
    uint32_t i, j, k = 0;
    dseq_off();
    for (i = 0; i < DQ_NOTES && s[i] && s[i] != DQ_TIE; i++) {
        int n = scale_shift(s[i], (int)st.v[P_DQ_TRANS] - 14, st.v);
        for (j = 0; j < k && dq.snd[j] != (uint8_t)n; j++)
            ;
        if (j < k)                       /* (two clamped onto one: once) */
            continue;
        dq.snd[k++] = (uint8_t)n;
        if (held_by[n]++ == 0)
            frasil_note_on(n, 100.0f * (1.0f / 127.0f));
    }
    dq.nsnd = (uint8_t)k;
    dq.steps++;
}

static float dseq_at(void) { return dq.t + (float)dseq_ticks() * (float)(dq.pos > 0 ? dq.pos : 0); }   /* (the playhead,
                                                                                                     * clocks into the loop) */
static int dseq_playing(void) { return dq.run && !dq.wait && dq.pos >= 0; }

static void dseq_begin(void)             /* from its first step, at the grid's next */
{
    dseq_off();
    dq.run = dq.wait = 1;
    dq.pos = -1;
    take_new(&dtk);
}

static void dseq_stop(void)              /* its notes let go */
{
    dseq_off();
    dq.run = dq.wait = 0;
    dq.pos = -1;
}

/* step i reached, a LIVE take's first turn with OVERDUB off: its notes gone (a rest), unless the take wrote them */
static int dtake_reach(uint32_t i)
{
    if (overdub)
        dtk.left = 0;
    if (!dtk.left)
        return 0;
    dtk.left--;
    if (dtk.wrote >> (i & 31u) & 1u || !dnote[i][0])
        return 0;
    memset(dnote[i], 0, DQ_NOTES);
    return 1;
}

/* step i begins: its notes struck (the last let go), unless CHANCE leaves it out (silent); a tie holds the last on, a
 * rest lets it go */
static void dseq_step(int32_t i)
{
    const uint8_t *s = dnote[i];
    dq.turn += i == 0;
    take_run(&dtk, st.v[P_DQ_REC] == 2u && st.v[P_DQ_ON], dseq_len());
    if (dtake_reach((uint32_t)i)) {
        dseq_view();
        st.dirty = (uint8_t)sound_changed();
    }
    dq.pos = (int8_t)i;
    if (s[0] == DQ_TIE)
        return;
    dseq_off();
    if (!s[0])
        return;
    dq.rng ^= dq.rng << 13;
    dq.rng ^= dq.rng >> 17;
    dq.rng ^= dq.rng << 5;
    if (dq.rng % 127u >= st.v[P_DQ_CHANCE])
        return;
    dseq_strike(s);
}

/* on the grid, after the bass's: the steps that began since the last run, at the loop's end round; GATE */
static void dseq_run(void)
{
    float g = grid.tick, dg = g - dq.g, t = (float)dseq_ticks();
    uint32_t len = dseq_len();
    dq.g = g;
    if (dg < -4800.0f)                   /* (the grid wraps at 9600) */
        dg += 9600.0f;
    else if (dg < 0.0f)
        dg = 0.0f;
    if (!dq.run)
        return;
    if (dq.wait == 1) {                  /* begun: on the grid's next step, GLINT's while its sequence runs, else the
                                          * next sixteenth (as the bass's) */
        float h = g, b = 6.0f;
        if (st.v[P_SQ_TYPE] == SQT_SEQ && sq.run) {
            b = (float)ARP_TICKS[st.v[P_ARP_RATE] % 10u];
            h = seq_grid(g);
        }
        if (fm_floorf(h / b) == fm_floorf((h - dg) / b))
            return;
        dq.t = h - b * fm_floorf(h / b);
    } else if (dq.wait == 2) {           /* MIDI Start, PLAY from the top: the grid's top */
        dq.t = g;
    } else {
        dq.t += dg;
    }
    if (dq.wait) {
        dq.wait = 0;
        dseq_step(0);
    }
    while (dq.t >= t) {
        dq.t -= t;
        dseq_step(dq.pos < (int32_t)len - 1 ? dq.pos + 1 : 0);
    }
    if (st.v[P_DQ_GATE] < 127u && dq.nsnd && dq.pos >= 0 &&   /* GATE: let go in its last step (a tie next: on) */
        dnote[dq.pos < (int32_t)len - 1 ? dq.pos + 1 : 0][0] != DQ_TIE &&
        dq.t >= t * (0.05f + 0.95f * (float)st.v[P_DQ_GATE] * (1.0f / 127.0f)))
        dseq_off();
}

#define DQ_BAR 96.0f                     /* a bar, in clocks */
#define DQ_EARLY 6.0f                    /* a hand this far ahead of a long step belongs to it (hands land before a bar line
                                          * as often as after, and a chord put back into the last step would take its place) */

/* a hand now goes into the drone's sequence (dseq_key): STEP, or LIVE with the sequence running, or about to */
static int dseq_takes(void)
{
    return st.v[P_DQ_REC] && st.v[P_DQ_ON] && (st.v[P_DQ_REC] == 1u || dq.run || count_near());
}

/* a new hand takes the step's place (STACK off, no key or finger down that wrote, not a MIDI chord's next note, not SEL's
 * single note over what is there): the FM-1's */
static int dseq_new_hand(int adds)
{
    uint32_t i, n = 0;
    for (i = 0; i < 4u; i++)
        n += mid_on[i] ? (mid_on[i] & (mid_on[i] - 1u) ? 2u : 1u) : 0u;
    return step_hand && !adds && !dq.keys && !st.v[P_KY_DR_STACK] && n < 2u;
}

/* a LIVE take's hand on step q, turn `turn` of the loop, on the FM-1. A new hand takes the place of this take's on that
 * step, what an earlier take left there staying; the loop come round to a step this take has written, it takes the whole
 * step's. STACK on, or a hand's other keys: beside what is there */
static struct {
    int8_t q;                            /* the step the hand went into last, -1: none */
    uint32_t turn;                       /* on that turn */
    uint8_t was[DQ_NOTES];               /* that step as it was before this turn's first hand */
    uint32_t wrote;                      /* the steps this take has written */
} dh = {.q = -1};

static void dtake_new(void) { dh.q = -1, dh.wrote = 0; }

static void dtake_hand(uint32_t q, uint32_t turn, int fresh)
{
    int same = dh.q == (int8_t)q && dh.turn == turn;
    if (fresh && same)
        memcpy(dnote[q], dh.was, DQ_NOTES);
    else if (fresh && dh.wrote >> (q & 31u) & 1u)
        memset(dnote[q], 0, DQ_NOTES);
    if (!same) {
        memcpy(dh.was, dnote[q], DQ_NOTES);
        dh.q = (int8_t)q;
        dh.turn = turn;
    }
    dh.wrote |= 1u << (q & 31u);
}

/* the notes a drone key, a chord latched or held, a finger or a MIDI note sounds (n[0 .. k - 1], as they sound), RECORD
 * on: into the sequence, TRANSPOSE off them (written so that, TRANSPOSE on them, they play as played). LIVE: the step
 * nearest the playhead (the sequence playing; begun, not yet at its first step: that one; stopped: nothing, it
 * sounds); STEP: STEP's step. 1: written there (STEP's: its up moves STEP on, dseq_up) */
static int dseq_key(const int8_t *n, uint32_t k)
{
    uint32_t i, q, len = dseq_len();
    int fresh = dseq_new_hand(dq.adds);
    dq.adds = 0;
    if (!dseq_takes() || !k)
        return 0;
    if (st.v[P_DQ_REC] == 1u) {
        q = st.v[P_DQ_STEP] % len;
        if (fresh)                       /* (STACK off, a new hand in place of what the step held; on, beside it) */
            memset(dnote[q], 0, DQ_NOTES);
    } else {
        float t = (float)dseq_ticks();
        uint32_t a = 0, over = 0;
        if (dseq_playing()) {            /* the nearest step; on the FM-1, a step of a bar or more, the one under the
                                          * playhead (a chord past the middle of a long step is played in it), a hand a
                                          * sixteenth ahead of the next already that one's */
            a = (uint32_t)fm_floorf((dseq_at() + (step_hand && t >= DQ_BAR ? DQ_EARLY : 0.5f * t)) / t);
            over = a >= len;
        } else {
            over = 1;                    /* (not begun: the first step of the turn to come) */
        }
        q = a % len;
        take_run(&dtk, 1, len);
        dtk.wrote |= 1u << (q & 31u);    /* (the take's own: its first turn replaces it no more) */
        if (step_hand)
            dtake_hand(q, dq.turn + over, fresh);
    }
    for (i = 0; i < k; i++)
        dstep_add(dnote[q], scale_shift(n[i], 14 - (int)st.v[P_DQ_TRANS], st.v));
    rec_was.wrote = 1;
    dseq_view();
    st.dirty = (uint8_t)sound_changed();
    return st.v[P_DQ_REC] == 1u || step_hand;
}

/* its steps as GLINT's are (step_start, step_len, step_stretch): a chord's LENGTH its step and the ties after it. The
 * chord step c is, or carries on through its ties: its step (-1: none, a rest there, or ties after one or from the
 * first step) */
static int32_t dstep_start(uint32_t c)
{
    int32_t i;
    for (i = (int32_t)(c % SC_DSEQ); i >= 0 && dnote[i][0] == DQ_TIE; i--)
        ;
    return i >= 0 && dnote[i][0] ? i : -1;
}

static uint32_t dstep_len(uint32_t s, uint32_t steps)
{
    uint32_t e, n = step_lim(s, steps);
    for (e = s + 1u; e < n && dnote[e][0] == DQ_TIE; e++)
        ;
    return e - s;
}

/* the chord at step s, l steps long (one at least, step_lim's at most): ties after it, rests where it reaches no more;
 * what it covered does not go on after it (its ties rests, till the next chord or rest) */
static void dstep_stretch(uint32_t s, uint32_t l, uint32_t steps)
{
    uint32_t n = step_lim(s, steps), was = dstep_len(s, steps), i;
    l = l < 1u ? 1u : l > n - s ? n - s : l;
    for (i = s + 1u; i < s + l; i++) {
        memset(dnote[i], 0, DQ_NOTES);
        dnote[i][0] = DQ_TIE;
    }
    for (i = s + l; i < s + was; i++)
        memset(dnote[i], 0, DQ_NOTES);
    for (i = s + l; i < SC_DSEQ && dnote[i][0] == DQ_TIE; i++)
        memset(dnote[i], 0, DQ_NOTES);
}

/* DRONE SEQ II's NOTES and LENGTH show the step on STEP: its content (REST 0, its notes 1, TIE 2: the screen's words,
 * dq_view_n, dq_view_note) and the LENGTH of the chord it is or carries on (-: none); recording on STEP, the length
 * carried on instead (dq.len), what the next chord will take */
static void dseq_view(void)
{
    uint32_t c = st.v[P_DQ_STEP] % SC_DSEQ, i;
    const uint8_t *s = dnote[c];
    int32_t a = dstep_start(c);
    for (i = 0; i < DQ_NOTES && s[i] && s[i] != DQ_TIE; i++)
        ;
    dq_view_n = (uint8_t)i;
    dq_view_note = s[0];
    st.v[P_DQ_NOTES] = (uint8_t)(s[0] == DQ_TIE ? 2u : s[0] ? 1u : 0u);
    st.v[P_DQ_NLEN] = (uint8_t)(st.v[P_DQ_REC] == 1u ? dq.len : a >= 0 ? dstep_len((uint32_t)a, dseq_len()) : 0u);
}

static void dseq_up(void)                /* a key or a finger that wrote on STEP let go: the last, the chord LENGTH's
                                          * steps long (dq.len: its ties), STEP on past it */
{
    if (dq.keys && !--dq.keys && st.v[P_DQ_REC] == 1u && !step_stays) {
        uint32_t len = dseq_len(), c = st.v[P_DQ_STEP] % len, l = dq.len ? dq.len : 1u;
        l = l < len - c ? l : len - c;
        if (l > 1u && dstep_set(dnote[c]))
            dstep_stretch(c, l, len);
        st.v[P_DQ_STEP] = (uint8_t)((c + l) % len);
        dseq_view();
    }
}

/* UNDO TAKE: the sequence and its SEQ LENGTH before the last take (RECORD on, from OFF) or CLEAR, as the bass's */
static uint8_t dq_prev[SC_DSEQ][DQ_NOTES], dq_prev_len, dq_had;
static void dseq_keep(void)
{
    memcpy(dq_prev, dnote, sizeof dq_prev);
    dq_prev_len = st.v[P_DQ_LEN];
    dq_had = 1;
    undone[0] = 0;
}

/* NOTES turned off a step's notes (to REST or TIE): those notes kept, the step's, to come back to (NOTES on them again) */
static uint8_t dq_memo[DQ_NOTES];
static int8_t dq_memo_at = -1;

/* the notes the drones' keys hold now (an FM-1 key down, latched or held; a finger), as they sound, into t (DQ_NOTES at
 * most, as dstep_add puts them), TRANSPOSE off them (as the step keeps them) */
static void dseq_held(uint8_t *t)
{
    uint32_t k, i, n;
    for (k = 0; k < FRS_NKEYS; k++) {
        const chord_t *c = key[k].kind == K_HOLD ? &key[k].chord : key[k].kind == K_LATCH && bit_get(lit, key[k].root)
                                                                    ? &lat[(uint8_t)key[k].root] : 0;
        for (i = 0; c && i < c->k; i++)
            dstep_add(t, scale_shift(c->n[i], 14 - (int)st.v[P_DQ_TRANS], st.v));
    }
    for (n = 0; n < 128u; n++)
        for (i = 0; i < hold_c[n].k; i++)
            dstep_add(t, scale_shift(hold_c[n].n[i], 14 - (int)st.v[P_DQ_TRANS], st.v));
}

/* NOTES turned (the step on STEP): REST, TIE, or its notes: those it had (turned off them just now), with the drones'
 * keys held now beside them; none, the chord HARMONY makes on ROOT (in the third octave, as ▶'s) */
static void dseq_notes(uint32_t x)
{
    uint32_t c = st.v[P_DQ_STEP] % SC_DSEQ;
    uint8_t *s = dnote[c], t[DQ_NOTES] = {0};
    if (dstep_set(s)) {
        memcpy(dq_memo, s, DQ_NOTES);
        dq_memo_at = (int8_t)c;
    }
    if (x == 1u) {
        uint32_t i;
        if (dq_memo_at == (int8_t)c)
            memcpy(t, dq_memo, DQ_NOTES);
        dseq_held(t);
        if (!t[0]) {
            chord_t ch;
            int root;
            chord_build(48 + (int)(st.v[P_ROOT] % 12u), st.v, &ch, &root);
            for (i = 0; i < ch.k; i++)
                dstep_add(t, scale_shift(ch.n[i], 14 - (int)st.v[P_DQ_TRANS], st.v));
        }
    } else if (x >= 2u) {
        t[0] = DQ_TIE;
    }
    memcpy(s, t, DQ_NOTES);
}

/* RECORD off: the chords the take latched let go, what plays now the sequence */
static void dtake_let_go(void)
{
    uint32_t r;
    for (r = 0; r < 128u; r++)
        if (bit_get(dt_lat, (int)r) && bit_get(lit, (int)r)) {
            chord_off(&lat[r]);
            bit_put(lit, (int)r, 0);
        }
    memset(dt_lat, 0, sizeof dt_lat);
}

static void dseq_set(int p, int32_t x)
{
    uint32_t k, c = st.v[P_DQ_STEP] % SC_DSEQ;
    if (p == P_DQ_REC && x && !set_was)  /* (a take begins: UNDO TAKE has what was there) */
        dseq_keep();
    if (p == P_DQ_ON) {
        if (x) {
            dseq_begin();
        } else {
            dseq_stop();
            if (st.v[P_DQ_REC]) {        /* (nothing to record into) */
                st.v[P_DQ_REC] = 0;
                dtake_let_go();
                rec_keys(P_DQ_REC);
            }
        }
    } else if (p == P_DQ_REC) {          /* (RECORD: into the sequence, on) */
        if (x && !st.v[P_DQ_ON])
            param_set(P_DQ_ON, 1);
        if (!x)
            dtake_let_go();
        dtake_new();
        dq.keys = 0;
        memset(drec_f, 0, sizeof drec_f);
        for (k = 0; k < FRS_NKEYS; k++)
            key[k].drec = 0;
        if (x == 1)                      /* STEP: the length carried on, the chord on STEP's to begin with (else 1) */
            dq.len = (uint8_t)(dstep_start(c) == (int32_t)c ? dstep_len(c, dseq_len()) : 1u);
    } else if (p == P_DQ_RATE) {         /* the step playing goes on, as far into it (less than a step) */
        float t = (float)dseq_ticks();
        if (dq.t >= t)
            dq.t -= t * fm_floorf(dq.t / t);
    } else if (p == P_DQ_CLEAR && x >= 2) {   /* turned twice: every step a rest */
        dseq_keep();
        memset(dnote, 0, sizeof dnote);
        dseq_off();
        take_adds(&dtk);
        dtake_new();
        st.v[P_DQ_CLEAR] = 0;
        eng_say("DRONE SEQUENCE", "CLEARED");
    } else if (p == P_DQ_NOTES) {        /* NOTES: REST, its notes, TIE (dseq_notes) */
        dseq_notes((uint32_t)x);
    } else if (p == P_DQ_NLEN) {         /* LENGTH: the chord on STEP's (on a tie, the one it carries on, as far as STEP
                                          * at least; on a rest, none); recording on STEP, the length carried on, the
                                          * chord on STEP's at once: as GLINT's */
        int32_t s = dstep_start(c);
        if (st.v[P_DQ_REC] == 1u) {
            dq.len = (uint8_t)(x < 1 ? 1 : x);
            if (s == (int32_t)c)
                dstep_stretch(c, dq.len, dseq_len());
        } else if (s >= 0) {
            dstep_stretch((uint32_t)s, (uint32_t)(x > (int32_t)c - s + 1 ? x : (int32_t)c - s + 1), dseq_len());
        }
    } else if (p == P_DQ_UNDO && x >= 2) {   /* turned twice: the sequence before the last take or CLEAR (again: back) */
        st.v[P_DQ_UNDO] = 0;
        take_adds(&dtk);
        dtake_new();
        if (dq_had) {
            static uint8_t t[SC_DSEQ][DQ_NOTES];
            uint8_t l = st.v[P_DQ_LEN];
            memcpy(t, dnote, sizeof t);
            memcpy(dnote, dq_prev, sizeof dnote);
            memcpy(dq_prev, t, sizeof t);
            st.v[P_DQ_LEN] = dq_prev_len;
            dq_prev_len = l;
            eng_say("DRONE SEQUENCE", undo_word(0u));
        } else {
            eng_say("UNDO TAKE", "NOTHING YET");
        }
    }
    dseq_view();
    st.dirty = (uint8_t)sound_changed();
}

/* a step's notes set as given (as the step keeps them, TRANSPOSE off: 1 .. 126, DQ_NOTES at most; none, a rest; DQ_TIE
 * first, a tie), the iPad's or the River's editor. 1: changed */
static int eng_dseq_step(uint32_t i, const uint8_t *n, uint32_t k)
{
    uint8_t s[DQ_NOTES] = {0};
    if (i >= SC_DSEQ)
        return 0;
    if (n && k)
        memcpy(s, n, k < DQ_NOTES ? k : DQ_NOTES);
    dstep_sane(s);
    if (!memcmp(s, dnote[i], DQ_NOTES))
        return 0;
    memcpy(dnote[i], s, DQ_NOTES);
    dseq_view();
    st.dirty = (uint8_t)sound_changed();
    return 1;
}

/* a note of step i's on (into it, as dstep_add) or off (out of it; a tie: as it was). 1: changed */
static int eng_dseq_note(uint32_t i, uint32_t note, int on)
{
    uint8_t s[DQ_NOTES];
    uint32_t j, k = 0;
    if (i >= SC_DSEQ || note < 1u || note >= DQ_TIE)
        return 0;
    memcpy(s, dnote[i], DQ_NOTES);
    if (on) {
        dstep_add(s, (int)note);
    } else if (s[0] != DQ_TIE) {
        for (j = 0; j < DQ_NOTES; j++)
            if (s[j] != (uint8_t)note)
                s[k++] = s[j];
        memset(s + k, 0, DQ_NOTES - k);
    }
    return eng_dseq_step(i, s, DQ_NOTES);
}

/* a scene with no drone's sequence: off, empty, its settings their defaults */
static void dseq_none(scene_t *sc)
{
    sc->dq_on = 0;
    sc->dq_rate = PD[P_DQ_RATE].def;
    sc->dq_len = PD[P_DQ_LEN].def;
    sc->dq_gate = PD[P_DQ_GATE].def;
    sc->dq_chance = PD[P_DQ_CHANCE].def;
    sc->dq_trans = PD[P_DQ_TRANS].def;
    memset(sc->dnote, 0, sizeof sc->dnote);
}

/* a scene's drone sequence and its settings into what plays (as they were kept: nothing begun) */
static void dseq_take(const scene_t *sc)
{
    static const uint8_t P[NDQ] = {P_DQ_ON, P_DQ_RATE, P_DQ_LEN, P_DQ_GATE, P_DQ_CHANCE, P_DQ_TRANS};
    uint32_t i;
    memcpy(dnote, sc->dnote, sizeof dnote);
    for (i = 0; i < SC_DSEQ; i++)
        dstep_sane(dnote[i]);
    for (i = 0; i < NDQ; i++) {
        uint8_t x = (&sc->dq_on)[i];
        st.v[P[i]] = x > PD[P[i]].max ? PD[P[i]].def : x;
    }
    dq_memo_at = -1;
    dseq_view();
}

/* a scene's MORPH under way: what each setting still lacks of the scene's at the start (morph_off goes from it to 0) */
static struct {
    uint8_t on;
    uint8_t anc;                         /* ANCHOR's way back, not a scene's */
    uint32_t t0, dur;
    float d[NMORPH];
    char name[SC_NAME_ALL + 1];          /* the scene it goes to (its 32) */
} mo;

/* ANCHOR (the FM-1's OCT- and OCT+ together; frs_anchor_*): the scene as it is now, kept in RAM alone (never the store,
 * no flash written), to come back to gliding (MORPH: the FM-1's and the iPad's press, since 3.1) or at once (BACK:
 * their hold). What comes back: what a scene keeps of the sound and of its tracks (every setting of the sound, the
 * effects, HARMONY, each layer's PLAYS, ARP's and SEQ's settings, TEMPO, BASS TONE, LOFI TAPE II's four, the LFOs'
 * AMOUNTs, SWELL, the three sequences' steps); not what plays: the notes latched or held sound on, the sequences play
 * on from where they are. A take under way keeps its
 * track's steps, PLAYS and SEQ LENGTH (the hand's). LOFI TAPE II's four and the AMOUNTs (past NMORPH) switch at the half,
 * the AMOUNTs with their TARGETs; SWELL glides (morph_off, eng_swell hearing it).
 * Not a scene's, not kept: KEYS, FREEZE, what the keys play, SWELL's ENV held and CC 11 (of the moment), the runner's
 * own (the FM-1's octave: its own, beside).
 * MORPH: the settings that glide (anc_glides) go from where they are heard to the anchor's on the scene MORPH's path
 * (morph_off, a hundred times a second, the DSP's 25 ms glide under it), smoothstep; the rest (a choice among a few: a
 * preset, MODEL, TYPE, ROOT, SCALE, CHORD, a TARGET, a RATE, PLAYS, TEMPO, SEQ LENGTH, the steps) switch together at
 * its half, where the sound moves most. A hand on a setting takes it out of either; a scene taken drops the anchor and
 * the way back under way, its switch with it */
#define ANCHOR_MS 8000u                  /* MORPH's (the FM-1's press, its long press until 3.1; 3.5 s until 3.0.2-b2, too
                                          * quick to follow; 14 s in b3, too long) */
#define ANCHOR_NAME "ANCHOR"             /* its one name, the screen's too (the FM-1's app.c) */
static struct {
    uint8_t on;                          /* set */
    uint8_t due;                         /* a way back's switch still to come, at mo.t0 + half */
    uint8_t seqs;                        /* the sequences it takes then: 1 GLINT's, 2 the bass's, 4 the drone's */
    uint32_t half;
    uint32_t pend[(NPARAM + 31u) / 32u]; /* the settings it switches then */
    scene_t sc;                          /* the anchor (its notes in it, as STORE has them: never played back) */
} anc;
static void anchor_drop(void);           /* (below) */
static void anc_switch(void);

/* KEYS turned (the layer's HARMONY, NOTES, LATCH, STACK). LATCH off lets the layer's latched notes go, as KEY MODE did
 * (the drones' chords latched; GLINT's ring, on KEYS; the bass's note latched, on KEYS), what is held sounding on; the
 * drones' is KEY MODE's (P_KEYS: LATCH on, HOLD off) */
static void arp_keep_held(arp_t *a);     /* (the arpeggiator, below) */
static void anc_hand(int p);             /* (ANCHOR, below) */
static void keys_set(int p, int32_t x)
{
    uint32_t k, r;
    if (p == P_KY_DR_LATCH)
        st.v[P_KEYS] = (uint8_t)!x;
    if (x || (p != P_KY_DR_LATCH && p != P_KY_GL_LATCH && p != P_KY_BS_LATCH))
        return;
    latch_gone |= (uint8_t)(1u << ((uint32_t)(p - P_KY_DR_HARM) / 4u));
    if (p == P_KY_DR_LATCH) {
        for (r = 0; r < 128u; r++)
            if (bit_get(lit, (int)r)) {
                chord_off(&lat[r]);
                bit_put(lit, (int)r, 0);
            }
        for (k = 0; k < FRS_NKEYS; k++)
            if (key[k].kind == K_LATCH)
                key[k].kind = K_NONE;
    } else if (p == P_KY_GL_LATCH) {     /* (GLINT's keys never latch: what THE KEYS' LATCH rings, on KEYS) */
        if (!st.v[P_SQ_TYPE])
            arp_keep_held(&arp);
    } else {
        int held = (mb_n && mbass[mb_n - 1] == bass_note) || (hold_bn >= 0 && hold_bn == bass_note);
        for (k = 0; k < FRS_NKEYS; k++) {
            if (key[k].kind == K_BASS && !key[k].hold)
                key[k].kind = K_NONE;
            held |= key[k].kind == K_BASS && key[k].root == bass_note;
        }
        if (st.v[P_BQ_ON] != SQT_ARP && !bseq_owns() && bass_note >= 0 && !held)
            bass_stop();
    }
}

static void param_set(int p, int32_t x)
{
    x = clamp(x, 0, PD[p].max);
    if (st.v[p] == (uint8_t)x)
        return;
    set_was = st.v[p];
    st.v[p] = (uint8_t)x;
    if (anc.due)                         /* (ANCHOR's way back: this one the hand's) */
        anc_hand(p);
    if (p == P_GL_PRESET)                /* a preset: its sound into the GLINT parameters */
        glint_preset(st.v, (uint32_t)x);
#if FRS_LEAD
    if (p >= P_LD_WAVE) {                /* LEAD's (card #144): a preset its sound; MODE turned (or a preset's), every
                                          * voice let go (the keys held as they are: the next played in the new MODE) */
        uint8_t m = st.v[P_LD_MODE];
        if (p == P_LD_PRESET)
            lead_preset(st.v, (uint32_t)x);
        if (p == P_LD_MODE || st.v[P_LD_MODE] != m)
            frasil_lead_off();
        st.engine_dirty = 1;
        st.dirty = (uint8_t)sound_changed();   /* (the scene's: its LEAD, app.c ldp_*) */
    }
#endif
    if (p == P_KEYS) {                   /* to held notes, or back: the latched ones go; KEYS' LATCH, the drones' and the
                                          * bass's, as it says (KEY MODE's in their place: read in so) */
        st.v[P_KY_DR_LATCH] = st.v[P_KY_BS_LATCH] = (uint8_t)!x;
        release_all();
        keymode_gone = 1;
    }
    if (p >= P_KY_DR_HARM && p <= P_KY_BS_STACK)
        keys_set(p, x);
    if (p == P_GA_LATCH || p == P_BA_LATCH) {   /* ARP II's LATCH: off, only the notes still held go on (GLINT on KEYS:
                                                 * its list is the ring, KEYS' LATCH's) */
        arp_t *a = p == P_GA_LATCH ? &arp : &barp;
        a->latch = (uint8_t)x;
        if (!x && (a != &arp || st.v[P_SQ_TYPE]))
            arp_keep_held(a);
    }
    if (p == P_METRO) {                  /* REC: a take's ticks; ALWAYS: the grid's beats too (frs_metronome's) */
        st.metro = x >= 1;
        metro_free = x == 2;
    }
    if (p == P_KB) {                     /* what the keys play (what sounds stays) */
        st.glint = x == 1;
        st.bass = x == 2;
#if FRS_LEAD
        st.lead = x == 3;
#endif
    }
    if (p == P_FREEZE)
        st.engine_dirty = 1;
    if (p == P_SPEAKER || p == P_ANIM || p == P_CLOCK || p == P_SAVER)   /* (the runner's: SPK/JACK's output, kept with
                                                                          * its settings) */
        eng_setting(p, x);
    if (p == P_CLOCK && x)               /* INTERNAL: TEMPO's grid at once, a clock coming in let go */
        grid.clk_at = eng_ms() - 60000u;
    if (p == P_SQ_TYPE || (p >= P_SQ_STEP && p <= P_SQ_CLEAR) || p == P_SQ_NEW || p == P_SQ_UNDO || p == P_SQ_NLEN ||
        p == P_SQ_LEN)
        seq_set(p, x);
    if ((p >= P_BQ_STEP && p <= P_BQ_TRANS) || (p >= P_BQ_NLEN && p <= P_BQ_UNDO))
        bseq_set(p, x);
    if ((p >= P_DQ_ON && p <= P_DQ_STEP) || (p >= P_DQ_NOTES && p <= P_DQ_UNDO))
        dseq_set(p, x);
    if (p < NMORPH)                      /* a hand on it: out of the scene's MORPH (a preset: all it sets) */
        morph_off[p] = mo.d[p] = 0.0f;
    if (p == P_DR_PRESET) {
        uint32_t i;
        for (i = 0; i < NDRONE; i++)
            morph_off[DRONE_P[i]] = mo.d[DRONE_P[i]] = 0.0f;
    } else if (p == P_GL_PRESET) {
        uint32_t i;
        for (i = P_GL_RATIO; i < P_GL_RATIO + 7u; i++)
            morph_off[i] = mo.d[i] = 0.0f;
        morph_off[P_GL_ATTACK] = mo.d[P_GL_ATTACK] = morph_off[P_GL_PING] = mo.d[P_GL_PING] = 0.0f;
        morph_off[P_GL_SUSTAIN] = mo.d[P_GL_SUSTAIN] = 0.0f;
    }
    if (p == P_SQ_REC || p == P_BQ_REC || p == P_DQ_REC)
        rec_keys(p);
    if (p == P_DR_PRESET) {              /* a preset, or CUSTOM back */
        drone_apply((uint32_t)x);
    } else if (drone_of[p] >= 0) {       /* a drone's setting turned: CUSTOM from here, the rest as they were */
        if (st.v[P_DR_PRESET]) {
            uint32_t i;
            st.v[P_DR_PRESET] = 0;
            for (i = 0; i < NDRONE; i++)
                custom[i] = st.v[DRONE_P[i]];
        } else {
            custom[drone_of[p]] = (uint8_t)x;
        }
    }
    if (p == P_EQ_PRESET && x >= 2) {    /* MASTER EQ's preset: its bands (OFF and CUSTOM leave them as they are) */
        uint32_t i;
        for (i = 0; i < 7u; i++) {
            st.v[P_EQ_LOW + i] = EQ_PRESETS[x - 2][i];
            morph_off[P_EQ_LOW + i] = mo.d[P_EQ_LOW + i] = 0.0f;
        }
    } else if (p >= P_EQ_LOW && p <= P_EQ_HIGHF && st.v[P_EQ_PRESET] >= 2u) {
        st.v[P_EQ_PRESET] = 1;           /* a band turned: CUSTOM, the bands as they are */
    }
    if (p < NSOUND) {
        st.dirty = (uint8_t)sound_changed();
        st.engine_dirty = 1;
    } else if (p == P_TEMPO) {           /* (the scene's TEMPO too) */
        st.dirty = (uint8_t)sound_changed();
    } else if (p == P_BS_WEIGHT || p == P_BS_PUNCH || p == P_BS_BITE || p == P_BS_SPACE || p == P_GL_VIB_AUTO ||
               (p >= P_TP_FLUTTER && p <= P_TP_DROPOUT)) {   /* (the scene's, the sound's) */
        st.dirty = (uint8_t)sound_changed();
        st.engine_dirty = 1;
    } else if ((p >= P_L1_AMT1 && p <= P_L3_AMT4) || p == P_SWELL) {   /* (the scene's too: the LFOs and eng_swell hear
                                                                         * them at their next pass) */
        st.dirty = (uint8_t)sound_changed();
    }
}

/* RECORD p turned (GLINT's or the bass's; by REC, NEW TAKE, a page, or gone with its sequencer), STEP and LIVE alike:
 * a recording begun has the keys at once, on its track (KEYS as they were kept, st.kb_rec); the other still
 * recording, they go to it; neither, back to what they played */
static void rec_keys(int p)
{
    int g = st.v[P_SQ_REC] != 0, b = st.v[P_BQ_REC] != 0, d = st.v[P_DQ_REC] != 0;
    if (!g && !b && !d) {
        if (st.rec_took)
            param_set(P_KB, st.kb_rec);
        st.rec_took = 0;
        return;
    }
    if (!st.rec_took) {
        st.kb_rec = st.v[P_KB];
        st.rec_took = 1;
    }
    param_set(P_KB, st.v[p] ? (p == P_BQ_REC ? 2 : p == P_DQ_REC ? 0 : 1) : g ? 1 : b ? 2 : 0);
}

/* ------------------------------------------------ a setting's kind, the state --- */
/* what a setting is (frasil_engine.h's FRS_P_*, the same numbers: frs_param_kind), and the engine's state beside a scene
 * (frs_state_save): the brain's, so that the FM-1's PERFORM STATE (app.c) says them as the engine alone does */
enum { PK_SCENE = 1, PK_STATE, PK_MOMENT, PK_STEP, PK_ACTION, PK_NONE };
static uint32_t param_kind(uint32_t p)
{
    if (p >= NPARAM || p == P_SC_GONE)
        return PK_NONE;
#if FRS_LEAD
    if (p >= P_LD_WAVE)                  /* LEAD's (card #144): a scene's (card #152: not in scene_t, a shard's, after
                                          * it: eng_lead_*; the FM-1's in its own object, a slot each; LIVE tells them as
                                          * they turn) */
        return PK_SCENE;
#endif
    if (p < NSOUND || p == P_TEMPO || p == P_BQ_ON || p == P_BQ_STEPS || p == P_BQ_TRANS || p == P_BQ_LEN ||
        p == P_BA_MODE || p == P_BA_RANGE || p == P_BA_CHANCE || p == P_BQ_GATE || p == P_BS_WEIGHT || p == P_BS_PUNCH ||
        p == P_BS_BITE || p == P_BS_SPACE || p == P_GL_VIB_AUTO || (p >= P_DQ_ON && p <= P_DQ_TRANS) ||
        (p >= P_TP_FLUTTER && p <= P_TP_DROPOUT) || (p >= P_L1_AMT1 && p <= P_L3_AMT4) || p == P_SWELL)
        return PK_SCENE;
    if (p == P_KEYS || p == P_CANAL || p == P_CH_GLINT || p == P_CH_BASS || p == P_LOAD || p == P_CLOCK ||
        (p >= P_KY_DR_HARM && p <= P_KY_BS_STACK) || p == P_ARP_SNAP)
        return PK_STATE;
    if (p == P_SPEAKER || p == P_ANIM || p == P_METRO || p == P_COUNT_IN || p == P_SAVER)   /* (the FM-1's: its output,
                                                                                            * its screen, its GLO ›
                                                                                            * RECORD; the engine alone's
                                                                                            * frs_metronome,
                                                                                            * frs_count_in) */
        return PK_NONE;
    if (p == P_SQ_NOTE || p == P_SQ_ACC || p == P_SQ_NLEN || p == P_BQ_NOTE || p == P_BQ_NLEN || p == P_BQ_ACC ||
        p == P_DQ_NOTES || p == P_DQ_NLEN)
        return PK_STEP;
    if (p == P_SQ_CLEAR || p == P_SQ_NEW || p == P_SQ_UNDO || p == P_BQ_CLEAR || p == P_BQ_NEW || p == P_BQ_UNDO ||
        p == P_DQ_CLEAR || p == P_DQ_UNDO)
        return PK_ACTION;
    return PK_MOMENT;                    /* (FREEZE, KEYS, MORPH, RECORD, STEP, ARP II's LATCH) */
}

/* the engine's state beside a scene: "FRst", its version, then pairs (a setting, its value; 255: the metronome) */
#define STATE_METRO 255u
#define STATE_V 3u                       /* 2.6's numbers (2: 2.5's, its settings past its NSOUND, 153, one lower; 1:
                                          * 2.0's, past 130, 24 lower) */
#define STATE_BYTES 64u                  /* at most (frasil_engine.h's FRS_STATE_BYTES) */
static uint32_t state_save(uint8_t *out)
{
    uint32_t n = 0, p;
    out[n++] = 'F';
    out[n++] = 'R';
    out[n++] = 's';
    out[n++] = 't';
    out[n++] = STATE_V;
    for (p = 0; p < NPARAM; p++)
        if (param_kind(p) == PK_STATE) {
            out[n++] = (uint8_t)p;
            out[n++] = st.v[p];
        }
    out[n++] = STATE_METRO;
    out[n++] = st.metro;
    return n;
}

/* ------------------------------------------------------- saving, scenes --- */
static void arp_stop(arp_t *a);          /* (the arpeggiator, below) */

/* what plays, into a scene: the latched chords (by root, lowest first) and the arpeggios' notes */
static void scene_notes_take(scene_t *sc)
{
    uint32_t n, i;
    sc->nch = 0;
    for (n = 0; n < 128u && sc->nch < SC_CHORDS; n++)
        if (bit_get(lit, (int)n)) {
            int8_t *c = sc->ch[sc->nch++];
            c[0] = (int8_t)n;
            for (i = 0; i < 4u; i++)
                c[1 + i] = i < lat[n].k ? lat[n].n[i] : -1;
        }
    /* (an arpeggio not latched has only the notes keys hold: no finger on them when the scene comes back, they would never
     * stop. GLINT on KEYS: its list the ring, LATCH's) */
    sc->narp = !arp.latch && st.v[P_SQ_TYPE] ? 0u : arp.n > SC_ARP ? (uint8_t)SC_ARP : arp.n;
    for (i = 0; i < sc->narp; i++)
        sc->arp[i] = arp.held[i];
    sc->nbarp = !barp.latch ? 0u : barp.n > SC_ARP ? (uint8_t)SC_ARP : barp.n;
    for (i = 0; i < SC_ARP; i++)
        sc->barp[i] = i < sc->nbarp ? barp.held[i] : 0;
    sc->bass = bass_holds() || st.v[P_BQ_ON] == SQT_ARP || bseq_owns() || bass_note < 0
                   ? -1 : (int8_t)bass_sounding(bass_note);   /* as it sounds; held notes are not kept, as the drones' are
                                                               * not (nor the note of the bass's sequence, its arpeggio) */
    for (i = 0; i < SC_SEQ; i++)
        sc->seq[i] = seq[i];
    memcpy(sc->bnote, bnote, sizeof bnote);
    sc->bq_on = st.v[P_BQ_ON];
    sc->bq_steps = st.v[P_BQ_STEPS];
    sc->bq_trans = st.v[P_BQ_TRANS];
    sc->tempo = st.v[P_TEMPO];
    sc->bq_len = st.v[P_BQ_LEN];
    sc->ba_mode = st.v[P_BA_MODE];
    sc->ba_range = st.v[P_BA_RANGE];
    sc->ba_chance = st.v[P_BA_CHANCE];
    sc->bq_gate = st.v[P_BQ_GATE];
    sc->bs_weight = st.v[P_BS_WEIGHT];
    sc->bs_punch = st.v[P_BS_PUNCH];
    sc->bs_bite = st.v[P_BS_BITE];
    sc->bs_space = st.v[P_BS_SPACE];
    sc->gl_vib = st.v[P_GL_VIB_AUTO];
    dq_now(&sc->dq_on);                  /* the drone's sequence, its settings */
    memcpy(sc->dnote, dnote, sizeof dnote);
    tp_now(&sc->tp_flutter);             /* LOFI TAPE II's four, */
    memcpy(sc->lfo_amt, &st.v[P_L1_AMT1], sizeof sc->lfo_amt);   /* the LFOs' AMOUNTs, SWELL, last (the name: the caller's) */
    sc->swell = st.v[P_SWELL];
}

/* an arpeggio on a scene's notes, latched, from the next step */
static void arp_take(arp_t *a, const int8_t *notes, uint32_t n)
{
    uint32_t i;
    a->n = 0;
    for (i = 0; i < n && i < 16u; i++) {
        a->held[i] = notes[i];
        a->in[i] = ++a->ins;
        a->prs[i] = 0.0f;
    }
    a->n = (uint8_t)i;
    a->dirty = 1;                        /* from the next step */
    if (a->n) {
        a->latch = 1;
        a->vel = 100;
        a->paused = 0;
    }
}

/* a scene's notes, played (everything let go before): the drones latched again, the arpeggios (latched) on their
 * notes */
static void scene_notes_play(const scene_t *sc)
{
    uint32_t i, j;
    for (i = 0; i < sc->nch && i < SC_CHORDS; i++) {
        int root = sc->ch[i][0];
        chord_t c;
        if (root < 0)
            continue;
        c.k = 0;
        for (j = 0; j < 4u; j++)
            if (sc->ch[i][1 + j] >= 0)
                c.n[c.k++] = sc->ch[i][1 + j];
        lat[root] = c;
        bit_put(lit, root, 1);
        chord_on(&c, 100);
    }
    arp_take(&arp, sc->arp, sc->narp);
    arp_take(&barp, sc->barp, sc->nbarp);
    if (sc->bass >= 0)                   /* (kept as it sounds: played without its OCTAVE, the engine's) */
        bass_play(sc->bass - 12 * ((int)st.v[P_BS_OCTAVE] - 2), 100);
}

static void new_shard(scene_t *out);     /* (below) */

/* a scene from before LOFI TAPE II (or a factory scene) with its four as the tape was (TAPE_NONE: TAPE SPEED X1.00, TONE
 * in the middle, no hiss, no dropouts; not their defaults, which colour it: tape_defaults) */
static void tape_none(scene_t *sc)
{
    sc->tp_flutter = TAPE_NONE[0];
    sc->tp_tone = TAPE_NONE[1];
    sc->tp_noise = TAPE_NONE[2];
    sc->tp_dropout = TAPE_NONE[3];
}

static void tape_defaults(scene_t *sc)   /* a new one's: LOFI TAPE II's four at their defaults (X1.00, TONE +10, 0, 0) */
{
    sc->tp_flutter = PD[P_TP_FLUTTER].def;
    sc->tp_tone = PD[P_TP_TONE].def;
    sc->tp_noise = PD[P_TP_NOISE].def;
    sc->tp_dropout = PD[P_TP_DROPOUT].def;
}

/* the LFOs' twelve AMOUNTs of "FRS|" (3.0.2-b5's test builds: -100 % .. +100 %, 100 its 0 %), in place, as "FRS}"'s
 * multipliers: the size of each, |v - 100| (+100 % x1.00, -35 % x0.35; a multiplier has no sign); past 200, x1.00 */
static void amt_pct_over(uint8_t *a)
{
    uint32_t i;
    for (i = 0; i < SC_LFO_AMT; i++)
        a[i] = a[i] > 200u ? (uint8_t)PD[P_L1_AMT1].def : (uint8_t)(a[i] >= 100u ? a[i] - 100u : 100u - a[i]);
}

/* a scene with what "FRS}" added at its defaults: no name past sixteen, every LFO AMOUNT at x1.00 (the LFOs as they
 * moved them), SWELL at 0 */
static void more_none(scene_t *sc)
{
    memset(sc->name_more, 0, sizeof sc->name_more);
    memset(sc->lfo_amt, PD[P_L1_AMT1].def, sizeof sc->lfo_amt);
    sc->swell = PD[P_SWELL].def;
}

/* a scene with no arpeggio on the bass: its settings' defaults, no notes */
static void barp_none(scene_t *sc)
{
    sc->ba_mode = PD[P_BA_MODE].def;
    sc->ba_range = PD[P_BA_RANGE].def;
    sc->ba_chance = PD[P_BA_CHANCE].def;
    sc->bq_gate = PD[P_BQ_GATE].def;
    sc->bs_weight = sc->bs_punch = sc->bs_bite = sc->bs_space = sc->gl_vib = 0;
    sc->nbarp = 0;
    memset(sc->barp, 0, SC_ARP);
}

/* slot s with nothing saved in it: a factory scene (params.c FACTORY: made as NEW SHARD is, then its settings, or its
 * sound whole as an FM-1 stored it; its TEMPO and what it plays), or past them NEW SHARD, named by its number */
static void factory_get(uint32_t s, scene_t *out)
{
    const factory_t *f = &FACTORY[s < NFACTORY ? s : 0];
    uint32_t i, j, pass;
    if (s >= NFACTORY) {
        new_shard(out);
        memset(out->name, 0, SC_NAME);
        text_cat(out->name, "SCENE ");
        int_text((int32_t)s + 1, out->name + 6);
        return;
    }
    memset(out, 0, sizeof *out);
    barp_none(out);
    dseq_none(out);
    tape_none(out);
    more_none(out);
    memcpy(out->name, f->name, SC_NAME);
    if (f->whole) {
        memcpy(out->v, f->sound, FACTORY_NV);
        later_defaults(out->v, FACTORY_NV);
    } else {
        for (i = 0; i < NSOUND; i++)
            out->v[i] = PD[i].def;
        for (pass = 0; pass < 2u; pass++) {  /* its settings, over its GLINT preset's and the arpeggiator's defaults */
            for (i = 0; i < sizeof f->p / sizeof f->p[0] && f->p[i][0] != P_END; i++)
                out->v[f->p[i][0]] = f->p[i][1];
            if (!pass) {
                glint_preset(out->v, out->v[P_GL_PRESET]);
                arp_defaults(out->v);
            }
        }
    }
    for (i = 0; i < f->nch; i++)         /* its chords as latched, */
        memcpy(out->ch[out->nch++], f->ch[i], 5);
    for (i = 0; i < sizeof f->keys && f->keys[i]; i++) {   /* or the drones it latches: each key's chord */
        chord_t c;
        int root;
        chord_build(f->keys[i], out->v, &c, &root);
        out->ch[out->nch][0] = (int8_t)root;
        for (j = 0; j < 4u; j++)
            out->ch[out->nch][1 + j] = (int8_t)(j < c.k ? c.n[j] : -1);
        out->nch++;
    }
    for (i = 0; i < sizeof f->arp && f->arp[i]; i++)
        out->arp[out->narp++] = f->arp[i];
    out->bass = f->bass;
    memcpy(out->seq, f->seq, SC_SEQ);
    memcpy(out->bnote, f->bnote, SC_BSEQ);
    out->bq_on = f->bq_on;
    out->bq_steps = f->bq_steps;
    out->bq_trans = f->bq_trans;
    out->tempo = f->tempo;
    out->bq_len = f->bq_len;
}

/* a scene's name as text, t SC_NAME_ALL + 1 long: its first sixteen (all used: no NUL in the scene), then, those all
 * used, the rest (name_more) */
static char *name_text(const scene_t *sc, char *t)
{
    uint32_t i, n;
    for (n = 0; n < SC_NAME && sc->name[n]; n++)
        t[n] = sc->name[n];
    for (i = 0; n == SC_NAME + i && i < SC_NAME_MORE && sc->name_more[i]; i++)
        t[n++] = sc->name_more[i];
    t[n] = 0;
    return t;
}

/* a scene's bass sequence empty: every step a CUT, PLAYS on KEYS, sixteen steps of a sixteenth, not transposed */
static void bseq_empty(scene_t *sc)
{
    memset(sc->bnote, SQ_REST, SC_BSEQ);
    sc->bq_on = 0;
    sc->bq_steps = PD[P_BQ_STEPS].def;
    sc->bq_trans = PD[P_BQ_TRANS].def;
    sc->bq_len = PD[P_BQ_LEN].def;
    barp_none(sc);
}

static void new_shard(scene_t *out)      /* NEW SHARD (params.c): made as a factory scene is, then its own settings */
{
    uint32_t i;
    *out = NEW_SHARD;
    dseq_none(out);
    tape_defaults(out);
    more_none(out);
    glint_preset(out->v, out->v[P_GL_PRESET]);
    arp_defaults(out->v);
    later_defaults(out->v, P_BS_DRIVE);
    for (i = 0; i < sizeof NEW_SHARD_P / sizeof NEW_SHARD_P[0]; i++)
        out->v[NEW_SHARD_P[i][0]] = NEW_SHARD_P[i][1];
}

/* a scene's earlier layouts (a shard's, as the River and the iPad keep them; the FM-1's store has its own records), each
 * its bytes: this one's, FRS} (FRS|'s bytes too: frasil_engine.c frs_scene_amt_over; the name's rest, the LFOs' AMOUNTs, SWELL last); FRS{'s (LOFI TAPE II's four last), before
 * it; FRSz's (3.0: the drone's sequence last), before that; FRSy's; 2.7's test builds' FRSx (before AUTO
 * VIB), FRSw (before the bass's SPACE), FRSv (before BASS TONE), FRSu (the first: to the bass's RATE); 2.6's FRSt */
#define FRSZ_SCENE 586u
#define FRSTP_SCENE 590u                 /* ("FRS{": LOFI TAPE II's four after FRSz's) */
#define FRSBAR_SCENE ((uint32_t)sizeof(scene_t))   /* ("FRS}": the name's rest, the LFOs' AMOUNTs, SWELL after FRS{'s) */
#define FRSY_SCENE 324u
#define FRSX_SCENE 323u
#define FRSW_SCENE 322u
#define FRSV_SCENE 319u
#define FRSU_SCENE 298u
#define FRST_SCENE 297u

/* sc, as a layout read it, its first n bytes (whatever was after them), made over in place: what it did not have yet at
 * its defaults (no name past sixteen, the LFOs' AMOUNTs x1.00, SWELL 0; LOFI TAPE II's four as the tape was; the
 * drone's sequence none; AUTO VIB, the bass's SPACE, BASS TONE 0; before FRSv, PLAYS' 1 (SEQUENCE on)
 * is 2, the bass's arpeggio none, its settings and GATE their defaults; FRSt's bass, its sixteen notes then their
 * lengths, onto the one grid). 0, or -1: not one it knows, as it was */
static int scene_over(scene_t *sc, uint32_t n)
{
    if (n == FRSBAR_SCENE)
        return 0;
    if (n != FRSTP_SCENE && n != FRSZ_SCENE && n != FRSY_SCENE && n != FRSX_SCENE && n != FRSW_SCENE && n != FRSV_SCENE && n != FRSU_SCENE && n != FRST_SCENE)
        return -1;
    if (n == FRSX_SCENE) {               /* (AUTO VIB not yet) */
        sc->gl_vib = 0;
    } else if (n == FRSW_SCENE) {        /* (the bass's SPACE not yet) */
        sc->bs_space = sc->gl_vib = 0;
    } else if (n == FRSV_SCENE) {        /* (BASS TONE's three not yet, before its arpeggio's notes either) */
        uint8_t *q = &sc->bs_weight;
        uint32_t i;
        for (i = 1u + SC_ARP; i-- > 0u;)
            q[3u + i] = q[i];            /* (to nbarp, three on) */
        sc->bs_weight = sc->bs_punch = sc->bs_bite = sc->bs_space = sc->gl_vib = 0;
    } else if (n <= FRSU_SCENE) {
        if (n == FRST_SCENE)
            bseq_over(sc->bnote, &sc->bq_steps, &sc->bq_len);
        sc->bq_on = sc->bq_on ? SQT_SEQ : 0u;   /* (SEQUENCE on was 1) */
        barp_none(sc);
    }
    if (n < FRSZ_SCENE)
        dseq_none(sc);                   /* (every one before FRSz: no drone's sequence) */
    if (n < FRSTP_SCENE)
        tape_none(sc);                   /* (every one before FRS{: LOFI TAPE II's four as the tape was) */
    more_none(sc);                       /* (every one before FRS}: no name's rest, the LFOs whole, no SWELL) */
    return 0;
}

static int grid_top(int start);          /* (below) */
static void seqs_top(int glint, int bass, int drone);

/* BASS TONE's three, a scene's, as its sound: taken with it (the other settings past the sound it keeps, the same:
 * the bass's SPACE, AUTO VIB, LOFI TAPE II's four, the LFOs' AMOUNTs, SWELL) */
static void bass_tone(const scene_t *sc)
{
    uint32_t i;
    st.v[P_BS_WEIGHT] = sc->bs_weight > PD[P_BS_WEIGHT].max ? PD[P_BS_WEIGHT].def : sc->bs_weight;
    st.v[P_BS_PUNCH] = sc->bs_punch > PD[P_BS_PUNCH].max ? PD[P_BS_PUNCH].def : sc->bs_punch;
    st.v[P_BS_BITE] = sc->bs_bite > PD[P_BS_BITE].max ? PD[P_BS_BITE].def : sc->bs_bite;
    st.v[P_BS_SPACE] = sc->bs_space > PD[P_BS_SPACE].max ? PD[P_BS_SPACE].def : sc->bs_space;
    st.v[P_GL_VIB_AUTO] = sc->gl_vib > PD[P_GL_VIB_AUTO].max ? PD[P_GL_VIB_AUTO].def : sc->gl_vib;
    st.v[P_TP_FLUTTER] = sc->tp_flutter > PD[P_TP_FLUTTER].max ? PD[P_TP_FLUTTER].def : sc->tp_flutter;   /* LOFI TAPE II's */
    st.v[P_TP_TONE] = sc->tp_tone > PD[P_TP_TONE].max ? PD[P_TP_TONE].def : sc->tp_tone;
    st.v[P_TP_NOISE] = sc->tp_noise > PD[P_TP_NOISE].max ? PD[P_TP_NOISE].def : sc->tp_noise;
    st.v[P_TP_DROPOUT] = sc->tp_dropout > PD[P_TP_DROPOUT].max ? PD[P_TP_DROPOUT].def : sc->tp_dropout;
    for (i = 0; i < SC_LFO_AMT; i++)     /* the LFOs' AMOUNTs, SWELL */
        st.v[P_L1_AMT1 + i] = sc->lfo_amt[i] > PD[P_L1_AMT1 + i].max ? PD[P_L1_AMT1 + i].def : sc->lfo_amt[i];
    st.v[P_SWELL] = sc->swell > PD[P_SWELL].max ? PD[P_SWELL].def : sc->swell;
}

/* a scene's sound and what it plays, taken as a load takes them (from a slot: the FM-1's scene_load; a shard the River
 * plays: scene_audition) */
static void count_stop(void);            /* (COUNT-IN, below) */
/* a scene's setting as this engine takes it: past its range, its range's last; a TARGET past the list this engine knows
 * (another's LFO TARGETs: the LEAD's, card #144, on an engine without it) none, not the list's last (DROPOUT) */
static uint8_t sound_val(uint32_t p, uint32_t x)
{
    return x <= PD[p].max ? (uint8_t)x : PD[p].fmt == F_LTGT ? 0u : PD[p].max;
}

static void scene_take(const scene_t *sc)
{
    uint32_t i;
    count_stop();                        /* (a count under way: no more) */
    anchor_drop();                       /* (another scene: no ANCHOR, no way back to it) */
    rec_was.on = 0;                      /* (its PLAYS are the scene's now) */
    for (i = 0; i < NSOUND; i++)
        st.v[i] = sound_val(i, sc->v[i]);
    drone_take();
    for (i = 0; i < SC_SEQ; i++)
        seq[i] = sc->seq[i];
    seq_view();
    if (st.v[P_SQ_TYPE] != SQT_SEQ) {    /* (its sequence: from the top, below) */
        seq_note_off();
        sq.run = 0;
        if (st.v[P_SQ_REC]) {
            st.v[P_SQ_REC] = 0;
            rec_keys(P_SQ_REC);
        }
    }
    bq.tn = -1;                          /* (a take under way: gone with the loop it was in) */
    memcpy(bnote, sc->bnote, sizeof bnote);   /* and the bass's, with its own PLAYS, SEQ LENGTH, TRANSPOSE, RATE, its
                                               * arpeggio's settings, GATE */
    st.v[P_BQ_ON] = sc->bq_on > PD[P_BQ_ON].max ? PD[P_BQ_ON].def : sc->bq_on;
    st.v[P_BQ_STEPS] = sc->bq_steps > PD[P_BQ_STEPS].max ? PD[P_BQ_STEPS].def : sc->bq_steps;
    st.v[P_BQ_TRANS] = sc->bq_trans > PD[P_BQ_TRANS].max ? PD[P_BQ_TRANS].def : sc->bq_trans;
    st.v[P_BQ_LEN] = sc->bq_len > PD[P_BQ_LEN].max ? PD[P_BQ_LEN].def : sc->bq_len;
    st.v[P_BA_MODE] = sc->ba_mode > PD[P_BA_MODE].max ? PD[P_BA_MODE].def : sc->ba_mode;
    st.v[P_BA_RANGE] = sc->ba_range > PD[P_BA_RANGE].max ? PD[P_BA_RANGE].def : sc->ba_range;
    st.v[P_BA_CHANCE] = sc->ba_chance > PD[P_BA_CHANCE].max ? PD[P_BA_CHANCE].def : sc->ba_chance;
    st.v[P_BQ_GATE] = sc->bq_gate > PD[P_BQ_GATE].max ? PD[P_BQ_GATE].def : sc->bq_gate;
    bass_tone(sc);
    if (sc->tempo <= PD[P_TEMPO].max)    /* its TEMPO (a MIDI clock coming in plays on: the clock's) */
        st.v[P_TEMPO] = sc->tempo;
    bseq_view();
    if (st.v[P_BQ_ON] != SQT_SEQ) {      /* (its sequence: from the top, below) */
        bseq_stop();
        if (step_hand && st.v[P_BQ_REC]) {   /* (nothing to record into, the keys back: as GLINT's and the drone's) */
            st.v[P_BQ_REC] = 0;
            rec_keys(P_BQ_REC);
        }
    }
    dseq_take(sc);                       /* and the drone's, with its own settings (on: from the top, below) */
    if (!st.v[P_DQ_ON]) {
        dseq_stop();
        if (st.v[P_DQ_REC]) {
            st.v[P_DQ_REC] = 0;
            rec_keys(P_DQ_REC);
        }
    }
    arp.paused = barp.paused = 0;
    eng_type_changed();
    sound_mark();
    if (st.loading_morph)                /* (scene_go sets the glide first: no step to the new sound) */
        st.engine_dirty = 1;
    else
        engine_push();
    release_all();                       /* a scene starts clean: every note let go, the arpeggios stopped */
    arp.n = arp.pn = arp.gap = 0;
    arp.dirty = 0;
    arp_stop(&arp);
    barp.n = barp.pn = barp.gap = 0;
    barp.dirty = 0;
    arp_stop(&barp);
    {                                    /* then what it plays, if anything, and its sequences from the top: the beat
                                          * begins there, as PLAY starts it (following a MIDI clock, on its next
                                          * step: its beats kept) */
        int gs = st.v[P_SQ_TYPE] == SQT_SEQ, bs = st.v[P_BQ_ON] == SQT_SEQ, ds = st.v[P_DQ_ON] != 0, top = grid_top(0);
        if (sc->nch || sc->narp || sc->nbarp || sc->bass >= 0)
            scene_notes_play(sc);
        if (top) {
            seqs_top(gs, bs, ds);
        } else {
            if (gs)
                seq_begin();
            if (bs)
                bseq_begin();
            if (ds)
                dseq_begin();
        }
    }
}

/* the River plays a shard (eng_scene_play): its scene as a load takes it, out of any MORPH; with `hold` (▶), one that
 * plays nothing holds a chord on its ROOT, in the third octave, as a key would (aud_root: its root, -1 none; what the
 * shard plays edited lets it go) */
static int aud_root = -1;
static void scene_audition(const scene_t *sc, int hold)
{
    uint32_t i, notes = 0;
    mo.on = 0;
    memset(morph_off, 0, sizeof morph_off);
    aud_root = -1;
    scene_take(sc);
    if (!hold)
        return;
    for (i = 0; i < SC_SEQ; i++)
        notes += (sc->seq[i] & 127u) != SQ_REST && (sc->seq[i] & 127u) != SQ_TIE;
    if (!sc->nch && !sc->narp && !sc->nbarp && sc->bass < 0 && !(st.v[P_SQ_TYPE] == SQT_SEQ && notes) &&
        !(st.v[P_DQ_ON] && dseq_has_notes())) {   /* (the drone's sequence plays its own) */
        chord_t c;
        int root;
        chord_build(48 + (int)(st.v[P_ROOT] % 12u), st.v, &c, &root);
        lat[root] = c;
        bit_put(lit, root, 1);
        chord_on(&c, 100);
        aud_root = root;
    }
}

static void audition_stop(void)          /* every note let go, as PLAY held */
{
    release_all();
    arp.n = arp.pn = arp.gap = 0;
    arp.dirty = arp.paused = 0;
    arp_stop(&arp);
    barp.n = barp.pn = barp.gap = 0;
    barp.dirty = barp.paused = 0;
    arp_stop(&barp);
    seq_note_off();
    sq.run = 0;
    bseq_stop();
    dseq_stop();
}

static scene_t rst;                     /* what PLAY held let go, for RESTORE (its notes alone) */
static uint8_t rst_valid;

/* a scene, as SCENES' MORPH and LOAD have it: LOAD SND+NOTES lets everything go and plays the scene's notes (as
 * scene_load); LOAD SND ONLY leaves what plays (the drones, the bass, GLINT's arpeggio and sequence, with its SEQ and
 * the settings that play it) and changes the sound alone. MORPH, CUT or a time: the sound glides there, the settings
 * of a few choices (presets, MODEL, ROOT, TARGET ...) at once, each a hand turns out of it */
static const uint8_t PLAYS_P[8] = {P_SQ_TYPE, P_ARP_MODE, P_ARP_RATE, P_ARP_RANGE, P_ARP_CHANCE, P_SQ_LEN, P_SQ_GATE,
                                   P_SQ_TRANS};
static int morphs(uint32_t p)            /* a setting that glides (not a choice among a few, not a name) */
{
    uint32_t f = PD[p].fmt;
    return PD[p].max > 16u && f != F_LTGT && f != F_DPRESET && f != F_GPRESET && f != F_LRATE && f != F_SQLEN &&
           f != F_DTIME && f != F_SQTRANS && f != F_CENTS;
}

static void eng_scene_go(const scene_t *sc)   /* (the FM-1's scene_go: its slot's) */
{
    static const uint32_t MORPH_MS[9] = {0, 1000, 2000, 4000, 8000, 15000, 30000, 60000, 120000};
    static float from[NSOUND];
    uint32_t i, dur = MORPH_MS[st.v[P_MORPH] % 9u];
    anchor_drop();                       /* (another scene: no ANCHOR; a way back under way glides on from where it is) */
    for (i = 0; i < NSOUND; i++)         /* (where it is now, a MORPH under way too) */
        from[i] = (float)st.v[i] + morph_off[i];
    for (i = NSOUND; i < NMORPH; i++)    /* (those ANCHOR's alone glides: at the scene's) */
        morph_off[i] = mo.d[i] = 0.0f;
    if (st.v[P_LOAD]) {
        uint8_t kept[sizeof PLAYS_P];
        for (i = 0; i < sizeof PLAYS_P; i++)
            kept[i] = st.v[PLAYS_P[i]];
        for (i = 0; i < NSOUND; i++)
            st.v[i] = sound_val(i, sc->v[i]);
        bass_tone(sc);
        drone_take();
        sound_mark();                    /* (the scene as saved: its own sequences) */
        for (i = 0; i < SC_SEQ; i++)
            saved_seq[i] = sc->seq[i];
        memcpy(saved_bnote, sc->bnote, sizeof saved_bnote);
        memcpy(saved_bq, &sc->bq_on, sizeof saved_bq);   /* (PLAYS, SEQ LENGTH, TRANSPOSE, TEMPO, RATE, MODE .. GATE) */
        saved_bsp = sc->bs_space;
        saved_gv = sc->gl_vib;
        memcpy(saved_tp, &sc->tp_flutter, sizeof saved_tp);
        memcpy(saved_amt, sc->lfo_amt, sizeof saved_amt);
        saved_sw = sc->swell;
        memcpy(saved_dnote, sc->dnote, sizeof saved_dnote);   /* (the drone's sequence: what plays stays) */
        memcpy(saved_dq, &sc->dq_on, sizeof saved_dq);
        for (i = 0; i < sizeof PLAYS_P; i++)
            st.v[PLAYS_P[i]] = kept[i];
        st.dirty = (uint8_t)sound_changed();
        st.engine_dirty = 1;
    } else {
        st.loading_morph = dur != 0u;
        scene_take(sc);
        st.loading_morph = 0;
    }
    mo.on = dur != 0u;
    for (i = 0; i < NSOUND; i++) {
        mo.d[i] = dur && morphs(i) ? from[i] - (float)st.v[i] : 0.0f;
        morph_off[i] = mo.d[i];
    }
    st.engine_dirty = 1;
    if (dur) {
        mo.t0 = eng_ms();
        mo.dur = dur;
        name_text(sc, mo.name);
    } else {
        char t[SC_NAME_ALL + 1];
        eng_say("LOADED", name_text(sc, t));
    }
}

static uint32_t morph_at;                /* (morph_run's last) */
static void morph_run(void)              /* a hundred times a second while a MORPH is under way */
{
    uint32_t now = eng_ms(), i;
    float k, g;
    if (!mo.on || now - morph_at < 10u)
        return;
    morph_at = now;
    k = (float)(now - mo.t0) / (float)mo.dur;
    if (mo.anc && anc.due && (k >= 1.0f || now - mo.t0 >= anc.half))
        anc_switch();                    /* (ANCHOR's way back: its choices at its half) */
    if (k >= 1.0f) {
        mo.on = mo.anc = 0;
        for (i = 0; i < NMORPH; i++)
            morph_off[i] = mo.d[i] = 0.0f;
    } else {
        g = 1.0f - k * k * (3.0f - 2.0f * k);
        for (i = 0; i < NMORPH; i++)
            morph_off[i] = mo.d[i] * g;
    }
    st.engine_dirty = 1;
}

/* ------------------------------------------------------------------ ANCHOR --- */
/* the scene's settings past the sound, where scene_t keeps them (scene_take's) */
static const struct {
    uint8_t p;
    uint16_t at;
} ANC_X[] = {{P_BQ_ON, offsetof(scene_t, bq_on)}, {P_BQ_STEPS, offsetof(scene_t, bq_steps)},
             {P_BQ_TRANS, offsetof(scene_t, bq_trans)}, {P_TEMPO, offsetof(scene_t, tempo)},
             {P_BQ_LEN, offsetof(scene_t, bq_len)}, {P_BA_MODE, offsetof(scene_t, ba_mode)},
             {P_BA_RANGE, offsetof(scene_t, ba_range)}, {P_BA_CHANCE, offsetof(scene_t, ba_chance)},
             {P_BQ_GATE, offsetof(scene_t, bq_gate)}, {P_BS_WEIGHT, offsetof(scene_t, bs_weight)},
             {P_BS_PUNCH, offsetof(scene_t, bs_punch)}, {P_BS_BITE, offsetof(scene_t, bs_bite)},
             {P_BS_SPACE, offsetof(scene_t, bs_space)}, {P_GL_VIB_AUTO, offsetof(scene_t, gl_vib)},
             {P_DQ_ON, offsetof(scene_t, dq_on)}, {P_DQ_RATE, offsetof(scene_t, dq_rate)},
             {P_DQ_LEN, offsetof(scene_t, dq_len)}, {P_DQ_GATE, offsetof(scene_t, dq_gate)},
             {P_DQ_CHANCE, offsetof(scene_t, dq_chance)}, {P_DQ_TRANS, offsetof(scene_t, dq_trans)},
             {P_TP_FLUTTER, offsetof(scene_t, tp_flutter)}, {P_TP_TONE, offsetof(scene_t, tp_tone)},
             {P_TP_NOISE, offsetof(scene_t, tp_noise)}, {P_TP_DROPOUT, offsetof(scene_t, tp_dropout)},
             {P_SWELL, offsetof(scene_t, swell)},
             {P_L1_AMT1, offsetof(scene_t, lfo_amt)}, {P_L1_AMT2, offsetof(scene_t, lfo_amt) + 1u},
             {P_L1_AMT3, offsetof(scene_t, lfo_amt) + 2u}, {P_L1_AMT4, offsetof(scene_t, lfo_amt) + 3u},
             {P_L2_AMT1, offsetof(scene_t, lfo_amt) + 4u}, {P_L2_AMT2, offsetof(scene_t, lfo_amt) + 5u},
             {P_L2_AMT3, offsetof(scene_t, lfo_amt) + 6u}, {P_L2_AMT4, offsetof(scene_t, lfo_amt) + 7u},
             {P_L3_AMT1, offsetof(scene_t, lfo_amt) + 8u}, {P_L3_AMT2, offsetof(scene_t, lfo_amt) + 9u},
             {P_L3_AMT3, offsetof(scene_t, lfo_amt) + 10u}, {P_L3_AMT4, offsetof(scene_t, lfo_amt) + 11u}};
#define NANC_X (sizeof ANC_X / sizeof ANC_X[0])

static int anc_x(uint32_t p)             /* its place in ANC_X, -1: none (not a scene's) */
{
    uint32_t i;
    for (i = 0; i < NANC_X; i++)
        if (ANC_X[i].p == p)
            return (int)i;
    return -1;
}

static int anc_glides(uint32_t p)        /* it glides back (else it switches) */
{
    return p < NSOUND ? morphs(p) : p == P_SWELL || (p >= P_BS_WEIGHT && p < NMORPH);
}

/* setting p as the anchor would have it played (as scene_take takes it: the drone's preset on its settings), p a
 * scene's */
static uint8_t anc_val(uint32_t p)
{
    int i;
    if (p < NSOUND) {
        uint8_t pre = anc.sc.v[P_DR_PRESET] > PD[P_DR_PRESET].max ? PD[P_DR_PRESET].max : anc.sc.v[P_DR_PRESET];
        if (pre && drone_of[p] >= 0)
            return DRONE_PRESET[(pre - 1u) % NDPRESET][drone_of[p]];
        return sound_val(p, anc.sc.v[p]);
    }
    i = anc_x(p);
    if (i < 0)
        return st.v[p];
    {
        uint8_t x = ((const uint8_t *)&anc.sc)[ANC_X[i].at];
        return x > PD[p].max ? PD[p].def : x;
    }
}

static int anc_pend(uint32_t p) { return (int)(anc.pend[p >> 5] >> (p & 31u) & 1u); }
static void anc_pend_put(uint32_t p, int on)
{
    if (on)
        anc.pend[p >> 5] |= 1u << (p & 31u);
    else
        anc.pend[p >> 5] &= ~(1u << (p & 31u));
}

static uint32_t anc_takes(void)          /* the tracks a take is under way on (1 GLINT, 2 the bass, 4 the drone) */
{
    return (st.v[P_SQ_REC] ? 1u : 0u) | (st.v[P_BQ_REC] ? 2u : 0u) | (st.v[P_DQ_REC] ? 4u : 0u);
}

static void anc_keep_track(uint32_t t)   /* track t's sequence the hand's: its steps, PLAYS, SEQ LENGTH out of the way back */
{
    anc.seqs &= (uint8_t)~t;
    if (t & 1u) {
        anc_pend_put(P_SQ_TYPE, 0);
        anc_pend_put(P_SQ_LEN, 0);
    }
    if (t & 2u) {
        anc_pend_put(P_BQ_ON, 0);
        anc_pend_put(P_BQ_STEPS, 0);
    }
    if (t & 4u) {
        anc_pend_put(P_DQ_ON, 0);
        anc_pend_put(P_DQ_LEN, 0);
    }
}

/* a hand on setting p while a way back's switch is to come: p out of it, and what p's own turning sets (a preset's
 * settings, CUSTOM's, a sequence edited, a take begun) */
static void anc_hand(int p)
{
    uint32_t i;
    anc_pend_put((uint32_t)p, 0);
    if (drone_of[p] >= 0 && anc_pend(P_DR_PRESET)) {   /* (CUSTOM from here: its settings as they are now) */
        anc_pend_put(P_DR_PRESET, 0);
        for (i = 0; i < NDRONE; i++)
            custom[i] = st.v[DRONE_P[i]];
    }
    if (p == P_DR_PRESET)
        for (i = 0; i < NDRONE; i++)
            anc_pend_put(DRONE_P[i], 0);
    if (p == P_GL_PRESET) {
        for (i = P_GL_RATIO; i < P_GL_RATIO + 7u; i++)
            anc_pend_put(i, 0);
        anc_pend_put(P_GL_ATTACK, 0);
        anc_pend_put(P_GL_PING, 0);
        anc_pend_put(P_GL_SUSTAIN, 0);
    }
    if (p == P_EQ_PRESET)
        for (i = P_EQ_LOW; i <= P_EQ_HIGHF; i++)
            anc_pend_put(i, 0);
    if (p >= P_EQ_LOW && p <= P_EQ_HIGHF)
        anc_pend_put(P_EQ_PRESET, 0);
    if ((p >= P_SQ_NOTE && p <= P_SQ_CLEAR) || p == P_SQ_NEW || p == P_SQ_UNDO || p == P_SQ_NLEN)
        anc_keep_track(1u);
    if (p == P_BQ_NOTE || p == P_BQ_REC || p == P_BQ_CLEAR || (p >= P_BQ_ACC && p <= P_BQ_UNDO) || p == P_BQ_NLEN)
        anc_keep_track(2u);
    if ((p >= P_DQ_REC && p <= P_DQ_CLEAR) || (p >= P_DQ_NOTES && p <= P_DQ_UNDO))
        anc_keep_track(4u);
}

/* the way back's switch: the sequences' steps, then the settings that switch, PLAYS and the RATEs as a hand turns them
 * (a sequence started or stopped, the step playing kept) */
static void anc_switch(void)
{
    static const uint8_t TURNED[] = {P_BQ_LEN, P_DQ_RATE, P_SQ_TYPE, P_BQ_ON, P_DQ_ON};
    uint8_t turn[sizeof TURNED];
    uint32_t i, p;
    anc.due = 0;
    if (anc.seqs & 1u)
        memcpy(seq, anc.sc.seq, sizeof seq);
    if (anc.seqs & 2u)
        memcpy(bnote, anc.sc.bnote, sizeof bnote);
    if (anc.seqs & 4u) {
        memcpy(dnote, anc.sc.dnote, sizeof dnote);
        for (i = 0; i < SC_DSEQ; i++)
            dstep_sane(dnote[i]);
        dq_memo_at = -1;
    }
    anc.seqs = 0;
    for (i = 0; i < sizeof TURNED; i++) {
        turn[i] = (uint8_t)anc_pend(TURNED[i]);
        anc_pend_put(TURNED[i], 0);
    }
    for (p = 0; p < NPARAM; p++)
        if (anc_pend(p))
            st.v[p] = anc_val(p);
    memset(anc.pend, 0, sizeof anc.pend);
    for (i = 0; i < sizeof TURNED; i++)
        if (turn[i])
            param_set(TURNED[i], anc_val(TURNED[i]));
    seq_view();
    bseq_view();
    dseq_view();
    st.dirty = (uint8_t)sound_changed();
    st.engine_dirty = 1;
}

static void anchor_drop(void)            /* no ANCHOR, no way back under way (what glides, gliding on) */
{
    anc.on = anc.due = 0;
    mo.anc = 0;
}

static void anchor_settle(void)          /* a way back under way: its switch now (a STORE, a new ANCHOR: where it goes) */
{
    if (anc.due)
        anc_switch();
}

static void anchor_set(void)             /* the scene as it is now, kept (as STORE keeps it) */
{
    uint32_t i;
    anchor_settle();
    memset(&anc.sc, 0, sizeof anc.sc);
    for (i = 0; i < NSOUND; i++)
        anc.sc.v[i] = kept_v(i);
    scene_notes_take(&anc.sc);
    anc.on = 1;
}

/* back to the ANCHOR: ms 0 at once, else gliding over ms; the anchor let go. None, a way back under way ends at once
 * (its values there). 0, or -1 neither */
static int anchor_back(uint32_t ms)
{
    uint32_t i, t;
    if (!anc.on) {
        if (!mo.on || !mo.anc)
            return -1;
        ms = 0;                          /* (where it went, now) */
    } else {
        anc.on = 0;
        for (i = 0; i < NDRONE; i++)     /* (the drone's CUSTOM, as the anchor keeps it) */
            custom[i] = anc.sc.v[DRONE_P[i]] > PD[DRONE_P[i]].max ? PD[DRONE_P[i]].max : anc.sc.v[DRONE_P[i]];
        memset(anc.pend, 0, sizeof anc.pend);
        for (i = 0; i < NMORPH; i++) {
            float from = (float)st.v[i] + morph_off[i];   /* (where it is heard now, a MORPH under way too) */
            mo.d[i] = morph_off[i] = 0.0f;
            if (i >= NSOUND && anc_x(i) < 0)
                continue;
            if (anc_glides(i)) {
                uint8_t x = anc_val(i);
                st.v[i] = x;
                if (ms)
                    mo.d[i] = morph_off[i] = from - (float)x;
            } else if (anc_val(i) != st.v[i]) {
                anc_pend_put(i, 1);
            }
        }
        for (i = 0; i < NANC_X; i++)
            if (ANC_X[i].p >= NMORPH && anc_val(ANC_X[i].p) != st.v[ANC_X[i].p])
                anc_pend_put(ANC_X[i].p, 1);
        anc.seqs = (uint8_t)((memcmp(seq, anc.sc.seq, sizeof seq) ? 1u : 0u) |
                             (memcmp(bnote, anc.sc.bnote, sizeof bnote) ? 2u : 0u) |
                             (memcmp(dnote, anc.sc.dnote, sizeof dnote) ? 4u : 0u));
        for (t = anc_takes(), i = 1u; i <= 4u; i <<= 1)
            if (t & i)
                anc_keep_track(i);
        anc.due = 1;
        mo.on = mo.anc = 0;
    }
    if (ms) {
        mo.on = mo.anc = 1;
        mo.t0 = eng_ms();
        mo.dur = ms;
        anc.half = ms / 2u;
        mo.name[0] = 0;
        text_cat(mo.name, ANCHOR_NAME);
    } else {
        for (i = 0; i < NMORPH; i++)
            morph_off[i] = mo.d[i] = 0.0f;
        mo.on = mo.anc = 0;
        anchor_settle();
    }
    st.dirty = (uint8_t)sound_changed();
    st.engine_dirty = 1;
    return 0;
}

/* 0 none, 1 set, 2 a way back under way (*k: how far, 0 .. 1, by the clock) */
static int anchor_state(float *k)
{
    if (k)
        *k = mo.on && mo.anc && mo.dur ? (float)(eng_ms() - mo.t0) / (float)mo.dur : 0.0f;
    if (k && *k > 1.0f)
        *k = 1.0f;
    return anc.on ? 1 : mo.on && mo.anc ? 2 : 0;
}

/* -------------------------------------------------------- the arpeggiator --- */
/* a layer's keys held, played one at a time at RATE (MIDI clock when one comes in, else TEMPO), over RANGE octaves,
 * each step played or not as CHANCE says. GLINT's (arp) and the bass's (barp), one arpeggiator: each layer's notes,
 * MODE, RANGE, CHANCE, latch and waiting its own, the grid one. The bass stays one note: each step played as any bass
 * note is (bass_play), let go half a step on as GLINT's is (bass_stop), its RELEASE after it: GLIDE from the last, still
 * ringing out (from silence it does not: bass.c). In the main loop: its timing is the loop's (a millisecond or two). */

static uint32_t a_rand(arp_t *a)
{
    a->rng ^= a->rng << 13;
    a->rng ^= a->rng >> 17;
    a->rng ^= a->rng << 5;
    return a->rng;
}
static uint32_t arp_rand(void) { return a_rand(&arp); }

static void arp_add(arp_t *a, int note, uint32_t vel)
{
    uint32_t i, j;
    if (a->n >= 16u)
        return;
    for (i = 0; i < a->n; i++)
        if (a->held[i] == note)
            return;
    for (i = 0; i < a->n && a->held[i] < note; i++)
        ;
    for (j = a->n; j > i; j--) {
        a->held[j] = a->held[j - 1];
        a->in[j] = a->in[j - 1];
        a->prs[j] = a->prs[j - 1];
    }
    a->held[i] = (int8_t)note;
    a->in[i] = ++a->ins;
    a->prs[i] = 0.0f;
    a->n++;
    a->dirty = 1;                        /* it plays from the next step (ARP SNAP: on its grid) */
    a->chg_ms = eng_ms();
    a->vel = (uint8_t)vel;
}

static void arp_remove(arp_t *a, int note)
{
    uint32_t i, j;
    for (i = 0; i < a->n; i++)
        if (a->held[i] == note) {
            for (j = i; j + 1u < a->n; j++) {
                a->held[j] = a->held[j + 1u];
                a->in[j] = a->in[j + 1u];
                a->prs[j] = a->prs[j + 1u];
            }
            a->n--;
            a->dirty = 1;
            a->chg_ms = eng_ms();
            return;
        }
}

static void arp_stop(arp_t *a)
{
    if (a->sounding >= 0) {
        if (a == &arp)
            frasil_glint_off(a->sounding);
        else if (bass_note == a->sounding)   /* (let go already: not another's note) */
            bass_stop();
    }
    a->sounding = -1;
}

static void arp_wait(arp_t *a, int w)    /* PLAY: it waits, or on from the top of its pattern */
{
    a->paused = (uint8_t)w;
    a->dirty = 1;
    if (w)
        arp_stop(a);
}

/* a key of this kind down now, but key `except` */
static int keys_down(uint32_t kind, uint32_t except)
{
    uint32_t k;
    for (k = 0; k < FRS_NKEYS; k++)
        if (k != except && key[k].kind == kind)
            return 1;
    return 0;
}

/* ARP SNAP (P_ARP_SNAP: GLOBAL's K4, the device's): a new set (a chord, a note more or less) is taken on the snap grid,
 * the pattern from its top there, the old one playing on till then; the grid itself never moves for it. Its hand first:
 * a take waits till no key has come for ARP_GATHER_MS (keys a little apart are one chord, from its lowest), its step held
 * back that long at most past ARP_LATE_MS; a set that comes ARP_LATE_MS after a snap point still lands on it, as if it
 * had come on time (the pattern where it would be now). From silence it starts on the arpeggio's next step, as before.
 * Latched, STACK off: a key or note is a new hand when no other of its kind is down, or the others went down more than
 * ARP_HAND_MS before it (a legato change: the old chord's keys still down as the new one comes). OFF: all as before */
#define ARP_GATHER_MS 35u
#define ARP_LATE_MS 80u
#define ARP_HAND_MS 100u

static int arp_new_hand(arp_t *a, int others)
{
    return !others || (st.v[P_ARP_SNAP] && eng_ms() - a->hand_ms > ARP_HAND_MS);
}

/* the latch let go: the arpeggio keeps only the notes still held (their keys down, THE KEYS' fingers; MIDI's go, as
 * they always went) */
static void arp_keep_held(arp_t *a)
{
    uint32_t i = 0, k, j, kind = a == &arp ? K_GLINT : K_BARP;
    while (i < a->n) {
        int down = 0, n = a->held[i];
        for (k = 0; k < FRS_NKEYS; k++)
            if (key[k].kind == kind)
                for (j = 0; j < key[k].chord.k; j++)
                    down |= key[k].chord.n[j] == n;
        down |= bit_get(a == &arp ? hold_a : hold_ba, n);
        if (down)
            i++;
        else
            arp_remove(a, a->held[i]);
    }
}

/* its note n pressed (0..1): the steps from it louder, the one sounding at once */
static void arp_press(arp_t *a, int n, float f)
{
    uint32_t i;
    for (i = 0; i < a->n; i++)
        if (a->held[i] == n)
            a->prs[i] = f;
    for (i = 0; i < a->pn; i++)
        if (a->play[i] == n)
            a->pprs[i] = f;
    if (a->sounding >= 0 && a->src == n) {
        if (a == &arp)
            frasil_glint_pressure(a->sounding, f);
        else
            frasil_bass_pressure(f);
    }
}

/* GLINT's delay on the beat: the MIDI clock's tempo when one comes in, else TEMPO; halved while too long */
static void delay_sync(void)
{
    float bpm = eng_ms() - grid.clk_at < 500u && grid.clk_ms > 1.0f ? 2500.0f / grid.clk_ms : (float)(st.v[P_TEMPO] + 30u);
    float s = (float)GD_TICKS[st.v[P_GD_TIME] % 12u] * 2.5f / bpm;
    while (s > GLINT_DELAY_MAX)
        s *= 0.5f;
    if (fm_fabsf(s - delay_s_now) > delay_s_now * 0.003f) {
        delay_s_now = s;
        st.engine_dirty = 1;
    }
#if FRS_LEAD
    {                                    /* LEAD's (card #144): GLINT's TIME, its own; an LFO on it a division at a time
                                          * (the delay glides there) */
        int32_t t = (int32_t)((float)st.v[P_LD_DTIME] + mod_of(P_LD_DTIME) + 0.5f);
        s = (float)GD_TICKS[clamp(t, 0, 11)] * 2.5f / bpm;
        while (s > LEAD_DELAY_MAX)
            s *= 0.5f;
        if (fm_fabsf(s - ld_delay_s) > ld_delay_s * 0.003f) {
            ld_delay_s = s;
            st.engine_dirty = 1;
        }
    }
#endif
}

/* PUMP on the arpeggiator's grid (MIDI clock, else TEMPO): where the beat is in RATE's period, and its pace */
static float grid_now(void);

static void pump_sync(void)
{
    uint32_t r = st.v[P_PUMP_RATE] % 7u;
    float per, ph, ms_tick;
    if (!r) {
        frasil_pump(0.0f, 0.0f);
        return;
    }
    ms_tick = grid_ms_tick(eng_ms());
    per = (float)PUMP_TICKS[r];
    ph = grid_now() / per;
    frasil_pump(ph - fm_floorf(ph), (1000.0f / FRASIL_FS) / (ms_tick * per));
}

/* the grid now: MIDI clocks since the start (grid.tick, from the clock or from TEMPO); clocked, the grid moves in whole
 * clocks, and between them this is how far into the next one */
static float grid_now(void)
{
    float g = grid.tick;
    if (eng_ms() - grid.clk_at < 500u && grid.clk_ms > 1.0f) {
        float f = (float)(eng_ms() - grid.clk_at) / grid.clk_ms;
        g += f > 1.0f ? 1.0f : f;
    }
    return g;
}

/* ------------------------------------------------------------------ the LFOs --- */
/* LFO 1, 2 and 3, free running while on (DEPTH over 0 and a TARGET set): at RATE's own pace, or on the grid (MIDI clock
 * or TEMPO; MIDI Start puts them back to the top of their turn). SHAPE's value, as POLARITY has it, times DEPTH times a
 * setting's whole range, goes onto each TARGET (mod: params_to_engine adds it), a hundred times a second; the engine
 * glides between. RANDOM and DRIFT take a new value each turn (the same for a turn number, from a hash) */
static struct {
    float ph;                            /* where in its turn, 0..1 */
    uint32_t cyc;                        /* the turns gone by */
    float out;                           /* SHAPE's value now, -1..1 (the screen) */
    float peak;                          /* the largest offset since the tests asked (0..1) */
    uint8_t on;
} lfo[NLFO];
static uint32_t lfo_at;
static float lfo_grid;                   /* the grid last time */
static uint8_t lfo_any, lfo_restart;

static float lfo_rnd(uint32_t i, uint32_t c)   /* RANDOM's value for turn c of LFO i, -1..1 */
{
    uint32_t h = (c + 1u) * 2654435761u ^ (i + 1u) * 0x9E3779B9u;
    h ^= h >> 15;
    h *= 0x85EBCA77u;
    h ^= h >> 13;
    return (float)(h >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

static float lfo_shape(uint32_t s, float x, uint32_t i, uint32_t c)   /* SHAPE s at x (0..1) of turn c, -1..1 */
{
    switch (s) {
    case 0:
        return fm_sinf(6.2831853f * x);
    case 1:                              /* TRIANGLE: up from 0, down, back */
        return x < 0.25f ? 4.0f * x : x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f;
    case 2:                              /* SAW: up */
        return 2.0f * x - 1.0f;
    case 3:                              /* RAMP: down */
        return 1.0f - 2.0f * x;
    case 4:
        return x < 0.5f ? 1.0f : -1.0f;
    case 5:                              /* RANDOM: a value a turn */
        return lfo_rnd(i, c);
    default: {                           /* DRIFT: from this turn's value to the next, eased */
        float a = lfo_rnd(i, c), b = lfo_rnd(i, c + 1u);
        return a + (b - a) * x * x * (3.0f - 2.0f * x);
    }
    }
}

static void lfo_run(void)
{
    uint32_t now = eng_ms(), i, k;
    float dt, g, dg;
    int any = 0;
    if (now - lfo_at < 10u)
        return;
    dt = (float)(now - lfo_at) * 0.001f;
    lfo_at = now;
    if (dt > 0.1f)
        dt = 0.1f;
    g = grid_now();
    dg = g - lfo_grid;
    if (dg < -4800.0f)                   /* (the grid wraps at 9600) */
        dg += 9600.0f;
    else if (dg < 0.0f)
        dg = 0.0f;
    lfo_grid = g;
    memset(mod, 0, sizeof mod);
    memset(mod_tp, 0, sizeof mod_tp);
#if FRS_LEAD
    memset(mod_ld, 0, sizeof mod_ld);
#endif
    for (i = 0; i < NLFO; i++) {
        const uint8_t *L = &st.v[LFO_BASE[i]];
        float depth = (float)L[2] * (1.0f / 127.0f), w, off;
        uint32_t r = L[1], used = 0, n;
        for (k = 0; k < 4u; k++)
            used |= L[4u + k];
        lfo[i].on = depth > 0.0f && used;
        if (!lfo[i].on)
            continue;
        any = 1;
        if (lfo_restart && r >= LFO_NFREE) {
            lfo[i].ph = 0.0f;
            lfo[i].cyc = 0;
        }
        lfo[i].ph += r < LFO_NFREE ? dt * tape_tk * lfo_hz(r) : dg / (float)LFO_TICKS[(r - LFO_NFREE) % 12u];   /* (TAPE
                                                                             * SPEED's pace: free, its own; synced, the grid's) */
        n = (uint32_t)lfo[i].ph;
        lfo[i].ph -= (float)n;
        lfo[i].cyc += n;
        w = lfo_shape(L[0] % 7u, lfo[i].ph, i, lfo[i].cyc);
        lfo[i].out = w;
        off = depth * (L[3] == 1u ? 0.5f * (w + 1.0f) : L[3] == 2u ? -0.5f * (w + 1.0f) : w);   /* BIPOLAR, UP, DOWN */
        if (fm_fabsf(off) > lfo[i].peak)
            lfo[i].peak = fm_fabsf(off);
        for (k = 0; k < 4u; k++)
            if (L[4u + k]) {             /* each TARGET times its AMOUNT (x0.00 .. x1.00 .. x5.00: at its middle, 1
                                          * exactly, the LFOs as they were; the setting stays in its range, u7) */
                uint32_t p = LFO_TGT[L[4u + k] % NLFO_TGT], v = st.v[P_L1_AMT1 + 4u * i + k];
                float a = (float)(v <= 100u ? v : 100u + 4u * (v - 100u)) / 100.0f, *m = mod_at(p);
                if (m)                       /* (LOFI TAPE II's four past the sound's: mod_tp) */
                    *m += off * a * (float)PD[p].max;
            }
    }
    lfo_restart = 0;
    if (any || lfo_any)                  /* (once more after the last goes off: back to the settings as they are) */
        st.engine_dirty = 1;
    lfo_any = (uint8_t)any;
}

/* CHANCE as it is now: with what the LFOs add to it (it is the arpeggiator's, not the engine's) */
static uint32_t arp_chance(void)
{
    float c = (float)st.v[P_ARP_CHANCE] + mod[P_ARP_CHANCE];
    return c <= 0.0f ? 0u : c >= 127.0f ? 127u : (uint32_t)(c + 0.5f);
}

/* SEQ LENGTH as it plays now, 1 .. SC_SEQ: with what the LFOs add to it */
static uint32_t seq_len(void)
{
    float c = (float)st.v[P_SQ_LEN] + mod[P_SQ_LEN];
    return c <= 0.0f ? 1u : c >= (float)(SC_SEQ - 1u) ? SC_SEQ : (uint32_t)(c + 0.5f) + 1u;
}

/* a step's note as it plays: TRANSPOSE's steps along the scale (OCTAVE on it in the engine, as on every glint) */
static int seq_note(uint32_t b)
{
    int n = scale_shift((int)(b & 127u), (int)st.v[P_SQ_TRANS] - 14, st.v);
    return n > 127 ? 127 : n;
}

/* the sequencer on the grid: its note off at GATE's end (a TIE on the next step holds it on), the next step's on */
static void seq_run(uint32_t now, int32_t stepn, uint32_t period)   /* stepn: its own (seq_stepn) */
{
    uint32_t len = seq_len(), b, nb;
    int32_t pos, was = sq.n;
    uint32_t gate = (uint32_t)((float)period * (0.05f + 0.95f * (float)st.v[P_SQ_GATE] * (1.0f / 127.0f)));
    if (sq.sounding >= 0 && (int32_t)(now - sq.off_ms) >= 0)
        seq_note_off();
    sq.n = stepn;
    if (sq.restart) {                    /* MIDI Start: its first step on the grid's top */
        sq.restart = 0;
        sq.run = 1;
        sq.start = stepn;
        sq.pos = -1;
        if (!sq.near)
            sq.skip = -1;
        sq.near = 0;
        was = stepn - 1;
        take_new(&gtk);
    }
    if (!sq.run || stepn == was)
        return;
    pos = (stepn - sq.start) % (int32_t)len;
    if (pos < 0)
        pos += (int32_t)len;
    sq.pos = pos;
    take_run(&gtk, st.v[P_SQ_REC] == 2u && st.v[P_SQ_TYPE] == SQT_SEQ, len);
    if (take_reach(&gtk, seq, (uint32_t)pos, SQ_REST)) {   /* (OVERDUB off, the take's first turn: what was there goes
                                                         * as the playhead comes to it) */
        seq_view();
        st.dirty = (uint8_t)sound_changed();
    }
    b = seq[pos];
    nb = seq[(uint32_t)(pos + 1) % len] & 127u;
    if ((b & 127u) == SQ_TIE) {          /* held on through this step */
        if (sq.sounding >= 0)
            sq.off_ms = now + (nb == SQ_TIE ? 2u * period : gate);
        return;
    }
    seq_note_off();
    if (pos == sq.skip) {                /* LIVE wrote it a moment ago, and it sounded then */
        sq.skip = -1;
        return;
    }
    if ((b & 127u) == SQ_REST || arp_rand() % 127u >= arp_chance())
        return;
    {
        int n = seq_note(b);
        float v = (b & SQ_ACC ? 1.9f : 0.68f) * (0.92f + 0.08f * (float)(arp_rand() & 255u) * (1.0f / 255.0f));
        frasil_glint_on(n, v);
        sq.sounding = (int16_t)n;
        sq.off_ms = now + (nb == SQ_TIE ? 2u * period : gate);
        sq.steps++;
    }
}

/* a GLINT key or note while the sequencer has them (seq_keys): RECORD OFF, the sequence transposed from its first note
 * to this one along the scale and, playing, begun again from its first step on the nearest beat (seq_due); STEP,
 * written at STEP, the LENGTH carried on (sq.len), then on past it (it sounds while down); LIVE, written on the step nearest the playhead (it
 * sounds as it is played; ahead of the playhead, that step skips this time round) */
static void seq_key(int n, int sound, uint32_t vel)   /* n: without GLINT's OCTAVE (kept so); sound: with it */
{
    uint32_t len = st.v[P_SQ_LEN] + 1u, i;
    int w = st.v[P_SQ_REC] ? scale_shift(n, 14 - (int)st.v[P_SQ_TRANS], st.v) : n;   /* (RECORDing: written so that,
                                                                                    * TRANSPOSE on it, it plays as played) */
    uint8_t b = (uint8_t)((w < 1 ? 1 : w > 126 ? 126 : w) | (vel >= 100u ? SQ_ACC : 0u));
    if (st.v[P_SQ_REC] == 1u) {
        uint32_t c = st.v[P_SQ_STEP] % len, l = step_stays ? 1u : sq.len ? sq.len : 1u;
        l = l < len - c ? l : len - c;
        if (l > 1u)
            step_lay(seq, len, c, l, b);
        else
            seq[c] = b;
        if (!step_stays)
            st.v[P_SQ_STEP] = (uint8_t)((c + l) % len);
        rec_was.wrote = 1;
        seq_view();
        frasil_glint_on(sound, (float)vel * (1.0f / 127.0f));
    } else if (st.v[P_SQ_REC] == 2u) {   /* (stopped: it sounds, nothing to put it on, as the bass's take) */
        float g = seq_grid(grid_now()) / (float)ARP_TICKS[st.v[P_ARP_RATE] % 10u] - (float)sq.start, f;
        int32_t q;
        int top = count_near() || (step_hand && sq.restart);   /* (on the downbeat: its first step, sounded now) */
        len = seq_len();                 /* (as it plays now) */
        f = g - fm_floorf(g);
        q = top ? 0 : (int32_t)fm_floorf(g) + (f >= 0.5f);
        q %= (int32_t)len;
        if (q < 0)
            q += (int32_t)len;
        if (sq.run || top) {
            take_run(&gtk, 1, len);
            seq[q] = b;
            rec_was.wrote = 1;
            gtk.wrote |= 1u << (q & 31);   /* (the take's own: its first turn replaces it no more) */
            if (sq.restart && top) {
                sq.skip = 0;
                sq.near = 1;
            } else if (top) {
                cnt.glint = 1;
            } else if (f >= 0.5f) {
                sq.skip = q;
            }
            seq_view();
        }
        frasil_glint_on(sound, (float)vel * (1.0f / 127.0f));
    } else {
        uint8_t t = st.v[P_SQ_TRANS];
        for (i = 0; i < len; i++)        /* from the first note (not a rest, not a tie) */
            if ((seq[i] & 127u) != SQ_REST && (seq[i] & 127u) != SQ_TIE)
                break;
        if (i < len)
            t = (uint8_t)clamp(14 + scale_steps(seq[i] & 127, n, st.v), 0, 28);
        if (sq.run) {                    /* playing: again from its first step on the nearest beat, TRANSPOSE with
                                          * it (seq_due): the one just gone, at once; in a beat's second half, the
                                          * next (what plays till then as it was) */
            float g = grid_now(), at = 24.0f * fm_floorf(g / 24.0f);
            if (g - at >= 12.0f)
                at += 24.0f;
            sq.at = at >= 9600.0f ? at - 9600.0f : at;
            sq.at_trans = t;
        } else {
            st.v[P_SQ_TRANS] = t;
        }
    }
    st.dirty = (uint8_t)sound_changed();
}

/* a key's restart (seq_key), the grid on its beat: the sequence from its first step there, its steps on that beat
 * from then on (sq.off), the one it is on now at once (seq_run: its NOTE sounds, as late as the key came; a TIE or a
 * REST, none), TRANSPOSE from then; already playing so, nothing again. Recording, or the sequence stopped meanwhile:
 * none */
static void seq_due(uint32_t ticks)
{
    float d = grid.tick - sq.at;
    uint8_t off = sq.off;
    int32_t start = sq.start;
    if (sq.at < 0.0f)
        return;
    if (!sq.run || st.v[P_SQ_REC]) {
        sq.at = -1.0f;
        return;
    }
    if (d < -4800.0f)                    /* (the grid wraps at 9600) */
        d += 9600.0f;
    else if (d > 4800.0f)
        d -= 9600.0f;
    if (d < 0.0f)
        return;
    sq.off = (uint8_t)((uint32_t)sq.at % ticks);
    sq.start = seq_stepn(sq.at, ticks);
    sq.at = -1.0f;
    if (sq.off == off && (sq.start - start) % (int32_t)seq_len() == 0 && st.v[P_SQ_TRANS] == sq.at_trans &&
        sq.pos >= 0)
        return;                          /* (its steps fall there already, as transposed: no note twice) */
    sq.n = seq_stepn(grid.tick, ticks) - 1;
    sq.pos = sq.skip = -1;
    seq_note_off();
    if (st.v[P_SQ_TRANS] != sq.at_trans) {
        st.v[P_SQ_TRANS] = sq.at_trans;
        st.dirty = (uint8_t)sound_changed();
    }
}

/* what the keys ask for plays from here, its pattern from the top (a set let go to nothing after one that played: a gap,
 * ARP SNAP's next set a change, not a start) */
static void arp_set_take(arp_t *a)
{
    uint32_t i;
    a->dirty = 0;
    if (!a->n && a->pn)
        a->gap = 1;
    for (i = 0; i < a->n; i++) {
        a->play[i] = a->held[i];
        a->pprs[i] = a->prs[i];
    }
    a->pn = a->n;
    a->idx = -1;
    a->dir = 1;
    a->fresh = a->owe = 0;
    ARP_TRACE(a, "take", (int)a->pn);
}

static void arp_advance(arp_t *a, uint32_t mode, int32_t count)
{
    switch (mode) {
    case 0:                              /* UP */
        a->idx = (a->idx + 1) % count;
        break;
    case 1:                              /* DOWN */
        a->idx = a->idx <= 0 ? count - 1 : a->idx - 1;
        break;
    case 2:                              /* UPDOWN */
        a->idx += a->dir;
        if (a->idx >= count) {
            a->idx = count > 1 ? count - 2 : 0;
            a->dir = -1;
        } else if (a->idx < 0) {
            a->idx = count > 1 ? 1 : 0;
            a->dir = 1;
        }
        break;
    case 3:                              /* RANDOM */
        a->idx = (int32_t)(a_rand(a) % (uint32_t)count);
        break;
    default:                             /* DRIFT: a step either way, or stay */
        a->idx = a->idx < 0 ? 0 : a->idx + (int32_t)(a_rand(a) % 3u) - 1;
        if (a->idx < 0)
            a->idx = count > 1 ? 1 : 0;
        if (a->idx >= count)
            a->idx = count > 1 ? count - 2 : 0;
        break;
    }
}

/* a step: the pattern on (skip steps first, silent: a set taken late, as if on its point), its note struck as CHANCE
 * says; keep: the note it comes to sounding already (a set taken late over the one that played there), left ringing */
static void arp_step(arp_t *a, uint32_t mode, uint32_t range, uint32_t chance, uint32_t now, uint32_t period, int32_t skip,
                     int keep)
{
    int32_t count;
    int at, note;
    if (!a->pn) {
        arp_stop(a);
        return;
    }
    count = (int32_t)a->pn * (int32_t)(range + 1u);
    while (skip-- > 0)
        arp_advance(a, mode, count);
    arp_advance(a, mode, count);
    at = a->idx % (int32_t)a->pn;
    note = a->play[at] + 12 * (a->idx / (int32_t)a->pn);   /* (OCTAVE: the engine's) */
    if (note > 127)
        note = 127;
    if (keep && a->sounding == note) {
        a->src = a->play[at];
        return;
    }
    arp_stop(a);
    if (a_rand(a) % 127u < chance) {
        float v = (float)a->vel * (1.0f / 127.0f) * (0.8f + 0.2f * (float)(a_rand(a) & 255u) * (1.0f / 255.0f));
        if (a == &arp) {
            frasil_glint_on(note, v);
            if (a->pprs[at] > 0.0f)      /* its key pressed: the step louder */
                frasil_glint_pressure(note, a->pprs[at]);
        } else {
            bass_play(note, (uint32_t)(v * 127.0f));
            if (a->pprs[at] > 0.0f)
                frasil_bass_pressure(a->pprs[at]);
        }
        a->src = a->play[at];
        ARP_TRACE(a, "step", note);
        a->sounding = (int16_t)note;
        a->off_ms = now + period / 2u;
        a->steps++;
    }
}

/* a set taken now as if on its point (b_step): the pattern where it would be at this step */
static void arp_late(arp_t *a, uint32_t mode, uint32_t range, uint32_t chance, int32_t stepn, uint32_t now,
                     uint32_t period, int keep)
{
    int32_t skip = stepn - a->b_step;
    if (skip < 0 || skip > 64)
        skip = 0;
    arp_set_take(a);
    arp_step(a, mode, range, chance, now, period, skip, keep);
}

/* a layer's arpeggio at this pass: way, its PLAYS; stepn its step of the grid (a new one, a step); period its length;
 * step_ticks a step's MIDI clocks, snap ARP SNAP's (0: OFF), ms_tick a clock's milliseconds */
static void arp_turn(arp_t *a, uint32_t way, uint32_t mode, uint32_t range, uint32_t chance, int32_t stepn, uint32_t now,
                     uint32_t period, uint32_t step_ticks, uint32_t snap, float ms_tick)
{
    int snapped, fresh;
    if (a->sounding >= 0 && (int32_t)(now - a->off_ms) >= 0)
        arp_stop(a);
    if (way != SQT_ARP || a->paused) {   /* off ARP, or waiting: silent, its notes kept (ARP again: they play on) */
        arp_stop(a);
        if (way != SQT_ARP) {
            a->pn = 0;
            a->dirty = 0;
            if (!way && (a != &arp || !a->n))
                a->paused = 0;
        }
        a->fresh = 1;                    /* (ARP SNAP: back, it starts on its next step) */
        a->owe = a->gap = 0;
        a->step_n = stepn;
        return;
    }
    if (!snap) {                         /* ARP SNAP OFF: what the keys ask for from the next step, as before */
        if (stepn == a->step_n)          /* between two steps of the grid: nothing changes */
            return;
        a->step_n = stepn;
        if (a->dirty)                    /* on the step: what the keys ask for plays from here, its pattern from the top */
            arp_set_take(a);
        a->fresh = a->owe = a->gap = 0;
        arp_step(a, mode, range, chance, now, period, 0, 0);
        return;
    }
    if (stepn == a->step_n) {            /* between two steps */
        if (a->dirty && !a->owe && now - a->b_ms <= a->b_late)
            a->owe = 2;                  /* a set just after its point: it lands there, the old note ringing meanwhile */
        if (a->owe && (now - a->chg_ms >= ARP_GATHER_MS || now - a->b_ms > (uint32_t)a->b_late + ARP_GATHER_MS))
            arp_late(a, mode, range, chance, stepn, now, period, a->owe == 2u);   /* (the hand in, or long enough) */
        return;
    }
    a->step_n = stepn;
    snapped = ((uint32_t)stepn * step_ticks) % snap == 0u;
    if (snapped && !a->pn)
        a->gap = 0;                      /* (a snap point with nothing: from silence again) */
    fresh = a->fresh || (!a->pn && !a->gap);
    if (a->owe && !snapped && !fresh) {  /* the hand still coming at the next step: taken here, as on its point */
        arp_late(a, mode, range, chance, stepn, now, period, 0);
        return;
    }
    if (snapped || fresh) {              /* a point a set is taken on (from silence: any step) */
        float late = (float)(snapped && !fresh ? snap : step_ticks) * ms_tick * 0.5f;
        a->b_ms = now;
        a->b_step = stepn;
        a->b_late = (uint16_t)(late < (float)ARP_LATE_MS ? late : (float)ARP_LATE_MS);
        if (a->dirty || a->owe) {
            if (now - a->chg_ms < ARP_GATHER_MS) {   /* the hand still coming: its step held back a moment */
                arp_stop(a);
                a->owe = 1;
                return;
            }
            arp_set_take(a);
        }
    } else if (a->dirty && !a->n) {      /* everything let go: silent from this step, as before */
        arp_set_take(a);
    }
    a->fresh = 0;
    arp_step(a, mode, range, chance, now, period, 0, 0);
}

/* GLINT on KEYS: its arpeggio's last RING notes ring, unless it waits (as its steps would) */
static void ring_run(void)
{
    int8_t want[RING];
    uint32_t n = 0, i, j;
    if (!st.v[P_SQ_TYPE] && !arp.paused)
        for (i = 0; i < arp.n; i++) {
            for (j = n; j && (int16_t)(arp.in[i] - arp.in[(uint8_t)want[j - 1u]]) > 0; j--)
                if (j < RING)
                    want[j] = want[j - 1u];
            if (j < RING) {
                want[j] = (int8_t)i;
                n += n < RING;
            }
        }
    for (i = 0; i < n; i++)
        want[i] = arp.held[(uint8_t)want[i]];
    for (i = 0; i < rung.n;) {           /* those let go (a key's or a finger's on the same note sound on) */
        int b = rung.note[i];
        for (j = 0; j < n && want[j] != b; j++)
            ;
        if (j < n) {
            i++;
            continue;
        }
        rung.note[i] = rung.note[--rung.n];
        if (!glint_taken(b))
            frasil_glint_off(b);
    }
    for (i = 0; i < n; i++)              /* those come */
        if (!ring_has(want[i]) && rung.n < RING) {
            rung.note[rung.n++] = want[i];
            frasil_glint_on(want[i], (float)arp.vel * (1.0f / 127.0f));
        }
}

static void arp_run(void)
{
    uint32_t now = eng_ms(), ticks = ARP_TICKS[st.v[P_ARP_RATE] % 10u], bticks = bseq_ticks(), dt = now - grid.last_ms;
    float ms_tick = grid_ms_tick(now);
    uint32_t period = (uint32_t)((float)ticks * ms_tick);   /* a step's length, for the gate below */
    uint32_t snap = ARP_SNAP_TICKS[st.v[P_ARP_SNAP] % 6u];   /* ARP SNAP's grid (0: OFF) */
    grid.last_ms = now;
    /* the grid moves on: by the clocks that came in, else by the time at TEMPO */
    if (now - grid.clk_at < 500u)
        grid.tick += (float)grid.clk_new;
    else
        grid.tick += (float)(dt > 1000u ? 1000u : dt) / ms_tick;
    grid.clk_new = 0;
    if (grid.tick >= 9600.0f) {
        grid.tick -= 9600.0f;
        grid.wraps++;
    }
    arp_turn(&arp, st.v[P_SQ_TYPE], st.v[P_ARP_MODE], st.v[P_ARP_RANGE], arp_chance(), (int32_t)(grid.tick / (float)ticks),
             now, period, ticks, snap, ms_tick);
    ring_run();
    if (st.v[P_SQ_TYPE] == SQT_SEQ) {    /* the sequencer instead: the arpeggio silent, its notes kept */
        seq_due(ticks);
        seq_run(now, seq_stepn(grid.tick, ticks), period);
    }
    arp_turn(&barp, st.v[P_BQ_ON], st.v[P_BA_MODE], st.v[P_BA_RANGE], st.v[P_BA_CHANCE],
             (int32_t)((grid.wraps * 9600u + (uint32_t)grid.tick) / bticks), now, (uint32_t)((float)bticks * ms_tick), bticks,
             snap, ms_tick);
}

/* REC's metronome, while a sequence records LIVE (GLINT's, else the bass's): a tick on each of its beats (24 MIDI
 * clocks) counted from its first step, the higher one as it comes round to that step (a bass take's loop still open,
 * growing: on each bar line too); a turn that is not a whole number of beats, its beats from its top again. Begun on
 * a sequence playing, from its next beat. On STEP, or the sequence stopped, none. Its beats are REC's flash's too
 * (mtr.at) */
static struct {
    float at;                            /* the clocks into the sequence's beat now, -1: none */
    float t;                             /* the clocks into its turn at the last run, -1: it is about to begin (its
                                          * first step ticks), -2: none (as it comes, its beat ticks only if it began
                                          * since the last run) */
    float g;                             /* the grid at the last run */
    uint8_t src;                         /* the sequence it follows: 1 GLINT's, 2 the bass's, 3 the drone's, 0 none */
    float ck;                            /* the grid at its last tick (the tests) */
} mtr = {-1.0f, -2.0f, 0.0f, 0, 0.0f};

/* (the metronome of its own, metro_free: above) */
static int32_t metro_beat = -1;
static uint32_t metro_n;                 /* its ticks (the tests) */

static void metro_run(void)
{
    float t = -1.0f, dg = grid.tick - mtr.g;
    uint32_t src = 0, k, tick, hi;
    int32_t b;
    if (dg < -4800.0f)                   /* (the grid wraps at 9600; MIDI Start puts it back to 0) */
        dg += 9600.0f;
    else if (dg < 0.0f)
        dg = 0.0f;
    mtr.g = grid.tick;
    if (!st.v[P_SQ_REC] && !st.v[P_BQ_REC] && !st.v[P_DQ_REC])
        st.clicks = st.hiclicks = 0;
    if (st.v[P_SQ_REC] == 2u && sq.run) {   /* its step, and how far into it on the grid */
        src = 1;
        k = ARP_TICKS[st.v[P_ARP_RATE] % 10u];
        if (sq.pos >= 0 && !sq.restart)
            t = (float)((uint32_t)sq.pos * k) + seq_grid(grid.tick) - (float)sq.n * (float)k;
    } else if (st.v[P_BQ_REC] == 2u && bq.run) {   /* the steps before it, and how far into it */
        src = 2;
        if (bseq_playing())
            t = bseq_at();
    } else if (st.v[P_DQ_REC] == 2u && dq.run) {   /* the drone's, the same */
        src = 3;
        if (dseq_playing())
            t = dseq_at();
    } else if (st.v[P_SQ_REC] == 2u || st.v[P_BQ_REC] == 2u || st.v[P_DQ_REC] == 2u) {   /* LIVE, stopped: PLAY starts
                                                                                          * it from the top */
        src = st.v[P_SQ_REC] == 2u ? 1u : st.v[P_BQ_REC] == 2u ? 2u : 3u;
    }
    if (src != mtr.src)                  /* (another sequence, or none: from its next beat) */
        mtr.t = -2.0f;
    mtr.src = (uint8_t)src;
    if (metro_free && !src) {
        int32_t beat = (int32_t)(grid.tick * (1.0f / 24.0f));
        /* turned on (or PLAY with it on): the beat it is in ticks too if it has only just begun, 60 ms at most (PLAY from
         * the top: its one); else from the next */
        float into = grid.tick - 24.0f * (float)beat, soon = 60.0f * (float)(st.v[P_TEMPO] + 30u) * (1.0f / 2500.0f);
        if (beat != metro_beat) {
            if (metro_beat >= 0 || into < soon) {
                frasil_click(!(beat & 3));
                metro_n++;
            }
            metro_beat = beat;
        }
    } else {
        metro_beat = -1;
    }
    if (t < 0.0f) {                      /* (not begun yet, or stopped: it will, from its first step) */
        mtr.at = -1.0f;
        if (src)
            mtr.t = -1.0f;
        return;
    }
    b = (int32_t)(t * (1.0f / 24.0f));
    mtr.at = t - 24.0f * (float)b;
    if (mtr.t == -2.0f)
        tick = mtr.at < dg;
    else
        tick = mtr.t < 0.0f || t < mtr.t || b != (int32_t)(mtr.t * (1.0f / 24.0f));
    mtr.t = t;
    hi = b == 0;
    if (tick && st.metro) {
        frasil_click((int)hi);
        st.clicks++;
        st.hiclicks += hi;
        mtr.ck = grid.tick;
    }
}

/* GENERATIVE FX's SPARKLE: glints by themselves now and then, on the notes the drones hold (GLINT's OCTAVE up, one
 * in three an octave more), as often as it is turned (with what the LFOs add: a few a minute .. six a second), soft,
 * short; nothing held, nothing */
static struct {
    uint8_t armed;                       /* the next one's time set */
    uint32_t next;
    int8_t note[2];                      /* the ones sounding, -1: none (two at most: GLINT's other voices are the
                                          * player's) */
    uint32_t off[2];
    uint32_t n;                          /* played (the tests) */
} spk = {.note = {-1, -1}};

/* a glint the player sounds on note n now (a key, MIDI, the arpeggio's or the sequence's step): SPARKLE leaves it be */
static int glint_taken(int n)
{
    uint32_t k;
    if (arp.sounding == n || sq.sounding == n || bit_get(mgl, n) || ring_has(n) || bit_get(hold_k, n))
        return 1;
    for (k = 0; k < FRS_NKEYS; k++)
        if (key[k].kind == K_GLINT) {
            uint32_t i;
            for (i = 0; i < key[k].chord.k; i++)
                if (key[k].chord.n[i] == n)
                    return 1;
        }
    return 0;
}

static void sparkle_run(void)
{
    uint32_t now = eng_ms(), i, n, cnt = 0, slot = 0;
    float d = ((float)st.v[P_GX_SPARK] + mod[P_GX_SPARK]) * (1.0f / 127.0f), mean;
    for (i = 0; i < 2u; i++)
        if (spk.note[i] >= 0 && (int32_t)(now - spk.off[i]) >= 0) {
            if (!glint_taken(spk.note[i]))   /* (taken since: the player's now) */
                frasil_glint_off(spk.note[i]);
            spk.note[i] = -1;
        }
    if (d <= 0.004f) {
        spk.armed = 0;
        return;
    }
    mean = 1000.0f / (6.0f * (d > 1.0f ? 1.0f : d * d) + 0.02f);   /* ms between two, on average */
    if (!spk.armed || (int32_t)(spk.next - now) > (int32_t)(mean * 1.7f)) {   /* just on, or turned up: the next one
                                                                            * from now (not at once) */
        spk.armed = 1;
        spk.next = now + (uint32_t)(mean * (0.3f + 1.4f * (float)(arp_rand() & 1023u) * (1.0f / 1023.0f)));
        return;
    }
    if ((int32_t)(now - spk.next) < 0)
        return;
    spk.next = now + (uint32_t)(mean * (0.3f + 1.4f * (float)(arp_rand() & 1023u) * (1.0f / 1023.0f)));
    for (n = 0; n < 128u; n++)
        cnt += held_by[n] != 0;
    if (!cnt)
        return;
    cnt = arp_rand() % cnt;
    for (n = 0; n < 128u; n++)
        if (held_by[n] && cnt-- == 0)
            break;
    {
        int note = (int)n + (arp_rand() % 3u == 0 ? 12 : 0);   /* (OCTAVE on it in the engine) */
        while (note + 12 * (int)st.v[P_GL_OCTAVE] > 120)
            note -= 12;
        if (glint_taken(note))
            return;
        for (i = 0; i < 2u; i++)         /* a free place, else the first goes */
            if (spk.note[i] < 0) {
                slot = i;
                break;
            }
        if (spk.note[slot] >= 0 && !glint_taken(spk.note[slot]))
            frasil_glint_off(spk.note[slot]);
        frasil_glint_on(note, 0.22f + 0.4f * (float)(arp_rand() & 255u) * (1.0f / 255.0f));
        spk.note[slot] = (int8_t)note;
        spk.off[slot] = now + 80u + arp_rand() % 140u;
        spk.n++;
    }
}

/* ----------------------------------------------------------------- MIDI --- */
static const struct {
    uint8_t cc, p;
} CC_MAP[] = {
    {1, P_MOUVEMENT}, {74, P_TIMBRE}, {91, P_ESPACE}, {70, P_CHOEUR}, {71, P_PARTIELS}, {72, P_SOUFFLE},
    {73, P_ATTAQUE}, {75, P_RELACHE}, {76, P_TAILLE}, {77, P_DUREE}, {78, P_SHIMMER}, {79, P_CHALEUR},
    {12, P_DR_FULL}, {13, P_DR_PRESET}, {14, P_GX_SPARK}, {15, P_GX_BLUR}, {16, P_GX_SINK}, {17, P_MX_DRONE},
    {18, P_MX_GLINT}, {19, P_MX_BASS},   /* (the performance's: the hands, GENERATIVE FX, the faders) */
};

/* GLINT's note n (as it sounds) pressed (0..1): arpeggiated, the steps from it (kept without OCTAVE); else the glint
 * itself */
static void glint_note_press(int n, float f)
{
    if (seq_keys()) {                    /* the sequencer's: the note sounding, louder and longer */
        if (sq.sounding >= 0)
            frasil_glint_pressure(sq.sounding, f);
        else if (st.v[P_SQ_REC])
            frasil_glint_pressure(n, f);
        return;
    }
    if (st.v[P_SQ_TYPE] == SQT_ARP)
        arp_press(&arp, bit_get(mgl, n) ? mgl_base[n] : n, f);
    else
        frasil_glint_pressure(n, f);
}

/* each track on its own MIDI channel (P_CANAL, P_CH_GLINT, P_CH_BASS: OFF, 1 .. 16, OMNI), whatever the keys play.
 * GLINT's and BASS's notes play as they come (no scale: the sequencer's own), their OCTAVE on them as on a key's (the
 * engine's: into the arpeggio and the sequences they go as they come, as a key's); the drones' go through the
 * harmony, as ever */
static int on_ch(int p, uint32_t c) { return st.v[p] > 16u || (st.v[p] && st.v[p] == c); }

static void midi_glint_on(int n, uint32_t vel)
{
    uint32_t i, held = 0;
    int sk = seq_keys();
    for (i = 0; i < 4u; i++)
        held |= mgl[i];
    bit_put(mgl, n, 1);
    mgl_base[n] = (int8_t)n;             /* (as it comes: OCTAVE on it in the engine, as on a key's) */
    bit_put(mgs, n, sk ? st.v[P_SQ_REC] != 0 : st.v[P_SQ_TYPE] != SQT_ARP);
    if (sk) {                            /* into the sequencer, as a key would */
        seq_key(mgl_base[n], n, vel);
        return;
    }
    if (st.v[P_SQ_TYPE] == SQT_ARP) {    /* into the arpeggio, as a key would */
        if (arp.latch && arp_new_hand(&arp, held || keys_down(K_GLINT, FRS_NKEYS))) {
            arp.n = 0;                   /* latched, a new hand: a new set */
            arp.dirty = 1;
        }
        arp_add(&arp, mgl_base[n], vel);
        arp.hand_ms = eng_ms();
    } else {
        frasil_glint_on(n, (float)vel * (1.0f / 127.0f));
    }
}

static void midi_glint_off(int n)
{
    if (!bit_get(mgl, n))
        return;
    bit_put(mgl, n, 0);
    if (bit_get(mgs, n) && sq.sounding != n && !ring_has(n))
        frasil_glint_off(n);
    bit_put(mgs, n, 0);
    if (st.v[P_SQ_TYPE] != SQT_SEQ && !arp.latch)
        arp_remove(&arp, mgl_base[n]);
}

static void midi_glints_off(void)        /* every GLINT note held from MIDI let go */
{
    uint32_t i;
    for (i = 0; i < 128u; i++)
        midi_glint_off((int)i);
}

static void midi_bass_on(int n, uint32_t vel)   /* mono, the last note on top */
{
    uint32_t i, j;
    for (i = j = 0; i < mb_n; i++)
        if (mbass[i] != n)
            mbass[j++] = mbass[i];
    mb_n = j;
    if (mb_n == sizeof mbass) {          /* full: the oldest goes */
        for (i = 1; i < mb_n; i++)
            mbass[i - 1] = mbass[i];
        mb_n--;
    }
    mbass[mb_n++] = (int8_t)n;
    if (bseq_keys()) {                   /* into the bass's sequence, as a key would */
        bseq_key(n, n, vel);
    } else if (st.v[P_BQ_ON] == SQT_ARP) {   /* into its arpeggio, as a key would */
        if (barp.latch && arp_new_hand(&barp, mb_n > 1u || keys_down(K_BARP, FRS_NKEYS))) {
            barp.n = 0;                  /* latched, a new hand: a new set */
            barp.dirty = 1;
        }
        arp_add(&barp, n, vel);
        barp.hand_ms = eng_ms();
    } else {
        bass_live(n, vel);
    }
}

static void midi_bass_off(int n)          /* the one on top let go: back to the one under it, else silence */
{
    uint32_t i, j;
    int top = mb_n ? mbass[mb_n - 1] : -1;
    for (i = j = 0; i < mb_n; i++)
        if (mbass[i] != n)
            mbass[j++] = mbass[i];
    mb_n = j;
    if (bseq_keys()) {                   /* (the bass's sequence's) */
        bseq_up(n);
        return;
    }
    if (st.v[P_BQ_ON] != SQT_SEQ && !(barp.latch && st.v[P_BQ_ON] == SQT_ARP))
        arp_remove(&barp, n);            /* (if it was arpeggiated; latched, it stays) */
    if (st.v[P_BQ_ON] == SQT_ARP || n != top || bass_note != n)
        return;                          /* not the one sounding (or the keys have the bass now) */
    if (mb_n)
        bass_live(mbass[mb_n - 1], 100);
    else
        bass_stop();
}

static void midi_note_off(int n)
{
    if (!bit_get(mid_on, n))
        return;
    chord_off(&mid[n]);
    bit_put(mid_on, n, 0);
}

static int arp_plays(void) { return st.v[P_SQ_TYPE] == SQT_ARP || (!st.v[P_SQ_TYPE] && arp.n); }
static int holding_any(void)
{
    return lit[0] | lit[1] | lit[2] | lit[3] || bass_note >= 0 || (arp.n && arp_plays()) ||
           (barp.n && st.v[P_BQ_ON] == SQT_ARP) || sq.run || bq.run || dq.run;
}

/* the grid from its top, now: its beat and its bar begin here (MIDI Start; PLAY starting the transport, RELEASE ALL,
 * RESTORE). The LFOs on the grid from the top of their turn, the arpeggio from the top of its pattern; OCT's blink, REC's
 * flash, the metronome, PUMP and PULSE's divisions (one on the top itself) follow it. Each that follows the grid from
 * its last run starts from there (no jump round the 9600). Following a MIDI clock, only its own Start does it (start):
 * PLAY leaves the clock's beats as they are, 0 */
static int grid_top(int start)
{
    if (!start && eng_ms() - grid.clk_at < 500u)
        return 0;
    grid.clk = grid.clk_new = 0;
    grid.tick = 0.0f;
    grid.wraps = 0;
    arp.step_n = barp.step_n = -1;
    arp.idx = barp.idx = -1;
    arp.owe = barp.owe = 0;              /* (ARP SNAP: no point before the top) */
    arp.b_ms = barp.b_ms = eng_ms() - 0x10000u;
    lfo_restart = 1;
    lfo_grid = mtr.g = bq.g = dq.g = 0.0f;
    bp.g = -1.0f;
    return 1;
}

/* the sequences from their first steps at the grid's top (grid_top just done), at once: GLINT's, the bass's, the
 * drone's */
static void seqs_top(int glint, int bass, int drone)
{
    if (glint) {
        sq.restart = 1;
        sq.off = 0;
        sq.at = -1.0f;
    }
    if (bass) {
        bseq_begin();
        bq.wait = 2;
    }
    if (drone) {
        dseq_begin();
        dq.wait = 2;
    }
}

/* RELEASE ALL: PLAY held, or MIDI's All Sound Off (CC 120, on any channel: the River sends it before it sends a shard
 * or a backup, so that the FM-1 plays nothing when the restore comes). Everything let go (the drones, the bass,
 * GLINT's notes, the arpeggio, the sequences), what was held kept for RESTORE (the sequences alone running: what was
 * kept before stays) */
static void count_stop(void);            /* (COUNT-IN, below) */
static void sound_off(void)
{
    count_stop();                        /* (a count under way: no more) */
    if (lit[0] | lit[1] | lit[2] | lit[3] || arp.n || barp.n ||
        (bass_note >= 0 && !bass_holds() && st.v[P_BQ_ON] != SQT_ARP && !bseq_owns())) {
        scene_notes_take(&rst);          /* (notes a scene keeps: a sequence's own are not) */
        rst_valid = 1;
    }
    release_all();
    arp.n = arp.pn = arp.gap = 0;
    arp.dirty = 0;
    arp.paused = 0;
    barp.n = barp.pn = barp.gap = 0;
    barp.dirty = 0;
    barp.paused = 0;
    seq_note_off();
    sq.run = 0;
    bseq_stop();
    dseq_stop();
}

#if FRS_LEAD
/* one track's notes let go as RELEASE ALL lets them go (sound_off), that track's alone (the SMK-37's track pad held,
 * admin #634), each fading as its RELEASE has it: DRONE its notes (latched, held, its sequence's) and its sequence
 * stopped; GLINT its notes (the keys', MIDI's, the ring's), its arpeggio emptied, its sequence stopped; BASS the same;
 * the LEAD its voices. RESTORE's notes not taken (PLAY held's alone) */
static void track_off(uint32_t t)
{
    uint32_t k;
    if (t == 0u) {
        release_drones();
        dseq_stop();
    } else if (t == 1u) {
        midi_glints_off();
        frasil_glint_off(-1);
        rung.n = 0;
        arp.n = arp.pn = arp.gap = 0;
        arp.dirty = 0;
        arp.paused = 0;
        seq_note_off();
        sq.run = 0;
    } else if (t == 2u) {
        bass_stop();
        mb_n = 0;
        for (k = 0; k < FRS_NKEYS; k++)
            if (key[k].kind == K_BASS || key[k].kind == K_BSEQ)
                key[k].kind = K_NONE;
        barp.n = barp.pn = barp.gap = 0;
        barp.dirty = 0;
        barp.paused = 0;
        bseq_stop();
    } else {
        frasil_lead_off();
    }
}
#endif

/* a MIDI message (pkt as the platforms pack one: its status byte in bits 8 .. 15, its data in 16 .. 23 and 24 .. 31):
 * the clock, Start, All Sound Off on any channel; on each track's channel its notes, their pressure, the channel's, the
 * CCs (the drones': sustain, All Notes Off, the sound's: the runner's first, eng_cc, then CC_MAP), Program Change (the
 * runner's: eng_program) */
static void key_notes(uint32_t layer, touch_t t, int played, int single, chord_t *c, int *root);   /* (the keys, below) */
static void eng_midi(uint32_t pkt)
{
    uint32_t i;
    int root, dr, gl, bs;
    {
        uint32_t st8 = (pkt >> 8) & 0xFFu, ch = st8 & 15u, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        uint32_t kind = st8 & 0xF0u;
        if ((st8 == 0xF8u || st8 == 0xFAu) && st.v[P_CLOCK])   /* CLOCK INTERNAL: clocks and Start not taken */
            return;
        if (st8 == 0xF8u) {              /* clock: the arpeggiator follows it */
            clock_in(eng_ms());
            grid.clk++;
            grid.clk_new++;
            return;
        }
        if (st8 == 0xFAu) {              /* start: the grid from its top, the sequences (each layer's PLAYS on SEQ) from
                                          * their first steps there */
            grid_top(1);
            seqs_top(st.v[P_SQ_TYPE] == SQT_SEQ, st.v[P_BQ_ON] == SQT_SEQ, st.v[P_DQ_ON] != 0);
            return;
        }
        if (st8 < 0x80u || st8 >= 0xF0u)
            return;
        if (kind == 0xB0u && d1 == 120u) {           /* all sound off: everything, whatever the channel */
            sound_off();
            return;
        }
        dr = on_ch(P_CANAL, ch + 1u);
        gl = on_ch(P_CH_GLINT, ch + 1u);
        bs = on_ch(P_CH_BASS, ch + 1u);
        if (!dr && !gl && !bs)
            return;
        if (kind == 0x90u && d2) {                   /* a note */
            if (dr) {                                /* the drones: its chord, held until its note-off */
                midi_note_off((int)d1);
                bit_put(sus, (int)d1, 0);
                key_notes(0, touch_set(0), (int)d1, 0, &mid[d1], &root);   /* (HARMONY, NOTES: the drones' KEYS) */
                chord_on(&mid[d1], d2);
                bit_put(mid_on, (int)d1, 1);
                dseq_key(mid[d1].n, mid[d1].k);   /* (RECORD on the drone's sequence: into it) */
            }
            if (gl)
                midi_glint_on((int)d1, d2);
            if (bs)
                midi_bass_on((int)d1, d2);
        } else if (kind == 0x80u || kind == 0x90u) {
            if (dr) {
                if (st.sustain)
                    bit_put(sus, (int)d1, 1);
                else
                    midi_note_off((int)d1);
            }
            if (gl)
                midi_glint_off((int)d1);
            if (bs)
                midi_bass_off((int)d1);
        } else if (kind == 0xA0u) {                  /* a note's pressure */
            float f = (float)d2 * (1.0f / 127.0f);
            if (dr && bit_get(mid_on, (int)d1))
                chord_press(&mid[d1], f);
            if (gl && bit_get(mgl, (int)d1))
                glint_note_press((int)d1, f);
            if (bs && mb_n && st.v[P_BQ_ON] == SQT_ARP)
                arp_press(&barp, (int)d1, f);
            else if (bs && mb_n && mbass[mb_n - 1] == (int8_t)d1 && bass_note == (int)d1)
                frasil_bass_pressure(f);
        } else if (kind == 0xD0u) {                  /* the channel's: every note of the track */
            float f = (float)d1 * (1.0f / 127.0f);
            if (dr)
                st.swell_midi = f;
            if (gl)
                for (i = 0; i < 128u; i++)
                    if (bit_get(mgl, (int)i))
                        glint_note_press((int)i, f);
            if (bs && st.v[P_BQ_ON] == SQT_ARP) {
                for (i = 0; i < mb_n; i++)
                    arp_press(&barp, mbass[i], f);
            } else if (bs && mb_n && bass_note == mbass[mb_n - 1]) {
                frasil_bass_pressure(f);
            }
        } else if (kind == 0xB0u) {
            if (d1 == 64u && !dr) {
                /* sustain: the drones' alone */
            } else if (d1 == 64u) {                  /* sustain: the notes let go meanwhile go out with it */
                st.sustain = d2 >= 64u;
                if (!st.sustain)
                    for (i = 0; i < 128u; i++)
                        if (bit_get(sus, (int)i)) {
                            bit_put(sus, (int)i, 0);
                            midi_note_off((int)i);
                        }
            } else if (d1 == 123u) {             /* all notes off: the track's */
                if (dr)
                    release_drones();
                if (gl)
                    midi_glints_off();
                if (bs && mb_n) {
                    int own = bass_note == mbass[mb_n - 1] && st.v[P_BQ_ON] != SQT_ARP;   /* (else the keys have the
                                                                                          * bass, or the arpeggio) */
                    for (i = 0; i < mb_n; i++)
                        if (st.v[P_BQ_ON] != SQT_SEQ && !(barp.latch && st.v[P_BQ_ON] == SQT_ARP))
                            arp_remove(&barp, mbass[i]);
                    mb_n = 0;
                    if (own)
                        bass_stop();
                }
            } else if (!dr) {
                /* the sound's CCs: on the drones' channel only */
            } else if (eng_cc(d1, d2)) {     /* the runner's first (the FM-1's: MIDI learn, a CC learned) */
            } else if (d1 == 11u) {
                st.swell_midi = (float)d2 * (1.0f / 127.0f);
            } else if (d1 == 80u) {
                param_set(P_FREEZE, d2 >= 64u);
            } else {
                for (i = 0; i < sizeof CC_MAP / sizeof CC_MAP[0]; i++)
                    if (CC_MAP[i].cc == d1)
                        param_set(CC_MAP[i].p, ((int32_t)d2 * PD[CC_MAP[i].p].max + 63) / 127);
            }
        } else if (kind == 0xC0u && dr) {   /* Program Change: the runner's (the FM-1's: a slot, as SCENES' MORPH and
                                             * LOAD) */
            eng_program(d1);
        }
    }
}

/* COUNT-IN (P_COUNT_IN, 1 BAR; the engine alone's frs_count_in): a LIVE take from silence (REC from nothing playing,
 * eng_record_on; PLAY with its take armed and nothing playing: eng_play_tap, frs_play_all) ticks a bar first, four beats at the
 * grid's tempo (TEMPO, or the MIDI clock's), the one high, whatever the metronome; on the downbeat after them everything
 * starts as PLAY would have (count_go: play_top), the take with it. What starts meanwhile (a sequence, an arpeggio)
 * waits for the downbeat with the rest (count_hush). A stop, CUT, PANIC, RELEASE ALL, a scene taken, REC off LIVE: no
 * more count, nothing started. The FM-1's runs at eng_moves (count_own); the engine alone's in its own place, with its
 * tracks (frasil_engine.c: count_own 0, its count_hush and count_go) */
static uint8_t count_own = 1;
static uint8_t rec_counts;               /* the take was begun by 3.0's REC (eng_record_on, counting): PLAY counts in for it
                                          * too; eng_record's (2.x's) plays at once, as it always did */

static int take_armed(void)              /* a LIVE take armed: RECORD LIVE, the layer on SEQ */
{
    return (st.v[P_SQ_REC] == 2u && st.v[P_SQ_TYPE] == SQT_SEQ) || (st.v[P_BQ_REC] == 2u && st.v[P_BQ_ON] == SQT_SEQ) ||
           (st.v[P_DQ_REC] == 2u && st.v[P_DQ_ON]);
}

static int playing_any(void)             /* a sequence or an arpeggio: what PLAY starts again with */
{
    return (st.v[P_SQ_TYPE] == SQT_SEQ && (sq.run || sq.restart)) || (st.v[P_BQ_ON] == SQT_SEQ && bq.run) ||
           (arp_plays() && !arp.paused) || (st.v[P_BQ_ON] == SQT_ARP && !barp.paused) || (st.v[P_DQ_ON] && dq.run);
}

/* REC's silence: no sequence runs, on the FM-1 none with notes (SEQUENCE ON, an empty one going round, is silence), no
 * arpeggio steps through notes (a layer on ARP with none plays nothing: NEW SHARD's GLINT; chords latched, the ring, a
 * bass held sound on over the count) */
static int transport_on(void)
{
    return (st.v[P_SQ_TYPE] == SQT_SEQ && (sq.run || sq.restart) && (!step_hand || seq_has_notes())) ||
           (st.v[P_BQ_ON] == SQT_SEQ && bq.run && (!step_hand || bseq_has_notes())) ||
           (st.v[P_DQ_ON] && dq.run && (!step_hand || dseq_has_notes())) ||
           (st.v[P_SQ_TYPE] == SQT_ARP && arp.n && !arp.paused) || (st.v[P_BQ_ON] == SQT_ARP && barp.n && !barp.paused);
}

static void count_stop(void) { cnt.run = cnt.beat = cnt.glint = 0; }

static void count_hush_own(void)         /* (what began during the count: stopped, or waiting, till the downbeat) */
{
    if (st.v[P_SQ_TYPE] == SQT_SEQ && (sq.run || sq.restart)) {
        sq.restart = 0;
        sq.at = -1.0f;
        seq_note_off();
        sq.run = 0;
    } else if (arp_plays() && !arp.paused) {
        arp_wait(&arp, 1);
    }
    if (st.v[P_BQ_ON] == SQT_SEQ && bq.run)
        bseq_stop();
    else if (st.v[P_BQ_ON] == SQT_ARP && !barp.paused)
        arp_wait(&barp, 1);
    if (dq.run)
        dseq_stop();
}

/* the sequences from their first steps together, the beat beginning there (following a MIDI clock, at its next step),
 * the arpeggios from their patterns' tops: PLAY's start (eng_play), COUNT-IN's downbeat */
static void play_top(void)
{
    int gs = st.v[P_SQ_TYPE] == SQT_SEQ, ga = arp_plays(), bs = st.v[P_BQ_ON] == SQT_SEQ;
    int ba = st.v[P_BQ_ON] == SQT_ARP, ds = st.v[P_DQ_ON] != 0;
    if (grid_top(0)) {
        seqs_top(gs, bs, ds);
    } else {
        if (gs)
            seq_begin();
        if (bs)
            bseq_begin();
        if (ds)
            dseq_begin();
    }
    if (ga)
        arp_wait(&arp, 0);
    if (ba)
        arp_wait(&barp, 0);
}

static void (*count_hush)(void) = count_hush_own;
static void (*count_go)(void) = play_top;

static void count_begin(void)
{
    cnt.run = 1;
    cnt.beat = 0;
    cnt.t = 0.0f;
    cnt.at = eng_ms();
    if (playing_any())
        count_hush();
}

static void count_run(void)              /* (as often as it comes: a beat's tick as it comes, the start on the fifth; by
                                          * the time gone, as arp_run's grid) */
{
    float ms_tick;
    uint32_t now = eng_ms(), dt = now - cnt.at;
    if (!cnt.run)
        return;
    cnt.at = now;
    if (playing_any())
        count_hush();
    if (cnt.t >= 4.0f) {                 /* (the FM-1's: what the last half beat wrote survives the sequences' top) */
        int tn = bq.tn, skip = bq.skip, gl = cnt.glint;
        count_stop();
        count_go();
        if (step_hand) {
            if (tn >= 0 && st.v[P_BQ_REC] == 2u) {   /* (a bass note held across it: it starts the take) */
                bq.tn = (int8_t)tn;
                bq.tp = bq.te = 0.0f;
            }
            if (skip == 0)               /* (struck and let go before it: heard then, once) */
                bq.skip = 0;
            if (gl) {
                sq.skip = 0;
                sq.near = sq.restart;
            }
        }
        return;
    }
    if ((uint32_t)cnt.t >= cnt.beat) {
        frasil_click(cnt.beat == 0u);
        cnt.beat++;
    }
    ms_tick = grid_ms_tick(eng_ms());
    cnt.t += (float)dt / (24.0f * ms_tick);
}

/* REC on a layer (0 the drone, 1 GLINT, 2 the bass: the FM-1's selected one, 3.0's R1): OFF, LIVE, STEP, OFF. LIVE: into
 * its sequence, recording (its PLAYS turned to SEQ if it was not, its arpeggio's notes kept; the drone's sequence turned
 * on; the keys its own, rec_keys, the sequence playing at once); with `count` and COUNT-IN on, from nothing playing
 * (transport_on), counted in first (count_begin: the take and every sequence from the downbeat after the bar). STEP: the
 * keys writing at STEP (the drone's: a chord from the keys down, STEP on as the last lets go), the sequence going on or
 * not as PLAY has it. OFF: RECORD OFF (all three), the keys back to what they played (rec_keys), what was recorded
 * playing on, a count under way stopped. Recording already, REC moves the layer's take on, else the one recording. 1:
 * GLINT's recording now, 2: the bass's, 3: the drone's, 0: none (the FM-1 shows its page as one begins). UNDO TAKE
 * has the sequence as it was before each take (seq_keep, bseq_keep; the drone's: dseq_set) */
static int rec_code(int p) { return p == P_BQ_REC ? 2 : p == P_SQ_REC ? 1 : 3; }

static void eng_record_off(void)         /* RECORD OFF, all three, a count under way stopped (the FM-1's PLAY too, stopping).
                                          * A take that wrote nothing leaves its track as it found it: its PLAYS back */
{
    static const uint8_t RP[3] = {P_DQ_REC, P_SQ_REC, P_BQ_REC}, TP[3] = {P_DQ_ON, P_SQ_TYPE, P_BQ_ON};
    uint32_t t = rec_was.layer % 3u;
    int back = rec_was.on && st.v[RP[t]] && !rec_was.wrote &&
               !(t == 0u ? memcmp(dnote, dq_prev, sizeof dnote) : t == 1u ? memcmp(seq, seq_prev, sizeof seq)
                                                                         : memcmp(bnote, bq_prev, sizeof bnote));
    rec_was.on = 0;
    param_set(P_SQ_REC, 0);
    param_set(P_BQ_REC, 0);
    param_set(P_DQ_REC, 0);
    if (cnt.run)
        count_stop();
    if (back)
        param_set(TP[t], rec_was.plays);
}

/* a take begun on the FM-1 while another track's sequence plays (layer: 0 the drone, 1 GLINT, 2 the bass): this track's
 * sequence, not yet going, put where the other is in its own loop, round this one's: each step falls where it did from
 * the top together, so the take stays against what plays once PLAY starts everything again, and the metronome (this
 * sequence's) stays on the beat. Not frasil_engine.c's origins (where it would be had it begun with the other, all those
 * turns ago): over a short loop, those would put a take's first chord anywhere in a longer one */
static void take_join(uint32_t layer)
{
    float c, t, loop;
    if (layer != 1u && st.v[P_SQ_TYPE] == SQT_SEQ && sq.run && sq.pos >= 0) {
        float k = (float)ARP_TICKS[st.v[P_ARP_RATE] % 10u];
        c = (float)sq.pos * k + seq_grid(grid.tick) - (float)sq.n * k;
    } else if (layer != 2u && bseq_playing()) {
        c = bseq_at();
    } else if (layer != 0u && dseq_playing()) {
        c = dseq_at();
    } else {
        return;
    }
    if (layer == 1u) {                   /* GLINT's: its steps sq.off clocks after the grid's */
        uint32_t ticks = ARP_TICKS[st.v[P_ARP_RATE] % 10u];
        float f, top;
        int32_t a;
        t = (float)ticks;
        loop = t * (float)seq_len();
        c -= loop * fm_floorf(c / loop);
        a = (int32_t)fm_floorf(c / t);
        f = c - t * (float)a;            /* (the clocks into its step) */
        top = fm_floorf(grid.tick - f + 0.5f);
        top = top < 0.0f ? top + 9600.0f : top;
        seq_note_off();
        sq.restart = sq.near = 0;
        sq.run = 1;
        sq.at = -1.0f;
        sq.off = (uint8_t)((uint32_t)top % ticks);
        sq.n = seq_stepn(grid.tick, ticks);
        sq.start = sq.n - a;
        sq.pos = a;                      /* (the step under way: it plays from the next) */
        sq.skip = -1;
        take_new(&gtk);
    } else if (layer == 2u) {
        loop = (float)bseq_ticks() * (float)(st.v[P_BQ_STEPS] + 1u);
        bseq_close();
        bq.run = 1;
        bq.wait = 0;
        bq.skip = -1;
        bq.g = grid.tick;
        take_new(&btk);
        bseq_seek(c - loop * fm_floorf(c / loop));
        if (bq.t < 0.5f)
            bseq_step(bq.pos);
    } else {
        int32_t a;
        t = (float)dseq_ticks();
        loop = t * (float)dseq_len();
        c -= loop * fm_floorf(c / loop);
        a = (int32_t)fm_floorf(c / t);
        dq.run = 1;
        dq.wait = 0;
        dq.g = grid.tick;
        dq.pos = (int8_t)a;
        dq.t = c - t * (float)a;
        take_new(&dtk);
        if (dq.t < 0.5f)
            dseq_step(a);
    }
}

static int eng_record_on(uint32_t layer, int count)
{
    static const uint8_t RP[3] = {P_DQ_REC, P_SQ_REC, P_BQ_REC};
    layer %= 3u;
    if (st.v[P_SQ_REC] || st.v[P_BQ_REC] || st.v[P_DQ_REC]) {
        static const char *const KB[3] = {"DRONE", "GLINT", "BASS"};
        int p = st.v[RP[layer]] ? RP[layer] : st.v[P_SQ_REC] ? P_SQ_REC : st.v[P_BQ_REC] ? P_BQ_REC : P_DQ_REC;
        if (st.v[p] == 2u) {
            param_set(p, 1);
            if (cnt.run && !take_armed())   /* (no take left to count in for) */
                count_stop();
            eng_say("RECORD", "STEP");
            return rec_code(p);
        }
        eng_record_off();
        eng_say("RECORD OFF", KB[st.v[P_KB] % 3u]);
        return 0;
    }
    {
        static const uint8_t TP[3] = {P_DQ_ON, P_SQ_TYPE, P_BQ_ON};
        int quiet = count && st.v[P_COUNT_IN] && !transport_on();
        rec_counts = (uint8_t)(count != 0);
        rec_was.on = step_hand;
        rec_was.layer = (uint8_t)layer;
        rec_was.plays = st.v[TP[layer]];
        rec_was.wrote = 0;
        if (layer == 2u) {
            bseq_keep();                 /* (UNDO: the sequence before this take) */
            param_set(P_BQ_ON, (int32_t)SQT_SEQ);
            param_set(P_BQ_REC, 2);
            if (!bq.run)
                bseq_begin();
        } else if (layer == 1u) {
            seq_keep();                  /* (UNDO: the sequence before this take) */
            param_set(P_SQ_TYPE, (int32_t)SQT_SEQ);
            param_set(P_SQ_REC, 2);      /* (the keys GLINT's: rec_keys) */
            if (!sq.run)
                seq_begin();
        } else {
            param_set(P_DQ_ON, 1);       /* (UNDO: the sequence before this take, dseq_set's) */
            param_set(P_DQ_REC, 2);
            if (!dq.run)
                dseq_begin();
        }
        if (quiet) {
            count_begin();
            eng_say("COUNT-IN", "1 BAR");
        } else {
            if (step_hand && (layer == 2u ? !bseq_playing() : layer == 1u ? sq.pos < 0 : !dseq_playing()))
                take_join(layer);
            eng_say("RECORD", "LIVE");
        }
        return layer == 2u ? 2 : layer == 1u ? 1 : 3;
    }
}

/* REC as the FM-1's 2.x had it (and frs_record): the layer the keys play, on the drones GLINT's; no count-in */
static int eng_record(void) { return eng_record_on(st.bass ? 2u : 1u, 0); }

/* with nothing playing, the scene again, its three tracks (RESTORE): what RELEASE ALL let go, else the scene's own notes
 * as saved (saved: at power-on, say), and its sequences that are on from their first steps (GLINT's, the bass's; the
 * arpeggio from its notes), the beat beginning there (grid_top: following a MIDI clock, its beats kept). 0: something
 * plays, or there is nothing to play */
static int eng_scene_again(const scene_t *saved)
{
    const scene_t *n = rst_valid ? &rst : saved;
    int gs = st.v[P_SQ_TYPE] == SQT_SEQ, bs = st.v[P_BQ_ON] == SQT_SEQ, ds = st.v[P_DQ_ON] != 0;
    if (holding_any() || !(rst_valid || n->nch || n->narp || n->nbarp || n->bass >= 0 || gs || bs || ds))
        return 0;
    rst_valid = 0;
    if (grid_top(0)) {
        scene_notes_play(n);             /* (after grid_top: the bass's note played on the top) */
        seqs_top(gs, bs, ds);
    } else {
        scene_notes_play(n);
        if (gs)
            seq_begin();
        if (bs)
            bseq_begin();
        if (ds)
            dseq_begin();
    }
    return 1;
}

/* PLAY: the transport (GLINT's sequence and the bass's stop together, or start together from their first step; an
 * arpeggio waits, or goes on from the top: with the other's sequence, as it does). Started, the beat begins there
 * (grid_top), the sequences' first steps at once; following a MIDI clock, on its next step (its beats kept). Stopped,
 * the grid goes on. The sequences say which: running, it stops; not, it starts (the arpeggio playing alone beside them
 * starts again with them) (the FM-1's play_tap: on SCENES, a slot instead; nothing playing at all, eng_play_tap) */
static void eng_play(void)
{
    int gs = st.v[P_SQ_TYPE] == SQT_SEQ, ga = arp_plays(), bs = st.v[P_BQ_ON] == SQT_SEQ;
    int ba = st.v[P_BQ_ON] == SQT_ARP, ds = st.v[P_DQ_ON] != 0;
    if (gs || bs || ds) {
        if ((gs && sq.run) || (bs && bq.run) || (ds && dq.run)) {
            seq_note_off();
            sq.run = 0;
            bseq_stop();
            dseq_stop();
            if (ga)
                arp_wait(&arp, 1);
            if (ba)
                arp_wait(&barp, 1);
            eng_say("SEQUENCE", "STOPPED");
        } else if (count_own && rec_counts && st.v[P_COUNT_IN] && take_armed()) {   /* (a LIVE take armed: COUNT-IN first) */
            count_begin();
            eng_say("COUNT-IN", "1 BAR");
        } else {
            play_top();
            eng_say("SEQUENCE", "FROM THE TOP");
        }
    } else if (ga || ba) {               /* (back: their patterns from the top, the beat from here) */
        int w = (ga && !arp.paused) || (ba && !barp.paused);
        if (ga)
            arp_wait(&arp, w);
        if (ba)
            arp_wait(&barp, w);
        if (!w)
            grid_top(0);
        eng_say("ARPEGGIO", w ? "WAITING" : "PLAYING");
    } else {
        eng_say("HOLD PLAY", "RELEASE ALL");
    }
}

/* PLAY tapped (not twice: the runner's): nothing playing at all (at power-on, after RELEASE ALL), the scene's three
 * tracks, as held (eng_scene_again: saved the scene's own); else the transport (eng_play) */
static void eng_play_tap(const scene_t *saved)
{
    if (count_own && cnt.run) {          /* (counting in: PLAY stops it, nothing started) */
        count_stop();
        eng_say("COUNT-IN", "STOPPED");
        return;
    }
    if (count_own && rec_counts && st.v[P_COUNT_IN] && take_armed() && !transport_on()) {   /* (a LIVE take armed, from
                                                                                             * silence: COUNT-IN, not the
                                                                                             * scene at once) */
        count_begin();
        eng_say("COUNT-IN", "1 BAR");
        return;
    }
    if (eng_scene_again(saved)) {
        eng_say("PLAY", "FROM THE TOP");
        return;
    }
    eng_play();
}

/* PLAY held: RELEASE ALL (sound_off) while anything plays; with nothing playing, the scene again (eng_scene_again,
 * RESTORED). Either way the beat begins there (grid_top: following a MIDI clock, its beats kept) */
static void eng_release_all(const scene_t *saved)
{
    if (eng_scene_again(saved)) {
        eng_say("RESTORED", "");
        return;
    }
    sound_off();
    grid_top(0);
    eng_say("RELEASE ALL", rst_valid ? "HOLD AGAIN: RESTORE" : "");
}

/* CUT: every sound gone now, the cloud's tail and the echoes too: RELEASE ALL, then the engine emptied (frasil_cut: the
 * next block faded out, the one after silent) */
static void eng_cut(void)
{
    sound_off();
    grid_top(0);
    frasil_cut();
}

/* the swell, every pass: up while held (the FM-1's ENV held), down once let go, 1.5 s each way; MIDI's (CC 11, the
 * channel's pressure) and GENERATIVE FX's SWELL beside it */
static uint32_t swell_at;                /* (eng_swell's last) */
static void eng_swell(int held)
{
    uint32_t now = eng_ms(), dt = now - swell_at;
    float s, k, step = (float)(dt > 100u ? 100u : dt) * (1.0f / 1500.0f);   /* 1.5 s up, 1.5 s down */
    swell_at = now;
    if (held)
        st.swell_btn = st.swell_btn + step > 1.0f ? 1.0f : st.swell_btn + step;
    else
        st.swell_btn = st.swell_btn - step < 0.0f ? 0.0f : st.swell_btn - step;
    s = st.swell_btn > st.swell_midi ? st.swell_btn : st.swell_midi;
    k = ((float)st.v[P_SWELL] + morph_off[P_SWELL]) * (1.0f / 127.0f);   /* GENERATIVE FX's, where it is turned (the
                                                                           * scene's: ANCHOR's way back glides it) */
    k = k < 0.0f ? 0.0f : k > 1.0f ? 1.0f : k;
    if (k > s)
        s = k;
    if (fm_fabsf(s - st.swell_sent) > 0.004f || (s == 0.0f && st.swell_sent != 0.0f)) {
        st.swell_sent = s;
        frasil_swell(s * s * (3.0f - 2.0f * s));
    }
}

/* key k down (k: the runner's own number for it, below FRS_FRS_NKEYS), its note as played (the runner's octave on it),
 * vel 1 .. 127: what the keys play (KEYS: the drones, GLINT, BASS) and each layer's KEYS say (touch_of: KEY MODE's till
 * eng_keys_touch). single: a single note, whatever NOTES and CHORD say (the FM-1: SEL held) */
static void key_begin(uint32_t k)        /* (its state, as it goes down) */
{
    key[k].t = eng_ms();
    key[k].prs = 0;
    key[k].pressed_in = 0;
    key[k].was_lit = 0;
    key[k].drec = 0;
    key[k].hold = 0;
    key[k].chord.k = 0;
}

/* the notes a key makes on a layer (played: as played; t: the layer's KEYS): HARMONY off, the note itself (chromatic, no
 * chord); one note (single, NOTES SINGLE NOTE, or a layer that plays one: GLINT on SEQ, the bass but on ARP), the scale's
 * note at or below it (the drones' through CHORD SINGLE, VOICING CLOSE, as a SEL's); else HARMONY's chord (the iPad's
 * FrasilModel.press: GLINT's and the bass's built here, each note of it into the arpeggio or sounding) */
static void key_notes(uint32_t layer, touch_t t, int played, int single, chord_t *c, int *root)
{
    played = played < 0 ? 0 : played > 127 ? 127 : played;
    if (!t.harm) {
        c->k = 1;
        c->n[0] = (int8_t)played;
        *root = played;
        return;
    }
    if (single || !t.chord) {
        if (layer == 0u) {
            uint8_t v1[NSOUND];
            memcpy(v1, st.v, sizeof v1);
            v1[P_CHORD] = 0;
            v1[P_VOICING] = 0;
            chord_build(played, v1, c, root);
        } else {
            *root = scale_snap(played, st.v);
            c->k = 1;
            c->n[0] = (int8_t)*root;
        }
        return;
    }
    chord_build(played, st.v, c, root);
    if (layer && !c->k) {                /* (a chord all past the ends: its note alone) */
        *root = scale_snap(played, st.v);
        c->k = 1;
        c->n[0] = (int8_t)*root;
    }
}

static int chord_has(const chord_t *c, int n)
{
    uint32_t i;
    for (i = 0; i < c->k; i++)
        if (c->n[i] == n)
            return 1;
    return 0;
}

/* a key's notes all among an arpeggio's (latched: a tap on them lets them go, STACK on) */
static int arp_has_all(const arp_t *a, const chord_t *c)
{
    uint32_t i, j, n = 0;
    for (i = 0; i < c->k; i++)
        for (j = 0; j < a->n; j++)
            if (a->held[j] == c->n[i]) {
                n++;
                break;
            }
    return c->k && n == c->k;
}

/* a key's notes into a layer's arpeggio (k: the key, kind its kind): latched, a new hand (no other key of its down) a
 * new set, STACK off (SWAP); STACK on, they add (all there already: a tap lets them go) */
static void arp_key(arp_t *a, uint32_t k, uint32_t kind, int stack, uint32_t vel)
{
    uint32_t i;
    if (a->latch) {
        if (stack && arp_has_all(a, &key[k].chord)) {
            key[k].was_lit = 1;
            return;
        }
        if (!stack && arp_new_hand(a, keys_down(kind, k))) {   /* latched, a new hand: a new set (at the next step;
                                                                * ARP SNAP: on its grid) */
            a->n = 0;
            a->dirty = 1;
        }
    }
    for (i = 0; i < key[k].chord.k; i++)
        arp_add(a, key[k].chord.n[i], vel);
    a->hand_ms = eng_ms();
    a->paused = 0;                       /* (waiting, PLAY tapped: a key plays it again, as frs_latch's) */
}

#if FRS_LEAD
/* LEAD (card #144, the app's builds: FRS_LEAD). MONO: one note at a time, the last key down sounding. A key played while
 * another is down glides there (legato, not struck again: GLIDE); one let go, back to the last one still down (gliding),
 * none left: let go. POLY: each key its voice, struck, let go with its key (past LEAD_NV the DSP takes the quietest
 * let go, else the oldest held). The note as played (no HARMONY: what the key says; OCTAVE on it in the DSP). Nothing
 * latches, nothing goes into a sequence */
enum { K_LEAD = 32 };                    /* (a key's kind, apart from the others' list) */
#define LD_KEYS 8u
static struct {
    uint8_t n;                           /* the keys down, the oldest first */
    uint8_t k[LD_KEYS], vel[LD_KEYS];
    int8_t note[LD_KEYS];
} ldk;

static void ldk_drop(uint32_t i)
{
    for (; i + 1u < ldk.n; i++) {
        ldk.k[i] = ldk.k[i + 1u];
        ldk.vel[i] = ldk.vel[i + 1u];
        ldk.note[i] = ldk.note[i + 1u];
    }
    ldk.n--;
}

static void ldk_clean(void)              /* (a panic forgets the keys, frs_panic's memset: theirs no more) */
{
    uint32_t i = 0;
    while (i < ldk.n)
        if (key[ldk.k[i]].kind != K_LEAD)
            ldk_drop(i);
        else
            i++;
}

static void lead_key_down(uint32_t k, int note, uint32_t vel)
{
    int legato, poly = st.v[P_LD_MODE] != 0;
    ldk_clean();
    legato = ldk.n > 0;
    if (ldk.n >= LD_KEYS) {
        if (poly)                        /* (its voice no more: let go) */
            frasil_lead_poly_off(ldk.note[0]);
        ldk_drop(0);
    }
    ldk.k[ldk.n] = (uint8_t)k;
    ldk.vel[ldk.n] = (uint8_t)vel;
    ldk.note[ldk.n] = (int8_t)note;
    ldk.n++;
    key[k].kind = K_LEAD;
    key[k].root = (int8_t)note;
    if (poly)
        frasil_lead_poly_on(note, (float)vel * (1.0f / 127.0f));
    else
        frasil_lead_on(note, (float)vel * (1.0f / 127.0f), legato);
}

static void lead_key_up(uint32_t k)
{
    uint32_t i;
    int top, note;
    for (i = 0; i < ldk.n && ldk.k[i] != k; i++)
        ;
    if (i == ldk.n)
        return;
    top = i + 1u == ldk.n;
    note = ldk.note[i];
    ldk_drop(i);
    key[k].kind = K_NONE;
    ldk_clean();
    if (st.v[P_LD_MODE]) {               /* POLY: its voice let go, the others as they are */
        frasil_lead_poly_off(note);
        return;
    }
    if (!top)
        return;
    if (ldk.n)
        frasil_lead_on(ldk.note[ldk.n - 1u], (float)ldk.vel[ldk.n - 1u] * (1.0f / 127.0f), 1);
    else
        frasil_lead_off();
}

static int lead_key_top(uint32_t k) { return ldk.n && ldk.k[ldk.n - 1u] == k; }

/* the SMK-37's wheels on the lead: mod 0 .. 127 (its VIBRATO, if more), the pitch wheel -8192 .. 8191 (two semitones
 * either way): the same floats wherever they are heard (Perform LIVE's FST_LEAD_WHEELS, card #152) */
static void eng_lead_wheels(uint32_t mod, int32_t bend)
{
    frasil_lead_mod((float)mod * (1.0f / 127.0f));
    frasil_lead_bend((float)bend * (2.0f / 8192.0f));
}
#endif

/* ------------------------------------------------------- the shard's LEAD --- */
/* a shard's LEAD (card #152, "FRS~": the scene's bytes, then LEAD_BYTES, P_LD_WAVE .. P_LD_FRELEASE in order): MOSS's
 * (none in it: an older layout's, a scene stored without one); taken as a scene's (app.c ldp_load's: each past its
 * max its default, PRESET's CUSTOM: lead_in; the LEAD as saved with them, the voices as they are: a MODE changed so, the
 * next key in it); put as
 * the LEAD is now (each so, the LEAD as saved left; MODE changed: the voices let go, as param_set's); as it is. Without
 * the LEAD (FRS_LEAD 0): MOSS's, nothing taken */
static void eng_lead_none(uint8_t *out) { memcpy(out, LEAD_MOSS, LEAD_BYTES); }
#if FRS_LEAD
_Static_assert(NLDS == LEAD_BYTES && P_MX_LEAD == P_LD_WAVE + 16 && P_LD_PRESET == P_LD_WAVE + 17,
               "a shard's LEAD: its 25 settings in a row, MOSS's in their order");
static void lead_set(const uint8_t *in)
{
    uint32_t i;
    for (i = 0; i < NLDS; i++) {
        uint32_t p = P_LD_WAVE + i;
        st.v[p] = lead_in(p, in[i]);
    }
    st.engine_dirty = 1;
}
static void eng_lead_take(const uint8_t *in)
{
    lead_set(in);
    memcpy(saved_ld, &st.v[P_LD_WAVE], sizeof saved_ld);
    st.dirty = (uint8_t)sound_changed();
}
static void eng_lead_put(const uint8_t *in)
{
    uint8_t m = st.v[P_LD_MODE];
    lead_set(in);
    if (st.v[P_LD_MODE] != m)
        frasil_lead_off();
    st.dirty = (uint8_t)sound_changed();
}
static void eng_lead_get(uint8_t *out) { memcpy(out, &st.v[P_LD_WAVE], NLDS); }
#else
static void eng_lead_take(const uint8_t *in) { (void)in; }
static void eng_lead_put(const uint8_t *in) { (void)in; }
static void eng_lead_get(uint8_t *out) { eng_lead_none(out); }
static void eng_lead_wheels(uint32_t mod, int32_t bend)
{
    (void)mod;
    (void)bend;
}
#endif

static void eng_key_down(uint32_t k, int played, uint32_t vel, int single)
{
    int root, again, adds = single && keys_touch;   /* (SEL held: the note over what is held or latched, whatever STACK says) */
    uint32_t i;
    chord_t c;
    touch_t t;
    key_begin(k);
#if FRS_LEAD
    if (st.lead) {                       /* LEAD: the note as played, one at a time (lead_key_down) */
        lead_key_down(k, played, vel);
        return;
    }
#endif
    if (st.glint) {                      /* a glint (its note, or HARMONY's chord): GLINT's octaves up (the arpeggio and
                                          * the sequence keep it without them, OCTAVE on them as they play) */
        int sk = seq_keys(), b;
        t = touch_of(1);
        key_notes(1, t, played, single || st.v[P_SQ_TYPE] == SQT_SEQ, &c, &root);   /* (OCTAVE on it in the engine) */
        b = c.n[0];
        key[k].kind = K_GLINT;
        key[k].root = (int8_t)b;
        key[k].gbase = (int8_t)b;
        key[k].chord = c;
        key[k].gsound = sk ? st.v[P_SQ_REC] != 0 : st.v[P_SQ_TYPE] != SQT_ARP;
        if (sk) {
            seq_key(b, key[k].root, vel);
            return;
        }
        if (st.v[P_SQ_TYPE] == SQT_ARP) {
            arp_key(&arp, k, K_GLINT, t.stack || adds, vel);
        } else {                         /* KEYS: sounding while down, whatever KEYS' LATCH says (card #143: its latch
                                          * ARP II's, on ARP, and the sequence's) */
            if (!st.v[P_SQ_TYPE] && ((arp.n && !arp.paused) || rung.n)) {   /* (the hand takes GLINT: what rings,
                                                             * THE KEYS' latch, a scene's, ARP's list once ARP is off,
                                                             * let go first, its voices free, as a new hand did on the
                                                             * ring; waiting, PLAY tapped, it stays) */
                key[k].kind = K_NONE;
                arp.n = 0;
                arp.dirty = 1;
                ring_run();
                key[k].kind = K_GLINT;
            }
            for (i = 0; i < c.k; i++)
                frasil_glint_on(c.n[i], (float)vel * (1.0f / 127.0f));
        }
        return;
    }
    if (st.bass) {                       /* the bass: one note (its arpeggio, HARMONY's chord), in the scale, BASS's
                                          * octaves down; held as a drone is */
        int n;
        t = touch_of(2);
        key_notes(2, t, played, single || st.v[P_BQ_ON] != SQT_ARP, &c, &root);   /* (OCTAVE on it in the engine) */
        n = c.n[0];
        key[k].root = (int8_t)n;
        key[k].chord = c;
        if (bseq_keys()) {               /* its sequence on (with notes, or RECORDing): into it (bseq_key) */
            key[k].kind = K_BSEQ;
            key[k].gbase = (int8_t)n;
            bseq_key(n, n, vel);
            return;
        }
        if (st.v[P_BQ_ON] == SQT_ARP) {  /* its arpeggio, as GLINT's */
            key[k].kind = K_BARP;
            key[k].gbase = (int8_t)n;
            arp_key(&barp, k, K_BARP, t.stack || adds, vel);
            return;
        }
        key[k].kind = K_BASS;
        key[k].hold = (uint8_t)(!t.latch || bseq_owns());   /* (tried over its sequence: while down, nothing latched) */
        if (!key[k].hold && bass_note == key[k].root)
            key[k].was_lit = 1;          /* latched already: a tap lets it go */
        else
            bass_live(key[k].root, vel);
        if (bseq_owns())
            bq.sounding = -1;            /* (not its note: no GATE on it) */
        return;
    }
    t = touch_of(0);
    if (step_hand && st.v[P_DQ_REC] == 1u)   /* (RECORD STEP on the FM-1: the keys sound while down, nothing latches, so
                                              * what plays after is the sequence) */
        t.latch = 0;
    key_notes(0, t, played, single, &c, &root);
    if (!t.latch) {                      /* held: the chord while the key is down */
        key[k].kind = K_HOLD;
        key[k].chord = c;
        chord_on(&c, vel);
        dq.adds = (uint8_t)adds;
        if (dseq_key(c.n, c.k)) {        /* (RECORD on the drone's sequence: into it; STEP's, its up moves STEP on) */
            key[k].drec = 1;
            dq.keys++;
        }
        return;
    }
    if (root < 0) {                      /* (a note under the scale's lowest: no root to latch it on; the FM-1's keys
                                          * never go so low) */
        key[k].kind = K_NONE;
        return;
    }
    key[k].kind = K_LATCH;
    key[k].root = (int8_t)root;
    again = bit_get(lit, root);
    if (again && !(step_hand && dseq_takes())) {
        key[k].was_lit = 1;              /* a tap lets it go; pressing in swells it (recording on the FM-1: it goes in as
                                          * any chord would, and stays) */
        return;
    }
    if (!t.stack && !adds && !keys_down(K_LATCH, k)) {   /* STACK off (SWAP): a new hand's chord in place of those latched (the
                                                 * iPad's FrasilModel.latch; a hand's keys down together stack) */
        uint32_t r;
        for (r = 0; r < 128u; r++)
            if (bit_get(lit, (int)r) && (int)r != root) {
                chord_off(&lat[r]);
                bit_put(lit, (int)r, 0);
            }
    }
    if (again) {
        c = lat[root];                   /* (what sounds) */
    } else {                             /* (eight at most, as eng_latch's and a scene's: a ninth does nothing) */
        uint32_t r, n = 0;
        for (r = 0; r < 128u; r++)
            n += (uint32_t)bit_get(lit, (int)r);
        if (n >= SC_CHORDS) {
            key[k].kind = K_NONE;
            return;
        }
        lat[root] = c;
        bit_put(lit, root, 1);
        chord_on(&c, vel);
    }
    if (step_hand && st.v[P_DQ_REC] == 2u)
        bit_put(dt_lat, root, 1);
    dq.adds = (uint8_t)adds;
    if (dseq_key(c.n, c.k)) {
        key[k].drec = 1;
        dq.keys++;
    }
}

/* KEYS honoured by the keys from now (on), or KEY MODE's as before (off: at the start) */
static void eng_keys_touch(int on) { keys_touch = (uint8_t)(on != 0); }

/* NEW SCENE: KEYS back to their defaults (the device's settings, not a scene's: this alone resets them) */
static void eng_keys_defaults(void)
{
    uint32_t p;
    for (p = P_KY_DR_HARM; p <= P_KY_BS_STACK; p++)
        param_set((int)p, PD[p].def);
}

/* a latched key's tap (let go soon, not pressed into): it lets what it latched go */
static int key_tapped(uint32_t k) { return key[k].was_lit && !key[k].pressed_in && eng_ms() - key[k].t < TAP_MS; }

static void key_up(uint32_t k)
{
    uint32_t i;
    if (key[k].drec) {                   /* (its chord into the drone's sequence on STEP: the last up moves STEP on) */
        key[k].drec = 0;
        dseq_up();
    }
#if FRS_LEAD
    if (key[k].kind == K_LEAD) {
        lead_key_up(k);
        return;
    }
#endif
    switch (key[k].kind) {
    case K_GLINT:
        for (i = 0; i < key[k].chord.k; i++) {
            int n = key[k].chord.n[i];
            if (key[k].gsound && sq.sounding != n && !ring_has(n))   /* what it sounded itself (not the sequence's step,
                                                                      * a latched note) */
                frasil_glint_off(n);
            if (st.v[P_SQ_TYPE] != SQT_SEQ && !arp.latch)
                arp_remove(&arp, n);     /* (if it was arpeggiated; latched, it stays) */
            else if (st.v[P_SQ_TYPE] == SQT_ARP && arp.latch && key_tapped(k))
                arp_remove(&arp, n);     /* (STACK on: a tap on notes latched lets them go) */
        }
        break;
    case K_HOLD:
        chord_off(&key[k].chord);
        break;
    case K_BASS:
        if (bass_note != key[k].root)
            break;                       /* another note has the bass now */
        if (key[k].prs)
            frasil_bass_pressure(0.0f);
        if (key[k].hold) {               /* held: back to the last other bass key still down, else let go */
            int32_t j, last = -1;
            for (j = 0; j < (int32_t)FRS_NKEYS; j++)
                if ((uint32_t)j != k && key[j].kind == K_BASS && (last < 0 || key[j].t - key[last].t < 0x80000000u))
                    last = j;
            key[k].kind = K_NONE;
            if (last >= 0)
                bass_live(key[last].root, 100);
            else
                bass_stop();
        } else if (key_tapped(k)) {
            bass_stop();                 /* latched: a tap on it */
        }
        break;
    case K_BSEQ:
        key[k].kind = K_NONE;            /* (not down any more, for bseq_held) */
        bseq_up(key[k].root);
        break;
    case K_BARP:
        for (i = 0; i < key[k].chord.k; i++)
            if ((st.v[P_BQ_ON] != SQT_SEQ && !(barp.latch && st.v[P_BQ_ON] == SQT_ARP)) ||
                (st.v[P_BQ_ON] == SQT_ARP && barp.latch && key_tapped(k)))
                arp_remove(&barp, key[k].chord.n[i]);   /* (latched, it stays; a tap on it, STACK on: it goes) */
        break;
    case K_LATCH:
        if (key[k].prs)
            chord_press(&lat[(uint8_t)key[k].root], 0.0f);
        if (key_tapped(k) && bit_get(lit, key[k].root)) {
            chord_off(&lat[(uint8_t)key[k].root]);
            bit_put(lit, key[k].root, 0);
        }
        break;
    }
    key[k].kind = K_NONE;
}

static void key_pressure(uint32_t k, uint8_t p)
{
    float f = (float)p * (1.0f / 127.0f);
    uint32_t i;
    if (p > 12u)
        key[k].pressed_in = 1;
    if (!(p > key[k].prs + 2u || p + 2u < key[k].prs || (p == 0 && key[k].prs)))
        return;
    key[k].prs = p;
#if FRS_LEAD
    if (key[k].kind == K_LEAD) {         /* (MONO: the key sounding's; POLY: each key's voice) */
        if (st.v[P_LD_MODE])
            frasil_lead_pressure(key[k].root, f);
        else if (lead_key_top(k))
            frasil_lead_pressure(-1, f);
        return;
    }
#endif
    if (key[k].kind == K_GLINT)
        for (i = 0; i < key[k].chord.k; i++)
            glint_note_press(key[k].chord.n[i], f);
    else if (key[k].kind == K_HOLD)
        chord_press(&key[k].chord, f);
    else if (key[k].kind == K_BARP)
        for (i = 0; i < key[k].chord.k; i++)
            arp_press(&barp, key[k].chord.n[i], f);
    else if ((key[k].kind == K_BASS || key[k].kind == K_BSEQ) && bass_note == key[k].root)
        frasil_bass_pressure(f);
    else if (key[k].kind == K_LATCH && bit_get(lit, key[k].root))
        chord_press(&lat[(uint8_t)key[k].root], f);
}

/* ---------------------------------------------------- THE KEYS, a shard --- */
/* The River's (app/web.c) and any runner's with keys of its own: a shard played (▶: one that plays nothing holds a
 * chord on its ROOT, never saved) or taken silent (an editor's), its settings and steps edited, its notes latched and
 * let go, keys held while they are down; the shard as it is now, as STORE keeps it */
static uint8_t keys_edited;              /* what the shard plays edited since it came in (THE KEYS) */
static void eng_holds_off(void);         /* (THE KEYS' HOLD, below: nothing held any more; what they hold let go) */
static void eng_holds_release(void);

static void eng_scene_play(const scene_t *sc, int hold)
{
    keys_edited = 0;
    eng_holds_off();
    scene_audition(sc, hold);
}

static void eng_scene_stop(void)
{
    eng_holds_off();
    audition_stop();
}

/* what the shard plays, edited: ▶'s chord on its ROOT let go first (it plays something of its own now), and what
 * HOLD holds (the editor's engine never had it) */
static void eng_edits(void)
{
    eng_holds_release();
    if (aud_root >= 0 && bit_get(lit, aud_root)) {
        chord_off(&lat[aud_root]);
        bit_put(lit, aud_root, 0);
    }
    aud_root = -1;
    keys_edited = 1;
}

/* the River's editor: a sound setting (p < NSOUND) or the scene's TEMPO set to x as a MIDI CC learned on it sets it
 * (param_set: a preset's sound, CUSTOM from a drone's setting turned, GLINT's type...); the sequences' steps as GLINT
 * SEQ's and BASS SEQ's encoders set them (the step on STEP, then its NOTE, ACCENT, the note's LENGTH; CLEAR turned twice:
 * x 2; SEQ LENGTH, RATE, PLAYS, TRANSPOSE, the bass's MODE, RANGE, CHANCE, GATE; RECORD not: THE KEYS write the steps).
 * Then the shard as it is now
 * (eng_scene_edited): the one played, its sound as STORE keeps it (kept_v: the drone's CUSTOM settings and its preset's
 * number, not the preset's values) and its TEMPO (one it had out of range kept as it was); once what it plays was
 * edited, what plays now as STORE keeps it (scene_notes_take: the chords latched, the arpeggio's notes, the bass held,
 * the sequences). BASS's OCTAVE turned with a note held is one: the bass is kept as it sounds. The scene's settings
 * past NSOUND (BASS TONE's WEIGHT, PUNCH, BITE, the bass's SPACE, GLINT's AUTO VIB, LOFI TAPE II's four, the LFOs'
 * AMOUNTs, SWELL) as STORE keeps them; ARP II's
 * LATCH, GLINT's and the bass's, as the FM-1 turns it (off: the arpeggio's notes no key holds go) */
static void eng_edit_param(uint32_t p, uint32_t x)
{
    if (p < NSOUND || p == P_TEMPO) {
        int b = bass_note >= 0 ? bass_sounding(bass_note) : -1;
        param_set((int)p, (int32_t)x);
        if (b >= 0 && bass_note >= 0 && bass_sounding(bass_note) != b)
            keys_edited = 1;
    } else if (p == P_BS_WEIGHT || p == P_BS_PUNCH || p == P_BS_BITE || p == P_BS_SPACE || p == P_GL_VIB_AUTO ||
               (p >= P_TP_FLUTTER && p <= P_TP_DROPOUT) || (p >= P_L1_AMT1 && p <= P_L3_AMT4) || p == P_SWELL) {
        uint8_t was = st.v[p];
        param_set((int)p, (int32_t)x);   /* (the scene's, not its sound's: kept as what it plays is, scene_notes_take) */
        if (st.v[p] != was)
            keys_edited = 1;
    } else if (p == P_GA_LATCH || p == P_BA_LATCH) {
        arp_t *a = p == P_GA_LATCH ? &arp : &barp;
        uint8_t n0 = a->n, l0 = a->latch;
        param_set((int)p, (int32_t)x);   /* (off: only the notes still held go on; none held in the editor: its notes go) */
        if (a->n != n0 || a->latch != l0)
            eng_edits();
    } else if ((p >= P_SQ_STEP && p <= P_SQ_CLEAR && p != P_SQ_REC) || p == P_SQ_NLEN ||
               (((p >= P_BQ_STEP && p <= P_BQ_TRANS) || (p >= P_BQ_NLEN && p <= P_BQ_ACC)) && p != P_BQ_REC) ||
               (p >= P_DQ_ON && p <= P_DQ_STEP && p != P_DQ_REC) || p == P_DQ_NOTES || p == P_DQ_NLEN) {
        static uint8_t d0[SC_DSEQ][DQ_NOTES];
        uint8_t s0[SC_SEQ], n0[SC_BSEQ], q0[NBQ], q1[NBQ], e0[NDQ], e1[NDQ];
        eng_holds_release();
        memcpy(s0, seq, sizeof s0);
        memcpy(n0, bnote, sizeof n0);
        memcpy(d0, dnote, sizeof d0);
        bq_now(q0);
        dq_now(e0);
        param_set((int)p, (int32_t)x);
        bq_now(q1);
        dq_now(e1);
        if (p == P_SQ_STEP)              /* (the step's own, whatever STEP was) */
            seq_view();
        else if (p == P_BQ_STEP)
            bseq_view();
        if (memcmp(s0, seq, sizeof s0) || memcmp(n0, bnote, sizeof n0) || memcmp(q0, q1, sizeof q0) ||
            memcmp(d0, dnote, sizeof d0) || memcmp(e0, e1, sizeof e0))
            eng_edits();                 /* (a step written as it was: nothing edited) */
    }
#if FRS_LEAD
    else if (p >= P_LD_WAVE && p < NPARAM)   /* the LEAD's (card #152: a shard's, eng_lead_get; PRESET a preset's sound,
                                              * MODE its voices let go, as the hand's) */
        param_set((int)p, (int32_t)x);
#endif
}

static void eng_scene_edited(const scene_t *played, scene_t *e)
{
    uint32_t i;
    *e = *played;
    if (keys_edited)
        scene_notes_take(e);
    for (i = 0; i < NSOUND; i++)
        e->v[i] = kept_v(i);
    e->tempo = played->tempo <= PD[P_TEMPO].max ? st.v[P_TEMPO] : played->tempo;
}

/* THE KEYS, LATCH: track t's note n (as it sounds) latched, or let go if it is (1: latched, 0: let go, -1: nothing,
 * full). 0 the drones: the chord HARMONY makes at n, on its root (eight at most, as a scene keeps them); a chord's own
 * root clicked lets it go, whatever HARMONY makes there now; 1 GLINT's
 * arpeggio: n without OCTAVE among its notes (sixteen at most), played from its next step; 2 the bass: n held (another
 * note: it goes there, PULSE keeping the beat), BASS's OCTAVE off it; on ARP, among its arpeggio's notes, as GLINT's */
static int eng_latch_arp(arp_t *a, int b)
{
    int i;
    for (i = 0; i < a->n; i++)
        if (a->held[i] == b) {
            eng_edits();
            arp_remove(a, b);
            if (!a->n)
                arp_stop(a);
            return 0;
        }
    if (a->n >= SC_ARP && (a != &arp || st.v[P_SQ_TYPE]))
        return -1;
    eng_edits();
    if (a == &arp && !st.v[P_SQ_TYPE] && a->n >= RING) {   /* GLINT on KEYS: the oldest goes, the new one in its voice */
        uint32_t o = 0;
        for (i = 1; i < a->n; i++)
            if ((int16_t)(a->in[i] - a->in[o]) < 0)
                o = (uint32_t)i;
        arp_remove(a, a->held[o]);
    }
    arp_add(a, b, 100);
    a->latch = 1;
    a->paused = 0;
    return 1;
}

static int eng_latch(uint32_t t, uint32_t note)
{
    int n = note > 127u ? 127 : (int)note, root, k = 0, i;
    chord_t c;
    eng_holds_release();                 /* (the shard as it is, then the key: as the editor's engine has it) */
    if (t == 0u) {
        chord_build(n, st.v, &c, &root);
        if (bit_get(lit, n) && n != aud_root)
            root = n;
        if (root < 0)                    /* (under the scale's lowest: no root to latch it on) */
            return -1;
        if (bit_get(lit, root) && root != aud_root) {
            eng_edits();
            chord_off(&lat[root]);
            bit_put(lit, root, 0);
            return 0;
        }
        for (i = 0; i < 128; i++)
            k += bit_get(lit, i) && i != aud_root;
        if (k >= (int)SC_CHORDS)
            return -1;
        eng_edits();
        lat[root] = c;
        bit_put(lit, root, 1);
        chord_on(&c, 100);
        dseq_key(c.n, c.k);              /* (RECORD on the drone's sequence: into it, STEP's staying: a set built there) */
        return 1;
    }
    if (t == 1u) {
        int b = n - 12 * (int)st.v[P_GL_OCTAVE];
        return b < 1 ? -1 : eng_latch_arp(&arp, b);
    }
    if (t == 2u) {
        int played = n - 12 * ((int)st.v[P_BS_OCTAVE] - 2);
        if (st.v[P_BQ_ON] == SQT_ARP)
            return played < 1 ? -1 : eng_latch_arp(&barp, played);
        eng_edits();
        if (bass_note >= 0 && bass_sounding(bass_note) == n) {
            bass_stop();
            return 0;
        }
        bass_live(played, 100);
        return 1;
    }
    return -1;
}

/* THE KEYS' CLEAR: every chord latched let go (0), an arpeggio's notes (1 GLINT's; 2 the bass's, on ARP: off ARP they
 * wait for it, as GLINT's), the bass held (2) */
static void eng_clear(uint32_t t)
{
    int i;
    eng_edits();
    if (t == 0u) {
        for (i = 0; i < 128; i++)
            if (bit_get(lit, i)) {
                chord_off(&lat[i]);
                bit_put(lit, i, 0);
            }
    } else if (t == 1u) {
        if (st.v[P_SQ_TYPE] != SQT_SEQ) {   /* (on SEQ its notes wait for it) */
            arp.n = arp.pn = arp.gap = 0;
            arp.dirty = 1;
        }
        arp_stop(&arp);
    } else if (t == 2u) {
        if (st.v[P_BQ_ON] == SQT_ARP) {  /* (the same) */
            barp.n = barp.pn = barp.gap = 0;
            barp.dirty = 1;
            arp_stop(&barp);
        }
        bass_stop();
    }
}

/* THE KEYS, HOLD (the engine playing only: nothing kept): track t's note n (as it sounds) while it is down. The drones
 * the chord HARMONY makes there; GLINT on ARP the arpeggio playing it too (a note it has already: as it was), else the
 * glint itself (the sequence's own note left to it); the bass there, and back to the note it had once let go, or
 * silent (its sequence playing: the sequence goes on) */
/* (hold_c, the drones' chords held: above, with hold_a and hold_ba, GLINT's notes held into the arpeggio, the bass's;
 * hold_b and hold_bn, the bass's note before the one held, the one held) */
static void eng_holds_off(void)          /* (a shard taken, or stopped: what the keys held is the engine's no more) */
{
    memset(hold_c, 0, sizeof hold_c);
    memset(hold_a, 0, sizeof hold_a);
    memset(hold_k, 0, sizeof hold_k);
    memset(hold_ba, 0, sizeof hold_ba);
    hold_b = hold_bn = -1;
}
static void eng_holds_release(void)      /* (everything HOLD holds let go, as its keys would be: back to the shard) */
{
    int n;
    for (n = 0; n < 128; n++) {
        if (hold_c[n].k) {
            chord_off(&hold_c[n]);
            hold_c[n].k = 0;
        }
        if (bit_get(hold_a, n)) {
            bit_put(hold_a, n, 0);
            arp_remove(&arp, n);
            if (!arp.n)
                arp_stop(&arp);
        }
        if (bit_get(hold_ba, n)) {
            bit_put(hold_ba, n, 0);
            arp_remove(&barp, n);
            if (!barp.n)
                arp_stop(&barp);
        }
    }
    if (hold_bn >= 0) {
        if (bass_note == hold_bn && !(st.v[P_BQ_ON] == SQT_SEQ && bq.run)) {
            if (hold_b >= 0)
                bass_live(hold_b, 100);
            else
                bass_stop();
        }
        hold_bn = hold_b = -1;
    }
}
static void eng_hold(uint32_t t, uint32_t note, uint32_t down)
{
    int n = note > 127u ? 127 : (int)note, root, i;
    if (t == 0u) {
        if (down && !hold_c[n].k) {
            if (keys_touch && !st.v[P_KY_DR_HARM]) {   /* (HARMONY OFF: chromatic, as the keys' key_notes) */
                hold_c[n].k = 1;
                hold_c[n].n[0] = (int8_t)n;
            } else {
                chord_build(n, st.v, &hold_c[n], &root);
            }
            chord_on(&hold_c[n], 100);
        } else if (!down && hold_c[n].k) {
            chord_off(&hold_c[n]);
            hold_c[n].k = 0;
        }
    } else if (t == 1u) {
        int b = n - 12 * (int)st.v[P_GL_OCTAVE];
        if (b < 1)
            return;
        if (st.v[P_SQ_TYPE] == SQT_ARP) {
            if (down && !bit_get(hold_a, b)) {
                for (i = 0; i < arp.n && arp.held[i] != b; i++)
                    ;
                if (i == arp.n && arp.n < 16u) {
                    arp_add(&arp, b, 100);
                    arp.paused = 0;
                    bit_put(hold_a, b, 1);
                }
            } else if (!down && bit_get(hold_a, b)) {
                bit_put(hold_a, b, 0);
                arp_remove(&arp, b);
                if (!arp.n)
                    arp_stop(&arp);
            }
        } else if (down) {
            bit_put(hold_k, b, 1);
            frasil_glint_on(b, 100.0f / 127.0f);
        } else {
            bit_put(hold_k, b, 0);
            if (sq.sounding != b && !ring_has(b))
                frasil_glint_off(b);
        }
    } else if (t == 2u && st.v[P_BQ_ON] == SQT_ARP) {
        int b = n - 12 * ((int)st.v[P_BS_OCTAVE] - 2);   /* (as GLINT's: its arpeggio playing it too) */
        if (b < 1)
            return;
        if (down && !bit_get(hold_ba, b)) {
            for (i = 0; i < barp.n && barp.held[i] != b; i++)
                ;
            if (i == barp.n && barp.n < 16u) {
                arp_add(&barp, b, 100);
                barp.paused = 0;
                bit_put(hold_ba, b, 1);
            }
        } else if (!down && bit_get(hold_ba, b)) {
            bit_put(hold_ba, b, 0);
            arp_remove(&barp, b);
            if (!barp.n)
                arp_stop(&barp);
        }
    } else if (t == 2u) {
        int played = n - 12 * ((int)st.v[P_BS_OCTAVE] - 2);
        if (down) {
            if (hold_bn < 0)
                hold_b = bass_note;
            hold_bn = (int8_t)played;
            bass_live(played, 100);
        } else if (hold_bn == played) {
            if (bass_note == played && !(st.v[P_BQ_ON] == SQT_SEQ && bq.run)) {
                if (hold_b >= 0)
                    bass_live(hold_b, 100);
                else
                    bass_stop();
            }
            hold_bn = hold_b = -1;
        }
    }
}

/* ------------------------------------------------------- a layer's preset --- */
/* A user's preset (3.0: the FM-1's, the iPad's UserPresets): a layer's sound kept under a name, not in a scene. The
 * settings each layer's keeps (the iPad's UserPresets.sets): the drone's what a factory preset sets (DRONE_P); GLINT's
 * its voice, its envelope, its echo's share of the cloud, its vibrato; the bass's its MODEL and its sound. Neither LEVEL
 * nor OCTAVE (the mix's and the hand's), nor PRESET (GLINT's, the drone's: what is browsed).
 *   Its bytes (PRESET_BYTES at most): "FRp", its version (1), its layer (0 the drone, 1 GLINT, 2 the bass), n, its name
 * (SC_NAME characters, NUL-padded, as a scene's), then n pairs: a setting's number (frasil_params.h), its value. Taken
 * (eng_preset_apply), each of its layer's settings set as a hand would (param_set: the drone's CUSTOM from them), a
 * setting it has not left as it is, another layer's never touched */
static const uint8_t PRESET_GL[] = {P_GL_RATIO, P_GL_BRIGHT, P_GL_DECAY, P_GL_SPACE, P_GL_RELEASE, P_GL_ATTACK,
                                    P_GL_PING, P_GL_SUSTAIN, P_GL_VIBRATO, P_GL_VIB_AUTO};
static const uint8_t PRESET_BS[] = {P_BS_MODEL, P_BS_DRIVE, P_BS_FILTER, P_BS_RESO, P_BS_GLIDE, P_BS_ATTACK, P_BS_DECAY,
                                    P_BS_SUSTAIN, P_BS_RELEASE, P_BS_PULSE, P_BS_WEIGHT, P_BS_PUNCH, P_BS_BITE,
                                    P_BS_SPACE};
#define PRESET_HEAD (6u + SC_NAME)
#define PRESET_BYTES (PRESET_HEAD + 2u * NDRONE)
#define PRESET_V 1u

static uint32_t preset_set(uint32_t layer, const uint8_t **ps)   /* the layer's settings: how many, *ps them */
{
    if (layer == 0u) {
        *ps = DRONE_P;
        return NDRONE;
    }
    if (layer == 1u) {
        *ps = PRESET_GL;
        return sizeof PRESET_GL;
    }
    *ps = layer == 2u ? PRESET_BS : 0;
    return layer == 2u ? sizeof PRESET_BS : 0u;
}

/* the layer's sound as it sounds now, under `name` (NUL-ended, SC_NAME at most; 0: none), into out: its length, 0 no
 * layer */
static uint32_t eng_preset_capture(uint32_t layer, const char *name, uint8_t *out)
{
    const uint8_t *ps;
    uint32_t n = preset_set(layer, &ps), i;
    if (!n)
        return 0;
    out[0] = 'F';
    out[1] = 'R';
    out[2] = 'p';
    out[3] = PRESET_V;
    out[4] = (uint8_t)layer;
    out[5] = (uint8_t)n;
    memset(out + 6, 0, SC_NAME);
    for (i = 0; name && i < SC_NAME && name[i]; i++)
        out[6u + i] = (uint8_t)name[i];
    for (i = 0; i < n; i++) {
        out[PRESET_HEAD + 2u * i] = ps[i];
        out[PRESET_HEAD + 2u * i + 1u] = st.v[ps[i]];
    }
    return PRESET_HEAD + 2u * n;
}

/* a preset's layer (0 .. 2), -1 not one (its n bytes) */
static int preset_layer(const uint8_t *in, uint32_t n)
{
    if (n < PRESET_HEAD || in[0] != 'F' || in[1] != 'R' || in[2] != 'p' || in[3] < 1u || in[4] > 2u ||
        n < PRESET_HEAD + 2u * in[5])
        return -1;
    return in[4];
}

static int preset_has(uint32_t layer, uint32_t p)   /* setting p is the layer's preset's */
{
    const uint8_t *ps;
    uint32_t n = preset_set(layer, &ps), i;
    for (i = 0; i < n; i++)
        if (ps[i] == p)
            return 1;
    return 0;
}

/* a preset taken: its layer, or -1 (not one: nothing set) */
static int eng_preset_apply(const uint8_t *in, uint32_t n)
{
    int l = preset_layer(in, n);
    uint32_t i;
    if (l < 0)
        return -1;
    for (i = 0; i < in[5]; i++) {
        uint32_t p = in[PRESET_HEAD + 2u * i];
        if (p < NPARAM && preset_has((uint32_t)l, p))
            param_set((int)p, in[PRESET_HEAD + 2u * i + 1u]);
    }
    return l;
}

/* the sound is that preset now (each of its layer's settings as it keeps it): 1, else 0 (-1 not one) */
static int eng_preset_is(const uint8_t *in, uint32_t n)
{
    int l = preset_layer(in, n);
    uint32_t i;
    if (l < 0)
        return -1;
    for (i = 0; i < in[5]; i++) {
        uint32_t p = in[PRESET_HEAD + 2u * i];
        if (p < NPARAM && preset_has((uint32_t)l, p) &&
            st.v[p] != (in[PRESET_HEAD + 2u * i + 1u] > PD[p].max ? PD[p].max : in[PRESET_HEAD + 2u * i + 1u]))
            return 0;
    }
    return 1;
}

/* what sounds now, for a live view (the River's: app_worklet.js {op: "live"}): ENG_LIVE floats, a voice's note -1 when
 * free, its level the envelope (0..1):
 *    0.. 7  the drone's voices' notes      8..15  their levels
 *   16..19  GLINT's voices' notes (OCTAVE on them)   20..23  their levels as heard (0..1: 48 dB from full to gone)
 *   24      the bass's note (OCTAVE on it, folded)   25  its level
 *   26..28  each LFO on (DEPTH over 0 and a TARGET)  29..31  where in its turn (0..1)  32..34  SHAPE's value (-1..1)
 *   35..37  its turns gone by (RANDOM and DRIFT: which turn's value)
 *   38      GLINT's sequence: the step playing, -1 none     39  GLINT's note sounding (its sequence's or the
 *                                                              arpeggio's, as played: before OCTAVE), -1 none
 *   40      the bass's sequence: the step playing, -1 none  41  MIDI clocks into it
 *   42..45  the tracks' meters (drones, bass, glints, the cloud), 0..1     46  the tide on the timbre, -1..1
 *   47      the notes GLINT's arpeggio and sequence have struck (more than at the last frame: those struck since are
 *           the voices struck last, 48..51)
 *   48..51  GLINT's voices: the seconds since each was struck
 *   52      LOFI TAPE's drag under way: how much slower, 0..1 (1: 8 %) */
#define ENG_LIVE 53
static void eng_live(float *out)
{
    frasil_view_t v[FR_NV];
    glint_view_t g[GLINT_NV];
    bass_view_t b;
    float m[4];
    uint32_t i;
    frasil_view(v);
    frasil_glint_view(g);
    frasil_bass_view(&b);
    frasil_meters(m);
    for (i = 0; i < FR_NV; i++) {
        out[i] = (float)v[i].note;
        out[8 + i] = v[i].note < 0 ? 0.0f : v[i].level;
    }
    for (i = 0; i < GLINT_NV; i++) {
        float l = g[i].level;
        out[16 + i] = (float)g[i].note;
        out[20 + i] = g[i].note < 0 || l < 0.004f ? 0.0f : (1.0f + fm_log2f(l) * (1.0f / 8.0f) < 0.0f ? 0.0f : 1.0f + fm_log2f(l) * (1.0f / 8.0f) > 1.0f ? 1.0f : 1.0f + fm_log2f(l) * (1.0f / 8.0f));
        out[48 + i] = g[i].t;
    }
    out[24] = (float)b.note;
    out[25] = b.note < 0 ? 0.0f : b.level;
    for (i = 0; i < NLFO; i++) {
        out[26 + i] = lfo[i].on ? 1.0f : 0.0f;
        out[29 + i] = lfo[i].ph;
        out[32 + i] = lfo[i].out;
        out[35 + i] = (float)lfo[i].cyc;
    }
    out[38] = st.v[P_SQ_TYPE] == SQT_SEQ && sq.run ? (float)sq.pos : -1.0f;
    out[39] = sq.sounding >= 0 ? (float)sq.sounding : arp.sounding >= 0 ? (float)arp.sounding : -1.0f;
    out[40] = st.v[P_BQ_ON] == SQT_SEQ && bq.run ? (float)bq.pos : -1.0f;
    out[41] = bq.t;
    for (i = 0; i < 4u; i++)
        out[42 + i] = m[i];
    out[46] = frasil_tide();
    out[47] = (float)(arp.steps + sq.steps);
    out[52] = frasil_tape_drag() * 12.5f;
}

/* ------------------------------------------------------------- driving it --- */
/* the engine started at the rate fs (Hz): its sound, the drone's presets; no MIDI clock yet (else the first half
 * second would follow one: the grid still, a scene loaded then not from its top) */
static void eng_init(float fs)
{
    grid.clk_at = eng_ms() - 60000u;
    tape_at = eng_ms();
    frasil_init(fs);
    drone_init();
}

/* every state the brain keeps, as at power-on (the FM-1's RAM as it starts): for a runner that starts the engine again
 * (eng_init, eng_power_on to follow; the FM-1 never does) */
static void eng_reset(void)
{
    memset(&st, 0, sizeof st);
    memset(held_by, 0, sizeof held_by);
    memset(lat, 0, sizeof lat);
    memset(mid, 0, sizeof mid);
    memset(lit, 0, sizeof lit);
    memset(mid_on, 0, sizeof mid_on);
    memset(sus, 0, sizeof sus);
    bass_note = -1;
    memset(mgl, 0, sizeof mgl);
    memset(mgs, 0, sizeof mgs);
    memset(mgl_base, 0, sizeof mgl_base);
    memset(mbass, 0, sizeof mbass);
    mb_n = 0;
    memset(key, 0, sizeof key);
    memset(&arp, 0, sizeof arp);
    rung.n = 0;
    arp.sounding = -1;
    arp.rng = 0x9E3779B9u;
    arp.latch = 1;
    arp.step_n = -1;
    memset(&barp, 0, sizeof barp);
    barp.sounding = -1;
    barp.rng = 0x7F4A7C15u;
    barp.latch = 1;
    barp.step_n = -1;
    memset(&grid, 0, sizeof grid);
    memset(seq, 0, sizeof seq);
    memset(&sq, 0, sizeof sq);
    sq.at = -1.0f;
    sq.pos = sq.skip = -1;
    sq.sounding = -1;
    memset(bnote, 0, sizeof bnote);
    memset(&bq, 0, sizeof bq);
    bq.pos = bq.tn = bq.skip = bq.sounding = -1;
    memset(dnote, 0, sizeof dnote);
    memset(&dq, 0, sizeof dq);
    dq.pos = -1;
    dq.rng = 0x2545F491u;
    memset(drec_f, 0, sizeof drec_f);
    memset(saved_dnote, 0, sizeof saved_dnote);
    memset(saved_dq, 0, sizeof saved_dq);
    memset(&dtk, 0, sizeof dtk);
    memset(dq_prev, 0, sizeof dq_prev);
    dq_prev_len = dq_had = 0;
    dq_memo_at = -1;
    count_stop();
    rec_counts = 0;
    metro_free = 0;
    delay_s_now = 0.25f;
#if FRS_LEAD
    ld_delay_s = 0.25f;
    memset(&ldk, 0, sizeof ldk);         /* (its keys and the LEAD as saved: an engine opened again, as a first one) */
    memset(saved_ld, 0, sizeof saved_ld);
#endif
    memset(saved_v, 0, sizeof saved_v);
    memset(saved_seq, 0, sizeof saved_seq);
    memset(saved_bnote, 0, sizeof saved_bnote);
    memset(saved_bq, 0, sizeof saved_bq);
    saved_bsp = 0;
    memset(saved_tp, 0, sizeof saved_tp);
    memset(saved_amt, 0, sizeof saved_amt);
    saved_sw = 0;
    tape_l2 = tape_sent = 0.0f;
    tape_tk = 1.0f;
    tape_clocked = 0;
    tape_fresh = 1;
    memset(&bp, 0, sizeof bp);
    memset(custom, 0, sizeof custom);
    memset(seq_prev, 0, sizeof seq_prev);
    seq_had = 0;
    memset(bq_prev, 0, sizeof bq_prev);
    bq_had = 0;
    memset(&mo, 0, sizeof mo);
    aud_root = -1;
    memset(&rst, 0, sizeof rst);
    rst_valid = 0;
    memset(lfo, 0, sizeof lfo);
    lfo_at = 0;
    lfo_grid = 0.0f;
    lfo_any = lfo_restart = 0;
    memset(&mtr, 0, sizeof mtr);
    mtr.at = -1.0f;
    mtr.t = -2.0f;
    memset(&gtk, 0, sizeof gtk);
    memset(&btk, 0, sizeof btk);
    memset(&spk, 0, sizeof spk);
    spk.note[0] = spk.note[1] = -1;
    keys_edited = 0;
    memset(hold_c, 0, sizeof hold_c);
    memset(hold_a, 0, sizeof hold_a);
    memset(hold_k, 0, sizeof hold_k);
    memset(hold_ba, 0, sizeof hold_ba);
    hold_b = hold_bn = -1;
    seq_shift = bseq_shift = 0;
    memset(morph_off, 0, sizeof morph_off);
    memset(mod, 0, sizeof mod);
    memset(mod_tp, 0, sizeof mod_tp);
#if FRS_LEAD
    memset(mod_ld, 0, sizeof mod_ld);
#endif
    morph_at = swell_at = 0;
}

/* the state as at power-on: nothing latched, held or to undo, every setting at its default, the arpeggio latched and
 * empty (then a sound, or a scene: the runner's) */
static void eng_power_on(void)
{
    uint32_t i;
    count_stop();
    st.metro = 1;
    st.glint = st.bass = 0;
    st.compare = st.rec_took = st.sustain = 0;
    rst_valid = seq_had = bq_had = dq_had = 0;   /* (nothing to restore or undo yet; the FM-1's simulator's reboot keeps
                                                  * statics) */
    mo.on = 0;
    memset(morph_off, 0, sizeof morph_off);
    arp.latch = barp.latch = 1;          /* the arpeggios latched from the start, empty */
    arp.n = arp.pn = barp.n = barp.pn = arp.gap = barp.gap = 0;
    arp.dirty = barp.dirty = 0;
    bass_note = -1;
    sq.run = 0;
    bq.run = 0;
    bq.tn = bq.skip = -1;
    dq.run = dq.wait = dq.nsnd = dq.keys = 0;
    dq.pos = -1;
    for (i = 0; i < NPARAM; i++)
        st.v[i] = PD[i].def;
    st.metro = st.v[P_METRO] >= 1u;      /* (METRONOME REC: a take's ticks) */
    metro_free = st.v[P_METRO] == 2u;
    dseq_view();
}

/* every millisecond, before the MIDI that came in since the last: the grid's (the arpeggiator or GLINT's sequence, the
 * bass's sequence, PULSE after it: a NOTE beginning on the division first; the delay on the beat) */
static void tape_run(void)              /* TAPE SPEED: some 120 ms a time constant (a tape slowing, never a jump) */
{
    uint32_t now = eng_ms(), dt = now - tape_at;
    float to = tape_octf((float)st.v[P_TP_FLUTTER] + mod_of(P_TP_FLUTTER)), d = to - tape_l2;   /* (an LFO on it
                                                                             * too: glided as the knob is) */
    uint8_t clocked = (uint8_t)grid_clocked(now);
    tape_at = now;
    if (d != 0.0f) {
        float k = tape_fresh ? 1.0f : (float)(dt > 120u ? 120u : dt) * (1.0f / 120.0f);
        tape_l2 = fm_fabsf(d) < 0.002f ? to : tape_l2 + d * k;
        if (fm_fabsf(tape_l2 - tape_sent) >= 0.01f || tape_l2 == to)   /* (the engine glides between) */
            st.engine_dirty = 1;
    }
    if (clocked != tape_clocked && tape_l2 != 0.0f)   /* (the times' factor to 1, or back) */
        st.engine_dirty = 1;
    tape_clocked = clocked;
    tape_fresh = 0;
    tape_tk = clocked || tape_l2 == 0.0f ? 1.0f : fm_exp2f(tape_l2);
}

static void eng_steps(void)
{
    tape_run();
    st.v[P_GA_LATCH] = arp.latch;        /* (the settings that show a state, as it is: ARP held, frs_metronome, REC held) */
    st.v[P_BA_LATCH] = barp.latch;
    st.v[P_METRO] = (uint8_t)(metro_free ? 2u : st.metro ? 1u : 0u);
    take_run(&gtk, st.v[P_SQ_REC] == 2u && st.v[P_SQ_TYPE] == SQT_SEQ && sq.run, seq_len());   /* (a take ended: stopped,
                                                                                              * RECORD off LIVE) */
    take_run(&btk, st.v[P_BQ_REC] == 2u && st.v[P_BQ_ON] == SQT_SEQ && bq.run, st.v[P_BQ_STEPS] + 1u);
    take_run(&dtk, st.v[P_DQ_REC] == 2u && st.v[P_DQ_ON] && dq.run, dseq_len());
    arp_run();
    bseq_run();
    dseq_run();
    pulse_run();
    delay_sync();
}

/* then, after it: PUMP on the beat (the clocks just come in), the LFOs, SPARKLE, the metronome, MORPH */
static void eng_moves(void)
{
    pump_sync();
    lfo_run();
    sparkle_run();
    metro_run();
    morph_run();
    if (count_own)                       /* (COUNT-IN: the FM-1's here, the engine alone's in its own place) */
        count_run();
}

#include "live.c"                        /* the running state Perform LIVE tells (live_save, live_load: card #131) */
