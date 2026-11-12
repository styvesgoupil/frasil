/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's engine, alone (frasil_engine.h): the brain (core.c) with hooks of its own, its milliseconds and its blocks
 * run as the FM-1's simulator runs them (base/host/plat_host.c: at each millisecond, the blocks due, then the main
 * loop's pass), so that the same MIDI at the same milliseconds renders the same samples. The sound, the DSP's units,
 * compiled beside it. */
#include "frasil_engine.h"
#include "core.c"
#include "dsp.h"                         /* (BR_FS_MAX, BR_FS_CONST) */

_Static_assert(sizeof(scene_t) == FRS_SCENE_BYTES && FRS_KEYS == FRS_NKEYS && ENG_LIVE == FRS_LIVE,
               "frasil_engine.h's sizes are the engine's");
_Static_assert(offsetof(scene_t, name_more) == 590u && offsetof(scene_t, lfo_amt) == 606u &&
                   offsetof(scene_t, swell) == FRS_SCENE_BYTES - 1u && P_L3_AMT4 + 1u - P_L1_AMT1 == SC_LFO_AMT,
               "\"FRS}\": the layout README.md has (the stream's SWELL its last byte)");
_Static_assert(sizeof(((frs_view_t *)0)->dnote) == sizeof dnote && sizeof(((frs_view_t *)0)->dq_snd) == sizeof dq.snd &&
                   FRS_DQ_STEPS == SC_DSEQ && FRS_DQ_NOTES == DQ_NOTES && FRS_DQ_TIE == DQ_TIE,
               "frs_view_t holds the drone's sequence");
_Static_assert(sizeof(((frs_view_t *)0)->seq) == SC_SEQ && sizeof(((frs_view_t *)0)->bnote) == SC_BSEQ &&
                   sizeof(((frs_view_t *)0)->arp) == SC_ARP &&
                   sizeof(((frs_view_t *)0)->arp) == sizeof arp.held && sizeof(((frs_view_t *)0)->latched) == sizeof lit &&
                   sizeof(((frs_view_t *)0)->kept) == sizeof lit,
               "frs_view_t holds what the engine has");

/* ---------------------------------------------------------------- hooks --- */
static uint64_t now64;                   /* the millisecond the engine runs next (its tick to come) */
static void (*say_cb)(void *, const char *, const char *);
static void (*program_cb)(void *, uint32_t);
static void *cb_user;
static uint8_t trying;

static uint32_t eng_ms(void) { return (uint32_t)now64; }   /* (the brain's sums wrap; the clock here does not) */
static void eng_say(const char *a, const char *b)
{
    if (say_cb)
        say_cb(cb_user, a ? a : "", b ? b : "");
}
static int eng_trying(uint32_t track) { return track == 1u && trying; }
static void eng_type_changed(void) {}
static int eng_cc(uint32_t cc, uint32_t v)
{
    (void)cc;
    (void)v;
    return 0;
}
static void eng_program(uint32_t n)
{
    if (program_cb)
        program_cb(cb_user, n);
}
static void eng_setting(int p, int32_t x)   /* (SPK/JACK, ANIMATIONS: the FM-1's; CLOCK: the brain's own) */
{
    (void)p;
    (void)x;
}

/* ----------------------------------------------------------- the tracks --- */
/* a track stopped by frs_track_play, a moment: the drones' latched chords let go and kept (by root; lat[] keeps their
 * notes), the bass's latched note let go and kept (as played); the arpeggios waiting, the sequences stopped (the
 * engine's own state says it: sq.run, arp.paused, barp.paused, bq.run). Kept nowhere: a scene stored meanwhile has the kept notes
 * (notes_in) */
static struct {
    uint8_t stop[3];                     /* stopped by the player (a MIDI Start, a host's, leaves it so) */
    uint32_t kept[4];                    /* the drones' chords kept, by root */
    int8_t bass;                         /* the bass's note kept, as played (OCTAVE off it); -1 none */
} tr = {.bass = -1};
static int8_t hold_g[128];               /* GLINT's notes a HOLD sounds itself while its arpeggio is stopped: by the
                                          * note as it came (its glint as it was struck), -1 none */
static int8_t hold_bg = -1;              /* the bass's the same (its note as played), one at a time */

static void tr_reset(void)               /* no track stopped, nothing kept (a scene taken, PANIC, RELEASE ALL) */
{
    memset(&tr, 0, sizeof tr);
    tr.bass = -1;
}

static int bass_on_seq(void) { return st.v[P_BQ_ON] == SQT_SEQ && bseq_has_notes(); }   /* (an empty one's: the keys') */
static int drone_on_seq(void) { return st.v[P_DQ_ON] && dseq_has_notes(); }   /* (the drone's sequence, with notes) */

static int tr_playing(uint32_t t)
{
    if (t == 1u)
        return st.v[P_SQ_TYPE] == SQT_SEQ ? sq.run || sq.restart
               : arp_plays() ? !arp.paused : !tr.stop[1];
    if (t == 2u)
        return st.v[P_BQ_ON] == SQT_ARP ? !barp.paused : bass_on_seq() ? bq.run : !tr.stop[2];
    return drone_on_seq() ? dq.run : !tr.stop[0];
}

/* bass note n is held, not latched: MIDI's top note, a key down in KEY MODE HOLD (THE KEYS' HOLD: bass_latched) */
static int bass_held_by(int n)
{
    uint32_t k;
    if (mb_n && mbass[mb_n - 1] == n)
        return 1;
    for (k = 0; k < FRS_NKEYS; k++)
        if (key[k].kind == K_BASS && key[k].hold && key[k].root == n)
            return 1;
    return 0;
}
static int bass_held(void) { return (hold_bn >= 0 && bass_note == hold_bn) || bass_held_by(bass_note); }

/* the bass's latched note (THE KEYS' HOLD over it: the note under, if that one is latched), or -1: what stopping the
 * bass keeps */
static int bass_latched(void)
{
    if (hold_bn >= 0 && bass_note == hold_bn)
        return hold_b >= 0 && !bass_held_by(hold_b) ? hold_b : -1;
    return bass_note >= 0 && !bass_held() ? bass_note : -1;
}

/* a track the player started by other means (a chord or a note latched, GLINT's SEQ turned, the bass's SEQUENCE on,
 * RECORD): it plays, what it kept gone. After the calls that may do it (not after MIDI: its Start starts nothing
 * stopped, tr_hold) */
static void drones_again(void);
static void tr_sync(void)
{
    if (tr.stop[0] && (lit[0] | lit[1] | lit[2] | lit[3])) {
        tr.stop[0] = 0;
        memset(tr.kept, 0, sizeof tr.kept);
    }
    if (tr.stop[0] && st.v[P_DQ_ON] && dq.run) {   /* (the drone's sequence begun: the drones play, their kept chords
                                                   * with it) */
        tr.stop[0] = 0;
        drones_again();
    }
    if (tr.stop[1] && (st.v[P_SQ_TYPE] != 0u || arp.n) && tr_playing(1))
        tr.stop[1] = 0;
    if (tr.stop[2] && (st.v[P_BQ_ON] == SQT_ARP ? !barp.paused
                       : bass_on_seq() ? bq.run : bass_latched() >= 0 && hold_bn < 0)) {
        tr.stop[2] = 0;                  /* (its arpeggio going, its sequence running, or a note latched: not one held
                                          * while down) */
        tr.bass = -1;
    }
}

/* a stopped track kept so through what starts the others (MIDI Start, a host's start or locate: seqs_top) */
static void tr_hold(void)
{
    if (tr.stop[1] && st.v[P_SQ_TYPE] == SQT_SEQ && (sq.run || sq.restart)) {
        sq.restart = 0;
        seq_note_off();
        sq.run = 0;
    }
    if (tr.stop[2] && st.v[P_BQ_ON] == SQT_SEQ && bq.run)
        bseq_stop();
    if (tr.stop[0] && st.v[P_DQ_ON] && dq.run)
        dseq_stop();
}

/* where each sequence began (its first step) on the grid unwrapped, the clocks since the open: a sequence started
 * again goes where it would be had it begun with the other (glint_join, bass_join), whatever their loops' lengths.
 * Read every millisecond, after the brain (seq_origins): where it is in its loop against where its origin says it
 * would be; apart (begun again: a Start, PLAY, a scene, a key's restart on a beat; its loop changed), its origin is
 * its loop's turn under way */
static double g_un;                      /* the grid unwrapped */
static float g_was;
static struct {
    double at;                           /* its first step, on g_un */
    uint8_t on;                          /* known: it plays, from there */
} org_gl, org_bs;
static uint8_t gl_joined;                /* GLINT's joined the bass's (glint_join): its origin kept to its first step */

static double glint_loop(void) { return (double)seq_len() * (double)ARP_TICKS[st.v[P_ARP_RATE] % 10u]; }
static double bass_loop(void) { return (double)(st.v[P_BQ_STEPS] + 1u) * (double)bseq_ticks(); }
static double into(double e, double loop) { return e > 0.0 ? e - loop * (double)(uint64_t)(e / loop) : 0.0; }

/* org kept if `a` (the clocks into its loop now) is where it says, else made the turn under way's */
static void org_check(double a, double loop, uint8_t *on, double *at)
{
    double d;
    if (*on) {
        d = a - into(g_un - *at, loop);
        if (d > 0.5 * loop)
            d -= loop;
        else if (d < -0.5 * loop)
            d += loop;
        if (d < 0.6 && d > -0.6)
            return;
    }
    *at = g_un - a;
    *on = 1;
}

/* before the brain's millisecond: a sequence about to begin again (a Start, PLAY, a scene, RECORD: sq.restart,
 * seq_begin's, bseq_begin's wait) has no origin till it does; the grid moved since the last (grid_top: a call between
 * two milliseconds) is no time gone by */
static void org_drop(void)
{
    if (sq.restart || (sq.run && sq.pos < 0 && !gl_joined))
        org_gl.on = 0;
    if (bq.run && (bq.wait || bq.pos < 0))
        org_bs.on = 0;
    g_was = grid.tick;
}

/* after the brain's steps: the grid as they moved it (round its 9600 too) */
static void grid_count(void)
{
    float dg = grid.tick - g_was;
    if (dg < 0.0f)
        dg += 9600.0f;
    if (dg < 200.0f)                     /* (a millisecond's: a few clocks, a burst of MIDI's at most) */
        g_un += (double)dg;
    g_was = grid.tick;
}

/* at the millisecond's end (what moved the grid after the steps, a Start, a locate, PLAY: a jump, no time) */
static void seq_origins(void)
{
    uint32_t ticks = ARP_TICKS[st.v[P_ARP_RATE] % 10u], len = seq_len();
    g_was = grid.tick;
    if (st.v[P_SQ_TYPE] == SQT_SEQ && sq.run) {
        if (sq.pos >= 0) {               /* (begun: its first step played; joined, before it: as glint_join set it) */
            gl_joined = 0;
            int32_t k = (sq.n - sq.start) % (int32_t)len;
            if (k < 0)
                k += (int32_t)len;
            org_check((double)k * (double)ticks + (double)(seq_grid(grid.tick) - (float)sq.n * (float)ticks),
                      glint_loop(), &org_gl.on, &org_gl.at);
        }
    } else {
        org_gl.on = gl_joined = 0;
    }
    if (st.v[P_BQ_ON] == SQT_SEQ && bseq_playing())
        org_check((double)bseq_at(), bass_loop(), &org_bs.on, &org_bs.at);
    else
        org_bs.on = 0;
}

static void tr_stop(uint32_t t)
{
    uint32_t r;
    int b;
    tr.stop[t] = 1;
    if (t == 0u) {
        for (r = 0; r < 128u; r++)
            if (bit_get(lit, (int)r)) {
                chord_off(&lat[r]);      /* (it rings out: its RELEASE) */
                bit_put(lit, (int)r, 0);
                bit_put(tr.kept, (int)r, 1);
            }
        if (st.v[P_DQ_ON])
            dseq_stop();                 /* (its sequence too: its notes let go, ringing out) */
    } else if (t == 1u) {
        if (st.v[P_SQ_TYPE] == SQT_SEQ) {
            sq.restart = 0;
            sq.at = -1.0f;
            seq_note_off();
            sq.run = 0;
            gl_joined = 0;
        } else if (arp_plays()) {
            arp.paused = 1;              /* (as PLAY has it: waiting, its notes kept) */
            arp_stop(&arp);
        }
    } else {
        if (st.v[P_BQ_ON] == SQT_SEQ)
            bseq_stop();                 /* (a LIVE take under way: closed, its note written, as PLAY closes it) */
        if (st.v[P_BQ_ON] == SQT_ARP) {
            arp_wait(&barp, 1);          /* (as GLINT's: waiting, its notes kept) */
        } else if (!bass_on_seq() && (b = bass_latched()) >= 0) {
            tr.bass = (int8_t)b;         /* the latched note kept; one held (MIDI's, a key's in HOLD) sounds on */
            if (hold_bn >= 0 && bass_note == hold_bn) {
                hold_b = -1;             /* (a HOLD over it: let go, it brings nothing back) */
            } else {
                bass_stop();
                if (mb_n)                /* (MIDI still holding one under it: that one again) */
                    bass_live(mbass[mb_n - 1], 100);
            }
        }
    }
}

/* GLINT's sequence again, from the grid's next step: in phase with the bass's sequence playing (where its steps would
 * be had it begun with the bass's), else from its first step (seq_begin) */
static void glint_join(void)
{
    uint32_t ticks = ARP_TICKS[st.v[P_ARP_RATE] % 10u];
    float top;
    if (!(st.v[P_BQ_ON] == SQT_SEQ && bseq_playing() && org_bs.on)) {
        seq_begin();
        return;
    }
    /* (its loop's turn began there on the grid, had it begun with the bass's: a clock) */
    top = fm_floorf((float)((double)grid.tick - into(g_un - org_bs.at, glint_loop())) + 0.5f);
    while (top < 0.0f)
        top += 9600.0f;
    while (top >= 9600.0f)
        top -= 9600.0f;
    seq_note_off();
    sq.run = 1;
    sq.restart = 0;
    sq.at = -1.0f;
    sq.off = (uint8_t)((uint32_t)top % ticks);
    sq.start = (int32_t)fm_floorf(seq_grid(top) / (float)ticks + 0.5f);
    sq.n = seq_stepn(grid.tick, ticks);   /* (the step under way: it plays from the next) */
    sq.pos = sq.skip = -1;
    org_gl.at = org_bs.at;               /* (begun with the bass's) */
    org_gl.on = 1;
    gl_joined = 1;
    take_new(&gtk);
}

/* the bass's sequence again, from the grid's next step: in phase with GLINT's sequence playing (as glint_join), else
 * from its first step (bseq_begin) */
static void bass_join(void)
{
    if (!(st.v[P_SQ_TYPE] == SQT_SEQ && sq.run && sq.pos >= 0 && org_gl.on)) {
        bseq_begin();
        return;
    }
    bseq_close();
    bq.run = 1;
    bq.wait = 0;
    bq.skip = -1;
    bq.g = grid.tick;
    take_new(&btk);
    bseq_seek((float)into(g_un - org_gl.at, bass_loop()));   /* (its step under way: silent till the next, but on
                                                               * its start) */
    if (bq.t < 0.5f)
        bseq_step(bq.pos);
    org_bs.at = org_gl.at;               /* (begun with GLINT's) */
    org_bs.on = 1;
}

/* the bass's kept note again: over a note MIDI holds (as it was latched over it), under a HOLD (back once it lets go) */
static void bass_again(void)
{
    if (!bass_on_seq() && tr.bass >= 0) {
        if (hold_bn >= 0 && bass_note == hold_bn)
            hold_b = tr.bass;
        else if (bass_note < 0 || bass_held())
            bass_live(tr.bass, 100);
    }
    tr.bass = -1;
}

static void drones_again(void)           /* the drones' kept chords latched again */
{
    uint32_t r;
    for (r = 0; r < 128u; r++)
        if (bit_get(tr.kept, (int)r) && !bit_get(lit, (int)r)) {
            bit_put(lit, (int)r, 1);
            chord_on(&lat[r], 100);      /* (at 100, as a scene's chords are) */
        }
    memset(tr.kept, 0, sizeof tr.kept);
}

static void tr_start(uint32_t t)
{
    tr.stop[t] = 0;
    if (t == 0u) {
        drones_again();
        if (st.v[P_DQ_ON] && !dq.run)
            dseq_begin();                /* (its sequence from its first step, at the grid's next) */
    } else if (t == 1u) {
        if (st.v[P_SQ_TYPE] == SQT_SEQ && !sq.run && !sq.restart)
            glint_join();
        else if (arp_plays() && arp.paused) {
            arp_wait(&arp, 0);           /* (its pattern from the top, at its next step) */
        }
    } else {
        if (st.v[P_BQ_ON] == SQT_SEQ && !bq.run)
            bass_join();
        else if (st.v[P_BQ_ON] == SQT_ARP && barp.paused)
            arp_wait(&barp, 0);          /* (its pattern from the top, at its next step) */
        bass_again();
    }
}

/* what a stopped track keeps goes with what lets the latched notes go: KEY MODE turned, as on the FM-1 (core.c
 * keymode_gone); KEYS' LATCH turned off, the layer's (latch_gone: the drones' chords, the bass's note) */
static void fingers_forget_drones(void), fingers_forget_bass(void);
static void tr_keys(void)
{
    if (keymode_gone) {
        memset(tr.kept, 0, sizeof tr.kept);
        tr.bass = -1;
        fingers_forget_drones();         /* (release_all let their notes go) */
        fingers_forget_bass();
    }
    if (latch_gone & 1u)
        memset(tr.kept, 0, sizeof tr.kept);
    if (latch_gone & 4u)
        tr.bass = -1;
    keymode_gone = latch_gone = 0;
}

/* ------------------------------------------------- THE KEYS' HOLD, fingers --- */
/* On an iPad's keys several fingers hold at once (frs_hold): a note two fingers hold lets go at the last; the bass's
 * held notes in the order they came, the last still down playing when the top one lets go. A latch, CLEAR or an edit
 * lets go what it acts on alone, what the fingers hold sounding on: the River's editor lets the holds go first (its
 * silent engine never had them: core.c's eng_holds_release), here they are hidden from it (holds_hide) */
static uint8_t fingers[3][128];          /* the fingers down on each note (as it came), by track */
static int8_t fnote[3][128];             /* each one's note as the engine took it at its down (GLINT's without OCTAVE,
                                          * the bass's as played): its up the same, OCTAVE turned meanwhile or not */
static int8_t bass_fingers[16];          /* the bass's notes held (as played), the last on top */
static uint32_t nbf;
static uint32_t hold_rec[2][4];          /* the notes (as they came) a finger's down put into a take: GLINT's, the bass's */
static uint8_t holds_off;               /* (frs_hold_off under way: its ups in note order, no legato from one to the next) */
static struct {
    chord_t c[128];
    uint32_t a[4], ba[4];
    int8_t bn, b;
} hid;

static void holds_hide(void)
{
    memcpy(hid.c, hold_c, sizeof hid.c);
    memset(hold_c, 0, sizeof hold_c);
    memcpy(hid.a, hold_a, sizeof hid.a);
    memset(hold_a, 0, sizeof hold_a);
    memcpy(hid.ba, hold_ba, sizeof hid.ba);
    memset(hold_ba, 0, sizeof hold_ba);
    hid.bn = hold_bn;
    hid.b = hold_b;
    hold_bn = hold_b = -1;
}
static void holds_show(void)
{
    memcpy(hold_c, hid.c, sizeof hold_c);
    memcpy(hold_a, hid.a, sizeof hold_a);
    memcpy(hold_ba, hid.ba, sizeof hold_ba);
    hold_bn = hid.bn;
    hold_b = hid.b;
}

/* what the fingers held is the engine's no more (a scene taken, PANIC, RELEASE ALL: let go with the rest) */
static void fingers_forget(void)
{
    eng_holds_off();
    memset(fingers, 0, sizeof fingers);
    memset(hold_g, -1, sizeof hold_g);
    memset(hold_rec, 0, sizeof hold_rec);
    hold_bg = -1;
    nbf = 0;
    memset(drec_f, 0, sizeof drec_f);    /* (and what they wrote on STEP: nothing down) */
    dq.keys = 0;
}

/* the drones' or the bass's alone (what lets their notes go beside: KEY MODE turned, All Notes Off on the drones'
 * channel; held_by's counts made from nothing there) */
static void fingers_forget_drones(void)
{
    memset(hold_c, 0, sizeof hold_c);
    memset(fingers[0], 0, sizeof fingers[0]);
    memset(drec_f, 0, sizeof drec_f);
}
static void fingers_forget_bass(void)
{
    hold_bn = hold_b = -1;
    memset(hold_ba, 0, sizeof hold_ba);
    memset(fingers[2], 0, sizeof fingers[2]);
    memset(hold_rec[1], 0, sizeof hold_rec[1]);
    nbf = 0;
}

static int bass_played(int n) { return n - 12 * ((int)st.v[P_BS_OCTAVE] - 2); }   /* (as THE KEYS' bass note sounds) */

static void bass_unfinger(int p)         /* p (as played) off the fingers' notes, wherever it is */
{
    uint32_t i;
    for (i = 0; i < nbf && bass_fingers[i] != (int8_t)p; i++)
        ;
    if (i == nbf)
        return;
    memmove(bass_fingers + i, bass_fingers + i + 1, nbf - i - 1u);
    nbf--;
}

/* a finger on the bass's note n (as it sounds), or off it: the top one let go, the last still down plays again (a note
 * latched over them meanwhile sounding on); the last let go, back to what was under them (eng_hold) */
static void bass_hold(uint32_t n, int down)
{
    int top, p;
    if (down) {
        p = bass_played((int)n);
        fnote[2][n] = (int8_t)p;
        bass_unfinger(p);
        if (nbf == sizeof bass_fingers) {   /* (full, ups lost: the oldest goes) */
            memmove(bass_fingers, bass_fingers + 1, nbf - 1u);
            nbf--;
        }
        bass_fingers[nbf++] = (int8_t)p;
        eng_hold(2, n, 1);               /* (the note under them kept at the first: hold_b) */
        return;
    }
    p = fnote[2][n];
    top = nbf && bass_fingers[nbf - 1u] == (int8_t)p;
    bass_unfinger(p);
    if (!top)
        return;                          /* (another finger's note plays on) */
    if (nbf) {
        int q = bass_fingers[nbf - 1u];
        if (hold_bn >= 0 && bass_note == hold_bn)
            bass_live(q, 100);
        hold_bn = (int8_t)q;
        return;
    }
    eng_hold(2, (uint32_t)(p + 12 * ((int)st.v[P_BS_OCTAVE] - 2)), 0);   /* (its note at the down, as played) */
}

/* the kept notes in what plays, for a scene stored meanwhile (scene_notes_take), then out again: what the fingers hold
 * out of it (the arpeggio's held notes, the bass's held note: the one under it) */
static uint32_t lit_was[4];
static int8_t bass_was, arp_was[16], barp_was[16];
static uint8_t narp_was, nbarp_was;
static void notes_in(void)
{
    uint32_t i, k = 0;
    memcpy(lit_was, lit, sizeof lit_was);
    bass_was = bass_note;
    memcpy(arp_was, arp.held, sizeof arp_was);
    narp_was = arp.n;
    for (i = 0; i < narp_was; i++)
        if (!(arp.held[i] >= 0 && bit_get(hold_a, arp.held[i])))
            arp.held[k++] = arp_was[i];
    arp.n = (uint8_t)k;
    memcpy(barp_was, barp.held, sizeof barp_was);
    nbarp_was = barp.n;
    for (i = k = 0; i < nbarp_was; i++)
        if (!(barp.held[i] >= 0 && bit_get(hold_ba, barp.held[i])))
            barp.held[k++] = barp_was[i];
    barp.n = (uint8_t)k;
    if (hold_bn >= 0 && bass_note == hold_bn)
        bass_note = hold_b;
    for (i = 0; i < 4u; i++)
        lit[i] |= tr.kept[i];
    if (tr.bass >= 0 && bass_note < 0)
        bass_note = tr.bass;
}
static void notes_out(void)
{
    memcpy(lit, lit_was, sizeof lit);
    bass_note = bass_was;
    memcpy(arp.held, arp_was, sizeof arp_was);
    arp.n = narp_was;
    memcpy(barp.held, barp_was, sizeof barp_was);
    barp.n = nbarp_was;
}

/* ------------------------------------------------------------ the clock --- */
#define MQ_N 256u                        /* MIDI messages waiting for their millisecond */
static struct {
    uint32_t pkt;
    uint64_t at;                         /* the millisecond it is taken at (0: the next one) */
} mq[MQ_N];
static uint32_t mq_r, mq_w;
static double due_ms, block_ms;          /* the next block's time; a block's length */
static uint64_t out_frames;              /* the frames rendered out so far */
static int32_t blk[2u * FRS_BLOCK];
static uint32_t blk_at = FRS_BLOCK;      /* the frames of blk read out */
static float out_gain = 1.0f;
static uint8_t swell_held, open_now;
static scene_t taken_sc;                 /* the scene given last (taken, played, stored): RESTORE's notes */
static struct {                          /* the host's tempo and transport (frs_host_tempo) */
    double bpm, beat;                    /* at frame `frame` */
    uint64_t frame;
    uint8_t on, playing, was;            /* a tempo given; playing; playing at the last tick */
    int64_t clocks;                      /* the MIDI clocks sent for it since it began */
} host;

static void snap_write(void), snap_none(void);   /* (frs_snapshot's, below) */
static void fe_count_hush(void), play_all(void);   /* (COUNT-IN's, by frs_play_all, frs_record_layer) */

/* the host's clocks due at this millisecond, as MIDI's: Start as it begins, or as it locates (more than two clocks
 * either way: no burst of clocks to catch up, each a period of nothing to the tempo the clocks make); then a clock each
 * 24th of a beat. The clocks' period and the last one's time from the host's beat, in place of what the clocks make of
 * them: its tempo at once, and between clocks the grid where the host is */
static void host_clock(void)
{
    double b, ms;
    int64_t c;
    if (!host.on || !host.playing || st.v[P_CLOCK]) {   /* (CLOCK INTERNAL: TEMPO's grid, a host's as a clock's) */
        host.was = 0;
        return;
    }
    ms = 2500.0 / host.bpm;              /* (a clock's period: 2.5 .. 192 ms) */
    b = 24.0 * (host.beat + ((double)now64 - (double)host.frame * 1000.0 / (double)FRASIL_FS) * host.bpm / 60000.0);
    c = (int64_t)b;                      /* (the clocks gone by: b's floor; b well inside, frs_host_tempo's clamp) */
    if ((double)c > b)
        c--;
    if (!host.was || c < host.clocks - 2 || c > host.clocks + 2) {
        eng_midi(0xFAu << 8);
        host.clocks = c;
    }
    for (; host.clocks < c; host.clocks++)
        eng_midi(0xF8u << 8);
    grid.clk_ms = (float)ms;
    grid.clk_at = eng_ms() - (uint32_t)((b - (double)c) * ms);
    host.was = 1;
}

/* a millisecond of the brain, as the FM-1's main loop runs it (app/app.c input: the grid's steps, the MIDI that came
 * in, the rest, the swell, the sound's settings pushed); frs_tick_hook's first */
static void (*tick_fn)(void *);
static void *tick_user;
void frs_tick_hook(void (*fn)(void *), void *user)
{
    tick_fn = fn;
    tick_user = user;
}

static void eng_tick(void)
{
    if (tick_fn)
        tick_fn(tick_user);
    org_drop();
    eng_steps();
    grid_count();
    count_run();
    while (mq_r != mq_w && mq[mq_r % MQ_N].at <= now64) {
        uint32_t pkt = mq[mq_r % MQ_N].pkt;
        eng_midi(pkt);
        if ((pkt >> 8 & 0xF0u) == 0xB0u && (pkt >> 16 & 0x7Fu) == 120u) {
            tr_reset();                  /* (All Sound Off, any channel: as RELEASE ALL, nothing kept, the fingers
                                          * forgotten) */
            fingers_forget();
        } else if ((pkt >> 8 & 0xF0u) == 0xB0u && (pkt >> 16 & 0x7Fu) == 123u && on_ch(P_CANAL, (pkt >> 8 & 15u) + 1u)) {
            tr.stop[0] = 0;              /* (All Notes Off on the drones' channel: their latched chords let go, the
                                          * kept ones too, the fingers' there) */
            memset(tr.kept, 0, sizeof tr.kept);
            fingers_forget_drones();
        }
        mq_r++;
    }
    host_clock();
    tr_hold();                           /* (a Start, the host's: a stopped track stays so) */
    eng_moves();
    seq_origins();
    eng_swell(swell_held);
    if (st.engine_dirty)
        engine_push();
}

/* the next block: the milliseconds before it run (the blocks due at a millisecond rendered before its tick) */
static float out_pk[2], out_ms[2];        /* the output's peak (falling) and mean square (some 300 ms), each side */
static void out_levels(const int32_t *lr)
{
    float pk[2] = {0.0f, 0.0f}, sq[2] = {0.0f, 0.0f}, k = (float)FRS_BLOCK / (0.3f * FRASIL_FS);
    uint32_t i, c;
    for (i = 0; i < FRS_BLOCK; i++)
        for (c = 0; c < 2u; c++) {
            float x = (float)lr[2u * i + c] * (1.0f / 8388608.0f), a = x < 0.0f ? -x : x;
            pk[c] = a > pk[c] ? a : pk[c];
            sq[c] += x * x;
        }
    for (c = 0; c < 2u; c++) {
        float fall = out_pk[c] * fm_exp2f(-(float)FRS_BLOCK / (0.15f * FRASIL_FS));   /* (-6 dB in 150 ms) */
        out_pk[c] = pk[c] > fall ? pk[c] : fall;
        out_ms[c] += (sq[c] * (1.0f / (float)FRS_BLOCK) - out_ms[c]) * (k > 1.0f ? 1.0f : k);
    }
}

static void next_block(int32_t *lr)
{
    while (!(due_ms <= (double)now64)) {
        eng_tick();
        now64++;
    }
    frasil_render(lr, FRS_BLOCK, out_gain);
    out_levels(lr);
    due_ms += block_ms;
    snap_write();
}

/* the brain alone to the millisecond ms, nothing rendered: frs_skip_to's */
void frs_skip_to(uint64_t ms)
{
    while (now64 < ms) {
        eng_tick();
        now64++;
        frasil_skip(1u);
    }
    frasil_skip_done();
    due_ms = (double)now64;
    blk_at = FRS_BLOCK;
    out_frames = (uint64_t)((double)now64 * (double)FRASIL_FS / 1000.0);
}

/* ------------------------------------------------------------------ life --- */
int frs_init(float fs)
{
    scene_t sc;
    if (open_now)
        return -2;
#if BR_FS_CONST
    if (fs != 44100.0f)                  /* (built as the FM-1's: its rate alone) */
        return -1;
#else
    if (!(fs >= 44100.0f && fs <= (float)BR_FS_MAX))   /* (lower, the filters' highest settings past Nyquist) */
        return -1;
#endif
    tick_fn = 0;
    now64 = 0;                           /* (as the FM-1 boots: at 0, its first millisecond 1) */
    due_ms = 0.0;
    block_ms = (double)FRS_BLOCK * 1000.0 / (double)fs;
    out_frames = 0;
    blk_at = FRS_BLOCK;
    mq_r = mq_w = 0;
    swell_held = trying = 0;
    count_stop();
    memset(&host, 0, sizeof host);
    out_pk[0] = out_pk[1] = out_ms[0] = out_ms[1] = 0.0f;
    tr_reset();
    memset(fingers, 0, sizeof fingers);
    nbf = 0;
    memset(hold_g, -1, sizeof hold_g);
    g_un = 0.0;
    g_was = 0.0f;
    org_gl.on = org_bs.on = gl_joined = 0;
    snap_none();                         /* (none of this open's yet) */
#if !BR_FS_CONST
    frasil_reset();                      /* (everything from the start: an engine opened again too) */
#endif
    eng_reset();
    eng_init(fs);
    eng_power_on();
    count_own = 0;                       /* (COUNT-IN in eng_tick's place, with the tracks) */
    count_hush = fe_count_hush;
    count_go = play_all;
    st.v[P_COUNT_IN] = 0;                /* (off at the start: frs_count_in) */
    keys_touch = 0;                      /* (the keys as 2.x's till frs_keys_touch) */
    new_shard(&sc);                      /* NEW SHARD, as an FM-1 with nothing saved starts */
    taken_sc = sc;
    scene_take(&sc);
    sound_mark();
    engine_push();
    now64 = 1;
    open_now = 1;
    return 0;
}

void frs_close(void)
{
    if (!open_now)
        return;
    frs_panic();
    open_now = 0;
}

const char *frs_version(void) { return FRS_VERSION; }
float frs_rate(void) { return FRASIL_FS; }
void frs_gain(float g) { out_gain = g > 0.0f ? (g < 1.0f ? g : 1.0f) : 0.0f; }   /* (NaN: 0) */
uint32_t frs_ms(void) { return (uint32_t)now64; }
uint32_t frs_ms_after(uint32_t frames)
{
    uint64_t now = now64;
    double due = due_ms;
    uint32_t at = blk_at;
    while (frames) {
        uint32_t k;
        if (at == FRS_BLOCK) {           /* (next_block's: the milliseconds due before it) */
            while (!(due <= (double)now))
                now++;
            due += block_ms;
            at = 0;
        }
        k = FRS_BLOCK - at < frames ? FRS_BLOCK - at : frames;
        at += k;
        frames -= k;
    }
    return (uint32_t)now;
}

void frs_panic(void)
{
    count_stop();
    tr_reset();
    fingers_forget();                    /* HOLD's: nothing held */
    mq_r = mq_w;                         /* the MIDI waiting: dropped */
    audition_stop();                     /* every note let go (the drones, MIDI's, the bass, the glints), the arpeggio
                                          * and the sequences stopped */
    memset(key, 0, sizeof key);          /* (keys down: theirs no more) */
    st.sustain = 0;
#if BR_FS_CONST
    frasil_panic();
#else
    frasil_silence();                    /* the sound at once: the voices, the glints, their echoes, the cloud */
#endif
}

void frs_cut(void)
{
    count_stop();
    eng_holds_release();                 /* (the fingers' notes let go first: RESTORE never has them) */
    fingers_forget();
    tr_reset();
    mq_r = mq_w;                         /* (the MIDI waiting: dropped) */
    eng_cut();
}

/* CUT as the iPad's transport has it: every track stopped (as frs_track_play stops it), each keeping what it plays (the
 * drones' chords, the arpeggios' notes, the bass's note, the sequences), then the sound emptied as frs_cut empties it;
 * PLAY (frs_play: frs_play_all) or a track's own play brings each back. RESTORE's notes as they were (the transport
 * keeps them now). Styves 2026-11-05: RESILIENCE, its tracks stopped, CUT, PLAY: the bass's sequence alone came back,
 * frs_cut's RELEASE ALL had let the drone's chords and GLINT's arpeggio go with what the transport kept */
static void arp_notes_back(arp_t *to, const arp_t *from)   /* (its notes as kept; its steps and the grid's from the CUT) */
{
    memcpy(to->held, from->held, sizeof to->held);
    memcpy(to->in, from->in, sizeof to->in);
    memcpy(to->prs, from->prs, sizeof to->prs);
    to->ins = from->ins;
    to->n = from->n;
    to->vel = from->vel;
    to->latch = from->latch;
    to->paused = from->paused;
    to->chg_ms = from->chg_ms;
    to->hand_ms = from->hand_ms;
    to->dirty = 1;
}

void frs_cut_stop(void)
{
    static scene_t r;
    static arp_t a, b;
    uint8_t rv = rst_valid;
    uint32_t t;
    count_stop();
    eng_holds_release();
    fingers_forget();
    mq_r = mq_w;
    for (t = 0; t < 3u; t++)
        tr_stop(t);                      /* (a stopped one too: a note latched since, kept) */
    a = arp;                             /* (their notes waiting: sound_off lets them go) */
    b = barp;
    memcpy(&r, &rst, sizeof r);
    eng_cut();
    arp_notes_back(&arp, &a);
    arp_notes_back(&barp, &b);
    memcpy(&rst, &r, sizeof rst);
    rst_valid = rv;
}

void frs_render_i24(int32_t *lr, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        if (blk_at == FRS_BLOCK) {
            next_block(blk);
            blk_at = 0;
        }
        lr[2u * i] = blk[2u * blk_at];
        lr[2u * i + 1u] = blk[2u * blk_at + 1u];
        blk_at++;
    }
    out_frames += n;
}

void frs_render(float *left, float *right, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        if (blk_at == FRS_BLOCK) {
            next_block(blk);
            blk_at = 0;
        }
        left[i] = (float)blk[2u * blk_at] * (1.0f / 8388608.0f);
        right[i] = (float)blk[2u * blk_at + 1u] * (1.0f / 8388608.0f);
        blk_at++;
    }
    out_frames += n;
}

/* -------------------------------------------------------------- settings --- */
uint32_t frs_param_count(void) { return NPARAM; }

int frs_param_info(uint32_t p, const char **name, uint32_t *max, uint32_t *def)
{
    if (p >= NPARAM)
        return -1;
    if (name)
        *name = PD[p].name;
    if (max)
        *max = PD[p].max;
    if (def)
        *def = PD[p].def;
    return 0;
}

_Static_assert(PK_SCENE == FRS_P_SCENE && PK_STATE == FRS_P_STATE && PK_MOMENT == FRS_P_MOMENT && PK_STEP == FRS_P_STEP &&
                   PK_ACTION == FRS_P_ACTION && PK_NONE == FRS_P_NONE && STATE_BYTES == FRS_STATE_BYTES,
               "the brain's kinds and state are frasil_engine.h's (core.c param_kind, state_save)");
uint32_t frs_param_kind(uint32_t p) { return param_kind(p); }

uint32_t frs_param_get(uint32_t p) { return p < NPARAM ? st.v[p] : 0u; }

/* after a setting: the bass's note a finger holds, cut (SEQUENCE off, its CLEAR), sounds again */
static void fingers_bass_on(void)
{
    if (hold_bn >= 0 && bass_note < 0 && !(st.v[P_BQ_ON] == SQT_SEQ && bq.run) && st.v[P_BQ_ON] != SQT_ARP)
        bass_live(hold_bn, 100);
}

void frs_param_set(uint32_t p, uint32_t x)
{
    keymode_gone = latch_gone = 0;
    if (p < NPARAM)
        param_set((int)p, (int32_t)(x > 255u ? 255u : PD[p].fmt == F_LTGT ? sound_val(p, x) : x));   /* (a TARGET
                                          * past this engine's list: none, as a scene's, core.c sound_val) */
    tr_keys();
    fingers_bass_on();
    tr_sync();
}

void frs_param_edit(uint32_t p, uint32_t x)
{
    keymode_gone = latch_gone = 0;
    holds_hide();                        /* (an edit: what the fingers hold sounds on) */
    if (p < NPARAM)
        eng_edit_param(p, x > 255u ? 255u : x);
    holds_show();
    tr_keys();
    fingers_bass_on();
    tr_sync();
}

void frs_param_text(uint32_t p, uint32_t x, char *out)
{
    out[0] = 0;
    if (p < NPARAM)
        value_text((int)p, (uint8_t)(x > PD[p].max ? PD[p].max : x), out);
}

/* the engine's state beside a scene (core.c state_save: "FRst", its version, then pairs; STATE_METRO the metronome) */
uint32_t frs_state_save(uint8_t *out) { return state_save(out); }
_Static_assert(5u + 2u * 20u <= FRS_STATE_BYTES, "the state fits FRS_STATE_BYTES (its 19 settings and the metronome)");

uint32_t frs_lfo_targets(void) { return NLFO_TGT - 1u; }
int frs_lfo_target(uint32_t t) { return t >= 1u && t < NLFO_TGT ? (int)LFO_TGT[t] : -1; }

uint32_t frs_tracks_bits(uint32_t tracks)
{
    static const uint8_t TRK[7] = {FRS_TRK_DRONE | FRS_TRK_GLINT | FRS_TRK_BASS, FRS_TRK_DRONE, FRS_TRK_GLINT,
                                   FRS_TRK_BASS, FRS_TRK_DRONE | FRS_TRK_GLINT, FRS_TRK_DRONE | FRS_TRK_BASS,
                                   FRS_TRK_GLINT | FRS_TRK_BASS};   /* (params.c params_to_engine's) */
    return TRK[tracks % 7u];
}

int frs_state_load(const uint8_t *in, uint32_t n)
{
    uint32_t i;
    keymode_gone = latch_gone = 0;
    if (n < 5u || in[0] != 'F' || in[1] != 'R' || in[2] != 's' || in[3] != 't' || in[4] < 1u || in[4] > STATE_V)
        return -1;
    for (i = 5; i + 1u < n; i += 2u) {
        uint32_t p = in[i], x = in[i + 1u];
        if (p == STATE_METRO) {
            st.metro = (uint8_t)(x != 0u);
            continue;
        }
        if (in[4] == 1u && p >= 130u)    /* (2.0's: KEY MODE 130, the MIDI channels, LOAD: 2.5's 23 higher) */
            p += 23u;
        if (in[4] <= 2u && p >= 153u)    /* (2.5's: KEY MODE 153 .. CLOCK 181: 2.6's one higher, SOFT before them) */
            p += 1u;
        if (p < NPARAM && frs_param_kind(p) == FRS_P_STATE)
            param_set((int)p, (int32_t)x);   /* (clamped; KEY MODE turned lets the notes go, as the FM-1's does; a
                                              * state before 3.0's: KEY MODE read in as the drones' and the bass's LATCH) */
    }
    tr_keys();
    tr_sync();
    return 0;
}

/* ------------------------------------------------------- the running state --- */
/* the engine alone's own part of it (core.c's live.c has the rest): the tracks stopped and what they keep, the
 * fingers (THE KEYS' HOLD), the sequences' origins, the host's tempo, the scene given last, the MIDI waiting for its
 * millisecond. Not in an FM-1's (none of it is the FM-1's): as an engine opened on its start has it, none stopped */
static void rs_dbl(rs_io *s, double *d)
{
    uint32_t w[2];
    memcpy(w, d, 8);
    V32(w[0]);
    V32(w[1]);
    if (!s->c.save)
        memcpy(d, w, 8);
}
static double alone_frac;                /* (alone_lv's: the next block's time after the clock's millisecond) */
static void alone_lv(br_live *c)
{
    rs_io *s = (rs_io *)c;
    uint32_t i, n = mq_w - mq_r;
    alone_frac = due_ms - (double)now64;
    rs_dbl(s, &alone_frac);
    VB(tr.stop);
    VU32S(tr.kept);
    V8(tr.bass);
    VB(hold_g);
    V8(hold_bg);
    VB(fingers);
    VB(fnote);
    VB(bass_fingers);
    V32(nbf);
    VU32S(hold_rec);
    V8(holds_off);
    rs_chords(s, hid.c, 128u);
    VU32S(hid.a);
    VU32S(hid.ba);
    V8(hid.bn);
    V8(hid.b);
    rs_scene(s, &taken_sc);
    rs_dbl(s, &host.bpm);
    rs_dbl(s, &host.beat);
    rs_dbl(s, (double *)&host.frame);
    V8(host.on);
    V8(host.playing);
    V8(host.was);
    rs_dbl(s, (double *)&host.clocks);
    rs_dbl(s, &g_un);
    VF(g_was);
    rs_dbl(s, &org_gl.at);
    V8(org_gl.on);
    rs_dbl(s, &org_bs.at);
    V8(org_bs.on);
    V8(gl_joined);
    V32(n);
    if (n > MQ_N) {
        s->bad = 1;
        return;
    }
    if (!s->c.save)
        mq_r = mq_w = 0;
    for (i = 0; i < n; i++) {
        uint32_t pkt = s->c.save ? mq[(mq_r + i) % MQ_N].pkt : 0u;
        double at = s->c.save ? (double)mq[(mq_r + i) % MQ_N].at : 0.0;
        V32(pkt);
        rs_dbl(s, &at);
        if (!s->c.save) {
            mq[i].pkt = pkt;
            mq[i].at = (uint64_t)at;
            mq_w = i + 1u;
        }
    }
}
_Static_assert(sizeof host.frame == 8u && sizeof host.clocks == 8u, "the host's frame and clocks: 8 bytes, as a double");

/* an FM-1's state has none of it: as an engine opened on its start (none stopped, no finger, the scene given last its) */
static void alone_reset(const uint8_t *scene)
{
    tr_reset();
    memset(hold_g, -1, sizeof hold_g);
    hold_bg = -1;
    memset(fingers, 0, sizeof fingers);
    memset(fnote, 0, sizeof fnote);
    memset(bass_fingers, 0, sizeof bass_fingers);
    nbf = 0;
    memset(hold_rec, 0, sizeof hold_rec);
    holds_off = 0;
    memset(&hid, 0, sizeof hid);
    if (scene)
        memcpy(&taken_sc, scene, sizeof taken_sc);
    memset(&host, 0, sizeof host);
    g_un = 0.0;
    g_was = 0.0f;
    org_gl.on = org_bs.on = gl_joined = 0;
    mq_r = mq_w = 0;
}

#if FRS_LEAD
static uint8_t lead_lv_buf[LIVE_LEAD_BYTES];   /* (the LEAD's part, then after the rest) */
#endif
uint32_t frs_live_save(uint8_t *out, uint32_t max, const uint8_t *scene)
{
    uint32_t n, m = 0;
    if (!open_now || !out)
        return 0;
    max = max > FRS_LIVE_BYTES ? FRS_LIVE_BYTES : max;
#if FRS_LEAD
    n = live_save(out, max, scene, swell_held, alone_lv, 1, lead_lv_buf, sizeof lead_lv_buf, &m);
    if (n && m && m <= max - n) {        /* (the LEAD's after the rest; no room: the rest alone) */
        memcpy(out + n, lead_lv_buf, m);
        n += m;
    }
#else
    n = live_save(out, max, scene, swell_held, alone_lv, 1, 0, 0, &m);
#endif
    return n;
}

uint32_t frs_live_ms(const uint8_t *in, uint32_t n)
{
    uint32_t ms;
    uint8_t flags;
    return in && live_head(in, n, &ms, &flags) ? ms : 0u;
}

int frs_live_load(const uint8_t *in, uint32_t n, const uint8_t *scene)
{
    uint32_t ms;
    uint8_t flags, held = 0;
    int f;
    if (!open_now || !in || !live_head(in, n, &ms, &flags))
        return -1;
    keymode_gone = latch_gone = 0;
    alone_frac = 0.0;
    f = live_load(in, n, scene, &held, alone_lv);
    if (f < 0)
        return -1;
    if (!(f & LIVE_F_ALONE))
        alone_reset(scene);
    swell_held = held;
    /* its millisecond next; a block begins at once, its milliseconds as the saved engine's next block had them (an
     * FM-1's: from this one) */
    now64 = ms;
    due_ms = (double)ms + (alone_frac > 0.0 && alone_frac < 2.0 * block_ms ? alone_frac : 0.0);
    blk_at = FRS_BLOCK;
    return 0;
}

/* --------------------------------------------------------------- playing --- */
void frs_key_down(uint32_t k, int note, uint32_t vel, int single)
{
    if (k >= FRS_NKEYS || key[k].kind != K_NONE)
        return;
    eng_key_down(k, note < 0 ? 0 : note > 127 ? 127 : note, vel < 1u ? 1u : vel > 127u ? 127u : vel, single);
    tr_sync();
}

void frs_key_up(uint32_t k)
{
    if (k < FRS_NKEYS)
        key_up(k);
}

void frs_key_pressure(uint32_t k, uint32_t p)
{
    if (k < FRS_NKEYS && key[k].kind != K_NONE)
        key_pressure(k, (uint8_t)(p > 127u ? 127u : p));
}

/* HARMONY's CHORD for one call as `chord` says (FRS_CHORD_HARMONY: as it is; SINGLE: VOICING CLOSE too, as a single
 * key's), then back: set in st.v alone (chord_build reads it there), not through param_set (no MORPH's step, nothing
 * edited) */
static uint8_t chord_was[2];
static void chord_as(int chord)
{
    chord_was[0] = st.v[P_CHORD];
    chord_was[1] = st.v[P_VOICING];
    if (chord < 0)
        return;
    st.v[P_CHORD] = (uint8_t)((uint32_t)chord > PD[P_CHORD].max ? PD[P_CHORD].max : (uint32_t)chord);
    if (!st.v[P_CHORD])
        st.v[P_VOICING] = 0;
}
static void chord_back(void)
{
    st.v[P_CHORD] = chord_was[0];
    st.v[P_VOICING] = chord_was[1];
}

int frs_latch(uint32_t track, uint32_t note, int chord)
{
    uint32_t n = note > 127u ? 127u : note;
    int r, b = (int)n - 12 * (int)st.v[P_GL_OCTAVE], bp = bass_played((int)n);
    if (track == 2u && st.v[P_BQ_ON] == SQT_ARP) {   /* on ARP as GLINT's: among its notes */
        if (bp >= 1 && bit_get(hold_ba, bp)) {
            bit_put(hold_ba, bp, 0);     /* (a note a finger holds in it, latched: it stays once let go) */
            holds_hide();
            eng_edits();
            holds_show();
            barp.latch = 1;
            barp.paused = 0;
            r = 1;
        } else {
            holds_hide();
            r = eng_latch(track, note);
            holds_show();
        }
    } else if (track == 1u && b >= 1 && bit_get(hold_a, b)) {
        bit_put(hold_a, b, 0);           /* a note a finger holds in the arpeggio, latched: it stays once let go */
        holds_hide();
        eng_edits();
        holds_show();
        arp.latch = 1;
        arp.paused = 0;
        r = 1;
    } else if (track == 2u && hold_bn >= 0 && hold_bn == bass_played((int)n) && bass_note == hold_bn) {
        hold_bn = hold_b = -1;           /* the bass's note a finger holds, latched: it stays once let go */
        bass_unfinger(bass_played((int)n));
        holds_hide();
        eng_edits();
        holds_show();
        r = 1;
    } else if (track == 2u && hold_bn >= 0 && bass_note == hold_bn && hold_b >= 0 && bass_played((int)n) == hold_b) {
        hold_b = -1;                     /* the note latched under the fingers', let go: theirs sounds on */
        holds_hide();
        eng_edits();
        holds_show();
        r = 0;
    } else if (track == 2u && hold_bn >= 0 && bass_note >= 0 && bass_note != hold_bn &&
               bass_played((int)n) == bass_note) {
        holds_hide();                    /* the note latched over the fingers', let go: theirs again */
        r = eng_latch(track, note);
        holds_show();
        hold_b = -1;
        bass_live(hold_bn, 100);
    } else {
        int over = track == 2u && hold_bn >= 0;
        chord_as(track == 0u ? chord : FRS_CHORD_HARMONY);
        holds_hide();                    /* (what the fingers hold sounds on) */
        r = eng_latch(track, note);
        holds_show();
        chord_back();
        if (over && r == 1)
            hold_b = (int8_t)bass_note;  /* (latched over the fingers' note: what they come back to) */
    }
    if (track == 2u && r == 1 && tr.stop[2] && !bass_on_seq()) {
        tr.stop[2] = 0;                  /* (a note latched on the stopped bass: it plays, with that one) */
        tr.bass = -1;
    }
    tr_sync();                           /* (latched on a stopped track: it plays, with that alone) */
    return r;
}

/* a finger into a take (RECORD, LIVE or STEP, the layer on SEQ), as the FM-1's keys write (seq_key, bseq_key): GLINT's
 * glint and the bass's note sounding while it is down; the bass's held notes the last on top, another still down taking
 * over as it lets go (LIVE: legato into the take, as bseq_up). Its up the take's whatever RECORD is by then. A finger
 * has no velocity: no ACCENT (100 would write it) */
#define REC_VEL 99u

static void glint_rec(uint32_t n, int down)
{
    int b;
    if (down) {
        b = (int)n - 12 * (int)st.v[P_GL_OCTAVE];
        fnote[1][n] = (int8_t)b;
        bit_put(hold_rec[0], (int)n, 1);
        bit_put(hold_k, b, 1);
        seq_key(b, b, REC_VEL);
        return;
    }
    b = fnote[1][n];
    bit_put(hold_rec[0], (int)n, 0);
    bit_put(hold_k, b, 0);
    if (sq.sounding != b && !ring_has(b))   /* (the step's own note sounds on) */
        frasil_glint_off(b);
}

static int rec_finger(int p)            /* the bass's held note p (as played) went into the take at its down */
{
    uint32_t m;
    for (m = 0; m < 128u; m++)
        if (bit_get(hold_rec[1], (int)m) && fnote[2][m] == (int8_t)p)
            return 1;
    return 0;
}

static void bass_rec(uint32_t n, int down)
{
    int p, top;
    if (down) {
        p = bass_played((int)n);
        fnote[2][n] = (int8_t)p;
        bit_put(hold_rec[1], (int)n, 1);
        bass_unfinger(p);
        if (nbf == sizeof bass_fingers) {   /* (full, ups lost: the oldest goes) */
            memmove(bass_fingers, bass_fingers + 1, nbf - 1u);
            nbf--;
        }
        bass_fingers[nbf++] = (int8_t)p;
        bseq_key(p, p, REC_VEL);
        return;
    }
    p = fnote[2][n];
    bit_put(hold_rec[1], (int)n, 0);
    top = nbf && bass_fingers[nbf - 1u] == (int8_t)p;
    bass_unfinger(p);
    if (!top)
        return;                          /* (another finger's note plays on) */
    if (nbf && st.v[P_BQ_REC] == 2u && !holds_off && rec_finger(bass_fingers[nbf - 1u])) {
        bseq_key(bass_fingers[nbf - 1u], bass_fingers[nbf - 1u], REC_VEL);
        return;
    }
    if (st.v[P_BQ_REC])
        bseq_up(p);
    else if (bass_note == p)             /* (RECORD off since its down: its note goes, the take closed already) */
        bass_stop();
    if (nbf && !holds_off && bass_note < 0 && !rec_finger(bass_fingers[nbf - 1u]))
        bass_live(bass_fingers[nbf - 1u], 100);   /* (a finger from before the take, under it: its note again) */
}

void frs_hold(uint32_t track, uint32_t note, int down, int chord)
{
    uint32_t n = note > 127u ? 127u : note;
    int b = (int)n - 12 * (int)st.v[P_GL_OCTAVE];
    if (track > 2u)
        return;
    if (down) {
        if (fingers[track][n] == 255u || fingers[track][n]++)
            return;                      /* (another finger on it: held already) */
    } else if (!fingers[track][n] || --fingers[track][n]) {
        return;                          /* (not held, or another finger still on it) */
    }
    if (track == 1u && (down ? b >= 1 && st.v[P_SQ_REC] && st.v[P_SQ_TYPE] == SQT_SEQ : bit_get(hold_rec[0], (int)n))) {
        glint_rec(n, down);
        return;
    }
    if (track == 2u && (down ? st.v[P_BQ_REC] && st.v[P_BQ_ON] == SQT_SEQ : bit_get(hold_rec[1], (int)n))) {
        bass_rec(n, down);
        return;
    }
    if (track == 2u && st.v[P_BQ_ON] != SQT_ARP && !down && bit_get(hold_ba, fnote[2][n])) {
        bit_put(hold_ba, fnote[2][n], 0);   /* (put in the arpeggio by its down, PLAYS turned since: out) */
        arp_remove(&barp, fnote[2][n]);
        return;
    }
    if (track == 2u && st.v[P_BQ_ON] == SQT_ARP) {   /* on ARP as GLINT's: into its notes; stopped, the note itself */
        int bp = bass_played((int)n);
        if (!down) {                     /* (held as a note by its down, PLAYS turned since: let go) */
            bass_unfinger(fnote[2][n]);
            if (!nbf)
                hold_bn = hold_b = -1;
        }
        if (hold_bg >= 0 || (down && tr.stop[2] && barp.paused)) {
            if (down && hold_bg < 0) {
                bass_play(bp, 100);
                hold_bg = (int8_t)bass_note;
            } else if (!down && bass_note == hold_bg) {
                bass_stop();
            }
            if (!down)
                hold_bg = -1;
            return;
        }
        if (down)
            fnote[2][n] = (int8_t)bp;
        eng_hold(2, (uint32_t)((down ? bp : fnote[2][n]) + 12 * ((int)st.v[P_BS_OCTAVE] - 2)), (uint32_t)(down != 0));
        return;
    }
    if (track == 1u && (hold_g[n] >= 0 || (down && b >= 1 && tr.stop[1] && st.v[P_SQ_TYPE] == SQT_ARP))) {
        if (down && hold_g[n] < 0) {     /* (GLINT's arpeggio stopped: the glint itself while down, the arpeggio
                                          * waiting on) */
            frasil_glint_on(b, 100.0f / 127.0f);
            hold_g[n] = (int8_t)b;
        } else if (!down) {
            frasil_glint_off(hold_g[n]); /* (the glint struck, whatever OCTAVE is now) */
            hold_g[n] = -1;
        }
        return;
    }
    if (track == 2u) {
        bass_hold(n, down);
        return;
    }
    if (track == 1u) {                   /* (its note without OCTAVE at the down: its up the same) */
        if (down)
            fnote[1][n] = (int8_t)b;
        else
            n = (uint32_t)(fnote[1][n] + 12 * (int)st.v[P_GL_OCTAVE]) > 127u ? 127u
                : (uint32_t)(fnote[1][n] + 12 * (int)st.v[P_GL_OCTAVE]);
    }
    chord_as(track == 0u && down ? chord : FRS_CHORD_HARMONY);
    eng_hold(track, n, (uint32_t)(down != 0));
    if (track == 0u && down && hold_c[n].k && dseq_key(hold_c[n].n, hold_c[n].k)) {
        bit_put(drec_f, (int)n, 1);      /* (RECORD STEP on the drone's sequence: its up moves STEP on) */
        dq.keys++;
    }
    chord_back();
    if (track == 0u && !down && bit_get(drec_f, (int)n)) {
        bit_put(drec_f, (int)n, 0);
        dseq_up();
    }
}

void frs_hold_off(uint32_t track)
{
    uint32_t n;
    if (track > 2u)
        return;
    holds_off = 1;
    for (n = 0; n < 128u; n++)           /* (the bass's: under the top ones first, so it ends where they began) */
        while (fingers[track][n])
            frs_hold(track, n, 0, FRS_CHORD_HARMONY);
    holds_off = 0;
}

void frs_clear(uint32_t track)
{
    int i;
    holds_hide();                        /* (what the fingers hold sounds on) */
    eng_clear(track);
    holds_show();
    if (track == 1u) {
        for (i = 1; i < 128; i++)        /* the arpeggio's latched notes gone, the fingers' in it again */
            if (bit_get(hold_a, i)) {
                arp_add(&arp, i, 100);
                if (!tr.stop[1])         /* (stopped, it waits on) */
                    arp.paused = 0;
            }
    } else if (track == 2u && st.v[P_BQ_ON] == SQT_ARP) {
        for (i = 1; i < 128; i++)        /* the arpeggio's latched notes gone, the fingers' in it again */
            if (bit_get(hold_ba, i)) {
                arp_add(&barp, i, 100);
                if (!tr.stop[2])         /* (stopped, it waits on) */
                    barp.paused = 0;
            }
    } else if (track == 2u && hold_bn >= 0) {
        hold_b = -1;                     /* (nothing latched under the fingers' note now) */
        bass_live(hold_bn, 100);
    }
    if (track == 0u)
        memset(tr.kept, 0, sizeof tr.kept);
    else if (track == 2u)
        tr.bass = -1;
}

void frs_track_play(uint32_t track, int on)
{
    if (track > 2u)
        return;
    count_stop();
    if (on)
        tr_start(track);
    else
        tr_stop(track);
}

/* COUNT-IN (frs_count_in, core.c's count_run, here in eng_tick's place): PLAY from nothing playing (frs_play_all), a LIVE
 * take armed (RECORD LIVE, the layer on SEQ), or REC on a layer from nothing playing (frs_record_layer), ticks a bar first,
 * four beats at the grid's tempo (TEMPO, or the MIDI clock's), the one high, whatever the metronome; on the downbeat
 * after them everything starts, as PLAY would have (play_all), the take with it. A stop, CUT, PANIC, RELEASE ALL, a track
 * played or stopped, a scene taken or stopped: no more count, nothing started */
static void play_all(void);
static void fe_count_hush(void)          /* (a sequence or an arpeggio begun meanwhile, SEQ turned on: it waits for the
                                          * downbeat with the rest) */
{
    tr_stop(1);
    tr_stop(2);
    if (dq.run)
        dseq_stop();                     /* (the drone's: its chords latched meanwhile sound on) */
}

void frs_count_in(int on) { st.v[P_COUNT_IN] = (uint8_t)(on != 0); }
void frs_overdub(int on) { overdub = (uint8_t)(on != 0); }
void frs_step_stays(int on) { step_stays = (uint8_t)(on != 0); }
void frs_record_metronome(int on) { st.metro = (uint8_t)(on != 0); }

void frs_play_all(int on)
{
    uint32_t t;
    if (!on) {
        count_stop();
        for (t = 0; t < 3u; t++)
            tr_stop(t);
        return;
    }
    if (cnt.run)
        return;                          /* (counting in: everything on its downbeat) */
    if (st.v[P_COUNT_IN] && take_armed() && !playing_any()) {
        count_begin();
        return;
    }
    play_all();
}

static void play_all(void)
{
    uint32_t t;
    int gs = st.v[P_SQ_TYPE] == SQT_SEQ, bs = st.v[P_BQ_ON] == SQT_SEQ, ds = st.v[P_DQ_ON] != 0;
    int ga = arp_plays(), ba = st.v[P_BQ_ON] == SQT_ARP;
    if (playing_any()) {
        /* one playing (a sequence, the arpeggio): the others started as frs_track_play starts them, the grid as it is */
        for (t = 0; t < 3u; t++)
            tr_start(t);
        return;
    }
    tr_start(0);                         /* (the drones' kept chords, the bass's kept note: as PLAY's top begins) */
    tr.stop[1] = tr.stop[2] = 0;
    if (gs || bs || ds) {                /* the sequences together from their first steps, as PLAY starts them */
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
    } else if ((ga && arp.paused) || (ba && barp.paused)) {
        grid_top(0);
    }
    if (ga)
        arp_wait(&arp, 0);
    if (ba)
        arp_wait(&barp, 0);
    bass_again();
}

static void midi_post(uint32_t pkt, uint64_t at)
{
    if (mq_w - mq_r >= MQ_N)
        return;                          /* (full: dropped) */
    mq[mq_w % MQ_N].pkt = pkt;
    mq[mq_w % MQ_N].at = at;
    mq_w++;
}

void frs_midi(uint8_t status, uint8_t d1, uint8_t d2)
{
    midi_post((uint32_t)status << 8 | (uint32_t)(d1 & 0x7Fu) << 16 | (uint32_t)(d2 & 0x7Fu) << 24, 0);
}

void frs_midi_at(uint32_t offset, uint8_t status, uint8_t d1, uint8_t d2)
{
    double x = (double)(out_frames + offset) * 1000.0 / (double)FRASIL_FS;   /* its frame's time, ms */
    uint64_t at = (uint64_t)x;
    midi_post((uint32_t)status << 8 | (uint32_t)(d1 & 0x7Fu) << 16 | (uint32_t)(d2 & 0x7Fu) << 24,
              (double)at < x ? at + 1u : at);   /* (the first millisecond at or after it) */
}

void frs_midi_ms(uint64_t ms, uint8_t status, uint8_t d1, uint8_t d2)
{
    midi_post((uint32_t)status << 8 | (uint32_t)(d1 & 0x7Fu) << 16 | (uint32_t)(d2 & 0x7Fu) << 24, ms);
}

void frs_host_tempo(double bpm, double beat, int playing)
{
    host.on = bpm >= 13.0 && bpm < 1000.0;   /* (a MIDI clock's: its clocks under 200 ms apart; NaN: none) */
    host.bpm = host.on ? bpm : 0.0;
    host.beat = beat < 1e9 ? (beat > -1e9 ? beat : -1e9) : beat >= 1e9 ? 1e9 : 0.0;   /* (finite; NaN: 0) */
    host.frame = out_frames;
    host.playing = (uint8_t)(playing != 0);
}

/* the transport keeps something to play again: a stopped track's chords or bass note, GLINT's arpeggio or sequence
 * stopped, the bass's sequence stopped */
static int tr_keeps(void)
{
    return (tr.kept[0] | tr.kept[1] | tr.kept[2] | tr.kept[3]) || tr.bass >= 0 ||
           (tr.stop[1] && ((arp.n && arp_plays()) || st.v[P_SQ_TYPE] == SQT_SEQ)) ||
           (tr.stop[2] && (bass_on_seq() || (barp.n && st.v[P_BQ_ON] == SQT_ARP))) || (tr.stop[0] && drone_on_seq());
}

/* what the transport stopped kept, and nothing else playing */
static int tr_only(void)
{
    return tr_keeps() && !(lit[0] | lit[1] | lit[2] | lit[3] || bass_note >= 0 || sq.run || bq.run || dq.run ||
                           (arp.n && !arp.paused && arp_plays()) ||
                           (barp.n && !barp.paused && st.v[P_BQ_ON] == SQT_ARP));
}

int frs_play(void)
{
    int was = holding_any();             /* (the FM-1's: a second tap quick after it, RELEASE ALL: frs_release) */
    if (tr_only()) {
        frs_play_all(1);                 /* what the transport stopped, nothing else playing: back as it was (not the
                                          * scene's notes over what it kept) */
        return was;
    }
    eng_play_tap(&taken_sc);             /* (nothing playing at all: the scene given last's three tracks) */
    tr_sync();
    return was;
}

void frs_release(void)
{
    count_stop();
    eng_holds_release();                 /* (the fingers' notes let go first: RESTORE never has them) */
    fingers_forget();
    tr_reset();                          /* (what a stopped track kept: gone too) */
    sound_off();                         /* the FM-1's PLAY tapped twice: RELEASE ALL, never RESTORE */
    grid_top(0);
    eng_say("RELEASE ALL", rst_valid ? "HOLD PLAY: RESTORE" : "");
}

int frs_record(void)
{
    int r = eng_record();
    tr_sync();
    return r;
}

int frs_record_layer(uint32_t layer)
{
    int r = eng_record_on(layer, 1);
    tr_sync();
    return r;
}

void frs_keys_touch(int on) { eng_keys_touch(on); }

/* ---------------------------------------------------------- a layer's preset --- */
_Static_assert(FRS_PRESET_BYTES == PRESET_BYTES && FRS_PRESET_HEAD == PRESET_HEAD, "frasil_engine.h's preset is core.c's");
uint32_t frs_preset_capture(uint32_t layer, const char *name, uint8_t *out) { return eng_preset_capture(layer, name, out); }
int frs_preset_apply(const uint8_t *in, uint32_t n)
{
    int r;
    keymode_gone = latch_gone = 0;
    r = eng_preset_apply(in, n);
    fingers_bass_on();
    tr_sync();
    return r;
}
int frs_preset_is(const uint8_t *in, uint32_t n) { return eng_preset_is(in, n); }
int frs_preset_param(uint32_t layer, uint32_t i)
{
    const uint8_t *ps;
    uint32_t n = preset_set(layer, &ps);
    return i < n ? (int)ps[i] : -1;
}
void frs_swell(int held) { swell_held = (uint8_t)(held != 0); }
void frs_metronome(int on) { metro_free = (uint8_t)(on != 0); }
void frs_trying(int on) { trying = (uint8_t)(on != 0); }

void frs_compare(int on)
{
    st.compare = (uint8_t)(on != 0);
    st.engine_dirty = 1;
}

void frs_release_all(void)
{
    count_stop();
    eng_holds_release();                 /* (the fingers' notes let go first: RESTORE never has them) */
    fingers_forget();
    if (tr_only()) {                     /* (RESTORE: what the transport stopped, back, frs_cut_stop's too) */
        frs_play_all(1);
        eng_say("RESTORED", "");
        return;
    }
    tr_reset();                          /* (what a stopped track kept: gone too) */
    eng_release_all(&taken_sc);          /* (RESTORE, nothing kept: the scene given last's notes) */
}

/* ---------------------------------------------------------------- scenes --- */
static void taken(const uint8_t *scene) { memcpy(&taken_sc, scene, sizeof taken_sc); }

void frs_scene_take(const uint8_t *scene)
{
    scene_t sc;
    memcpy(&sc, scene, sizeof sc);
    taken(scene);
    count_stop();
    tr_reset();
    fingers_forget();
    scene_take(&sc);
}

void frs_scene_play(const uint8_t *scene, int hold)
{
    scene_t sc;
    memcpy(&sc, scene, sizeof sc);
    taken(scene);
    count_stop();
    tr_reset();
    fingers_forget();
    eng_scene_play(&sc, hold);
}

void frs_scene_go(const uint8_t *scene)
{
    scene_t sc;
    memcpy(&sc, scene, sizeof sc);
    taken(scene);
    count_stop();
    if (!st.v[P_LOAD]) {                 /* (LOAD SND ONLY: what plays stays, a stopped track and the fingers' too) */
        tr_reset();
        fingers_forget();
    }
    eng_scene_go(&sc);
}

void frs_scene_stop(void)
{
    count_stop();
    tr_reset();
    fingers_forget();
    eng_scene_stop();
}

void frs_scene_store(uint8_t *out, const uint8_t *from)
{
    scene_t sc;
    uint32_t i;
    if (from)
        memcpy(&sc, from, sizeof sc);
    else
        new_shard(&sc);
    anchor_settle();                     /* (ANCHOR's way back under way: stored where it goes) */
    for (i = 0; i < NSOUND; i++)
        sc.v[i] = kept_v(i);
    notes_in();                          /* (a stopped track's notes as it would play them) */
    scene_notes_take(&sc);
    notes_out();
    sound_mark();
    taken_sc = sc;
    memcpy(out, &sc, sizeof sc);
}

/* ---------------------------------------------------------------- ANCHOR --- */
_Static_assert(ANCHOR_MS == FRS_ANCHOR_MS, "the FM-1's long press is the API's");
void frs_anchor_set(void) { anchor_set(); }
int frs_anchor_back(uint32_t ms) { return anchor_back(ms); }
void frs_anchor_drop(void) { anc.on = 0; }
int frs_anchor_state(float *k) { return anchor_state(k); }
uint32_t frs_anchor_get(uint8_t *out)
{
    if (!anc.on)
        return 0;
    memcpy(out, &anc.sc, sizeof anc.sc);
    return (uint32_t)sizeof anc.sc;
}
void frs_anchor_put(const uint8_t *scene)
{
    anchor_settle();
    memcpy(&anc.sc, scene, sizeof anc.sc);
    anc.on = 1;
}
uint32_t frs_param_heard(uint32_t p)     /* (the FM-1's app.c vh_n: morph_off on its value, rounded, in its range) */
{
    float v;
    if (p >= NPARAM)
        return 0u;
    if (!mo.on || !mo.anc || p >= NMORPH)
        return st.v[p];
    v = (float)st.v[p] + morph_off[p] + 0.5f;
    return v < 0.0f ? 0u : v > (float)PD[p].max ? PD[p].max : (uint32_t)v;
}

void frs_scene_edited(const uint8_t *played, uint8_t *out)
{
    scene_t p, e;
    memcpy(&p, played, sizeof p);
    notes_in();
    eng_scene_edited(&p, &e);
    notes_out();
    memcpy(out, &e, sizeof e);
}

void frs_keys_defaults(void)
{
    uint32_t p;
    for (p = P_KY_DR_HARM; p <= P_KY_BS_STACK; p++)
        frs_param_set(p, PD[p].def);
}

void frs_scene_new(uint8_t *out)
{
    scene_t sc;
    new_shard(&sc);
    memcpy(out, &sc, sizeof sc);
}

int frs_scene_over(uint8_t *scene, uint32_t n)
{
    scene_t sc;
    if (n == FRS_SCENE_BYTES)
        return 0;
    memcpy(&sc, scene, sizeof sc);       /* (the caller's bytes: no alignment of its own needed) */
    if (scene_over(&sc, n))              /* (core.c: FRS{'s 590 .. FRSt's 297) */
        return -1;
    memcpy(scene, &sc, sizeof sc);
    return 0;
}

void frs_scene_amt_over(uint8_t *scene)
{
    amt_pct_over(scene + offsetof(scene_t, lfo_amt));   /* (core.c: the FM-1's FRNN and FRSR too) */
}

int frs_scene_factory(uint32_t i, uint8_t *out)
{
    scene_t sc;
    if (i >= NFACTORY)
        return -1;
    factory_get(i, &sc);
    memcpy(out, &sc, sizeof sc);
    return 0;
}

int frs_dirty(void) { return st.dirty; }

/* ------------------------------------------------------------------ LEAD --- */
_Static_assert(FRS_LEAD_BYTES == LEAD_BYTES && FRS_SHARD_BYTES == FRS_SCENE_BYTES + LEAD_BYTES,
               "a shard: its scene, then its LEAD (core.c eng_lead_*)");
#if FRS_LEAD
_Static_assert(FRS_LEAD_NV == LEAD_NV, "frs_view_t's LEAD voices are dsp/lead.h's");
#endif
void frs_lead_none(uint8_t *out) { eng_lead_none(out); }
void frs_lead_take(const uint8_t *in) { eng_lead_take(in); }
void frs_lead_put(const uint8_t *in) { eng_lead_put(in); }
void frs_lead_get(uint8_t *out) { eng_lead_get(out); }
void frs_lead_wheels(uint32_t mod, int32_t bend)
{
    eng_lead_wheels(mod > 127u ? 127u : mod, bend < -8192 ? -8192 : bend > 8191 ? 8191 : bend);
}

/* the drone's sequence's steps, edited (frs_param_set's way: what the fingers hold sounds on; the shard edited) */
int frs_dseq_step(uint32_t step, const uint8_t *notes, uint32_t n)
{
    if (step >= SC_DSEQ)
        return -1;
    if (eng_dseq_step(step, notes, notes ? n : 0u))
        keys_edited = 1;
    return 0;
}

int frs_dseq_note(uint32_t step, uint32_t note, int on)
{
    if (step >= SC_DSEQ || note < 1u || note >= DQ_TIE)
        return -1;
    if (eng_dseq_note(step, note, on))
        keys_edited = 1;
    return 0;
}

void frs_dseq_clear_step(uint32_t step) { frs_dseq_step(step, 0, 0); }

/* ---------------------------------------------------------------- a UI's --- */
void frs_view(frs_view_t *v)
{
    uint32_t i;
    memset(v, 0, sizeof *v);
    for (i = 0; i < 4u; i++)
        v->latched[i] = lit[i];
    for (i = 0; i < 128u; i++)
        if (held_by[i])
            v->held[i >> 5] |= 1u << (i & 31u);
    v->narp = arp.n > 16u ? 16u : arp.n;
    for (i = 0; i < v->narp; i++)
        v->arp[i] = arp.held[i];
    memcpy(v->seq, seq, sizeof v->seq);
    memcpy(v->bnote, bnote, sizeof v->bnote);
    v->bq_steps = st.v[P_BQ_STEPS];
    v->bq_len = st.v[P_BQ_LEN];
    v->nbarp = barp.n > 16u ? 16u : barp.n;
    for (i = 0; i < v->nbarp; i++)
        v->barp[i] = barp.held[i];
    v->barp_note = (int8_t)(barp.sounding >= 0 ? barp.sounding : -1);
    v->bass = bass_note < 0 ? -1 : (int8_t)bass_sounding(bass_note);
    v->seq_pos = (int8_t)(st.v[P_SQ_TYPE] == SQT_SEQ && sq.run ? sq.pos : -1);
    v->bseq_pos = (int8_t)(st.v[P_BQ_ON] == SQT_SEQ && bq.run ? bq.pos : -1);
    v->seq_run = sq.run;
    v->bseq_run = bq.run;
    v->arp_latch = arp.latch;
    v->arp_paused = arp.paused;
    v->barp_latch = barp.latch;
    v->barp_paused = barp.paused;
    v->dirty = st.dirty;
    v->grid = grid.tick;
    v->tempo = eng_ms() - grid.clk_at < 500u && grid.clk_ms > 1.0f ? 2500.0f / grid.clk_ms : (float)(st.v[P_TEMPO] + 30u);
    v->ms = eng_ms();
    for (i = 0; i < 4u; i++)
        v->kept[i] = tr.kept[i];
    v->playing = (uint8_t)(tr_playing(0) | tr_playing(1) << 1 | tr_playing(2) << 2);
    v->bass_kept = tr.bass < 0 ? -1 : (int8_t)bass_sounding(tr.bass);
    v->counting = cnt.run;
    v->count_beat = cnt.beat;
    memcpy(v->dnote, dnote, sizeof v->dnote);
    v->dq_len = st.v[P_DQ_LEN];
    v->dq_rate = st.v[P_DQ_RATE];
    v->dseq_pos = (int8_t)(st.v[P_DQ_ON] && dq.run ? dq.pos : -1);
    v->dseq_run = dq.run;
    v->dq_nsnd = dq.nsnd;
    memcpy(v->dq_snd, dq.snd, sizeof v->dq_snd);
#if FRS_LEAD
    {                                    /* the LEAD's voices (card #152: PERFORM's FRAME's lead_view) */
        lead_view_t l;
        uint32_t i;
        frasil_lead_view(&l);
        for (i = 0; i < FRS_LEAD_NV; i++) {
            v->lead_note[i] = l.vnote[i];
            v->lead_level[i] = l.venv[i];
        }
        v->lead_held = l.held;
        v->lead_meter = frasil_lead_meter();
    }
#else
    memset(v->lead_note, -1, sizeof v->lead_note);
    memset(v->lead_level, 0, sizeof v->lead_level);
    v->lead_held = 0;
    v->lead_meter = 0.0f;
#endif
}

void frs_live(float *out) { eng_live(out); }

void frs_live_fx(float *out)
{
    uint32_t i;
    for (i = 0; i < FRS_LIVE_FX; i++)
        out[i] = 0.0f;
#if !BR_FS_CONST
    frasil_fx_view(out);
#endif
    for (i = 0; i < 2u; i++) {
        out[32u + i] = out_pk[i] > 1.0f ? 1.0f : out_pk[i];
        out[34u + i] = fm_sqrtf(out_ms[i]);
    }
    out[36] = frasil_pump_duck();
}

/* |1 + mix (H - 1)| in dB at w (radians a sample), H's numerator and denominator in z^-1 (b0 b1 b2, 1 a1 a2) */
static float fx_db(const float *b, const float *a, float mix, float w)
{
    float c1 = fm_cosf(w), s1 = -fm_sinf(w), c2 = fm_cosf(2.0f * w), s2 = -fm_sinf(2.0f * w);
    float nr = b[0] + b[1] * c1 + b[2] * c2, ni = b[1] * s1 + b[2] * s2;
    float dr = 1.0f + a[0] * c1 + a[1] * c2, di = a[0] * s1 + a[1] * s2, d2 = dr * dr + di * di;
    float hr, hi, mr, mi, m2;
    if (!(d2 > 1e-30f))
        return 0.0f;
    hr = (nr * dr + ni * di) / d2;       /* H = N / D */
    hi = (ni * dr - nr * di) / d2;
    mr = 1.0f + mix * (hr - 1.0f);
    mi = mix * hi;
    m2 = mr * mr + mi * mi;
    return 3.0103f * fm_log2f(m2 > 1e-20f ? m2 : 1e-20f);   /* (10 log10) */
}

float frs_fx_response(const float *fx, int which, float hz, float fs)
{
    float w = 6.2831853f * hz / fs, db = 0.0f;
    if (!(fs > 0.0f) || !(hz > 0.0f) || hz >= 0.5f * fs)
        return 0.0f;
    if (which == 0) {                    /* MASTER FILTER: Zavalishin's state-variable, the bilinear transform's */
        float g = fm_tanf(3.14159265f * br_clampf(fx[10], 10.0f, 0.49f * fs) / fs);
        float k = fx[11] > 0.0f ? 1.0f / fx[11] : 1.4142f, d0 = 1.0f + g * k + g * g, b[3], a[2];
        if (fx[9] < 0.5f || fx[8] <= 0.0f)
            return 0.0f;
        if (fx[9] < 1.5f) {
            b[0] = g * g / d0;
            b[1] = 2.0f * g * g / d0;
            b[2] = g * g / d0;
        } else {
            b[0] = 1.0f / d0;
            b[1] = -2.0f / d0;
            b[2] = 1.0f / d0;
        }
        a[0] = 2.0f * (g * g - 1.0f) / d0;
        a[1] = (1.0f - g * k + g * g) / d0;
        return fx_db(b, a, fx[8], w);
    }
    if (fx[14] > 0.0f) {                 /* MASTER EQ: its three biquads, in at its mix */
        uint32_t k;
        float all = 0.0f;
        for (k = 0; k < 3u; k++)
            all += fx_db(fx + 15u + 5u * k, fx + 18u + 5u * k, 1.0f, w);
        db = 6.0206f * fm_log2f(1.0f + fx[14] * (fm_exp2f(all * 0.16609640f) - 1.0f));   /* (dB to a gain and back) */
    }
    return db;
}

/* the snapshot: after each block, the view and the live floats, written word by word under a sequence count (odd
 * while it is written: a reader copies, then finds the same even count, else tries again; it wraps as it likes). C11's
 * atomics where the compiler has them (an MSVC build: C11 with /experimental:c11atomics), else GCC's and
 * Clang's builtins */
typedef struct {
    frs_view_t view;
    float live[FRS_LIVE];
    float fx[FRS_LIVE_FX];
} snap_t;
#define SNAP_W ((sizeof(snap_t) + 3u) / 4u)
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 201112L && !defined __STDC_NO_ATOMICS__
#include <stdatomic.h>
static _Atomic uint32_t snap_word[SNAP_W], snap_seq, snap_any;
#define A_LOAD(p, o) atomic_load_explicit(p, memory_order_##o)
#define A_STORE(p, v, o) atomic_store_explicit(p, v, memory_order_##o)
#define A_FENCE(o) atomic_thread_fence(memory_order_##o)
#else
static uint32_t snap_word[SNAP_W], snap_seq, snap_any;
#define A_ORDER_relaxed __ATOMIC_RELAXED
#define A_ORDER_acquire __ATOMIC_ACQUIRE
#define A_ORDER_release __ATOMIC_RELEASE
#define A_LOAD(p, o) __atomic_load_n(p, A_ORDER_##o)
#define A_STORE(p, v, o) __atomic_store_n(p, v, A_ORDER_##o)
#define A_FENCE(o) __atomic_thread_fence(A_ORDER_##o)
#endif

static void snap_write(void)
{
    union {
        snap_t s;
        uint32_t w[SNAP_W];
    } u;
    uint32_t i, s = A_LOAD(&snap_seq, relaxed);   /* (the writer's own: even) */
    memset(&u, 0, sizeof u);
    frs_view(&u.s.view);
    eng_live(u.s.live);
    frs_live_fx(u.s.fx);
    A_STORE(&snap_seq, s + 1u, relaxed);
    A_FENCE(release);
    for (i = 0; i < SNAP_W; i++)
        A_STORE(&snap_word[i], u.w[i], relaxed);
    A_STORE(&snap_seq, s + 2u, release);
    A_STORE(&snap_any, 1u, release);
}

static void snap_none(void) { A_STORE(&snap_any, 0u, release); }

static int snap_read(snap_t *out)
{
    union {
        snap_t s;
        uint32_t w[SNAP_W];
    } u;
    uint32_t i, s1, s2, k;
    if (!A_LOAD(&snap_any, acquire))
        return -1;                       /* (no block rendered yet) */
    for (k = 0; k < 64u; k++) {
        s1 = A_LOAD(&snap_seq, acquire);
        if (s1 & 1u)
            continue;
        for (i = 0; i < SNAP_W; i++)
            u.w[i] = A_LOAD(&snap_word[i], relaxed);
        A_FENCE(acquire);
        s2 = A_LOAD(&snap_seq, relaxed);
        if (s1 == s2) {
            *out = u.s;
            return 0;
        }
    }
    return -1;
}

int frs_snapshot(frs_view_t *v, float *live)
{
    snap_t s;
    if (snap_read(&s))
        return -1;
    if (v)
        *v = s.view;
    if (live)
        memcpy(live, s.live, sizeof s.live);
    return 0;
}

int frs_snapshot_fx(float *fx)
{
    snap_t s;
    if (snap_read(&s))
        return -1;
    if (fx)
        memcpy(fx, s.fx, sizeof s.fx);
    return 0;
}

void frs_callbacks(void (*say)(void *user, const char *a, const char *b), void (*program)(void *user, uint32_t n),
                   void *user)
{
    say_cb = say;
    program_cb = program;
    cb_user = user;
}
