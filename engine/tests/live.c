/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* A performance heard live (fst_live_open, fst_op: Perform LIVE's listener, design/perform/LIVE.md), as stream.wasm's
 * st_live_open and st_op call them: a start captured, then each op given by itself between two blocks, as it comes.
 *   Against the same take journaled as a recorder would (FSTJ_SCENE for its scenes and its state), through fst_finish and
 *   fst_read, replayed by fst_apply between the same two blocks: every sample the replay's, and at seven places every
 *   setting and the notes (latched, held, GLINT's arpeggio and steps, the bass, the drone's steps, the tracks playing).
 *   The ops: settings, the drones and GLINT latched, a key on GLINT, a finger on the bass and its HOLD OFF, MIDI ahead of
 *   its stamp, a scene taken at once, STORE, an ANCHOR given and its way back, a scene gone to (MORPH), a track stopped
 *   and PLAY ALL, a drone step, a state, CUT_STOP then CUT, PLAY, CLEAR, RECORDING; the FM-1's gestures (part A's): a
 *   key's velocity and pressure, a single note (KEY_SINGLE), SWELL, COMPARE, RECORD (LIVE, STEP, OFF), RELEASE,
 *   RELEASE_ALL (RESTORE).
 *   Among them, ops not known (ignored) and ops refused (args too short, a scene's op without its scene, FST_OPEN, a step
 *   of none or ten bytes, a state too long): each counted, the samples the same. Quiet: the start's metronome and
 *   METRONOME set after it. Controls that must differ: a setting more, a scene's op given another scene, another
 *   velocity, the single note a KEY_DOWN, each gesture left out.
 *   engine/tests/run.sh, or: live */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../frasil_engine.h"
#include "../frasil_params.h"
#include "../frasil_stream.h"

static int fails, oks;
static void check(int ok, const char *what)
{
    if (ok)
        oks++;
    else {
        fails++;
        printf("FAIL %s\n", what);
    }
}

static float L[256], R[256];
static uint64_t hash;
static void hash_block(uint32_t n)
{
    uint32_t i, x;
    for (i = 0; i < n; i++) {
        memcpy(&x, &L[i], 4);
        hash = (hash ^ x) * 1099511628211u;
        memcpy(&x, &R[i], 4);
        hash = (hash ^ x) * 1099511628211u;
    }
}

/* the take. scene: -1 none, a factory scene's number (its bytes: FSTJ_SCENE, st_op_scene's), STATE2 the state below.
 * only: 0 both, 1 given live alone (what a file never has: ignored or refused), 2 live alone in a control */
#define STATE2 (-2)
typedef struct { uint32_t ms; uint8_t op, n; uint8_t a[10]; int scene, only, r; } ev_t;
#define END 16000u
#define EV(ms, op, n, a0, a1, a2, a3, sc) {ms, op, n, {a0, a1, a2, a3}, sc, 0, 0}
#define JUNK(ms, op, n, r) {ms, op, n, {0}, -1, 1, r}
static ev_t ev[] = {
    EV(200, FST_LATCH, 4, 0, 48, 0xff, 0, -1),
    EV(300, FST_LATCH, 4, 1, 72, 0xff, 0, -1), EV(301, FST_LATCH, 4, 1, 76, 0xff, 0, -1),
    EV(302, FST_LATCH, 4, 1, 79, 0xff, 0, -1),
    EV(600, FST_PARAM, 2, P_FILTRE, 30, 0, 0, -1),
    JUNK(650, 0x3f, 0, 1), JUNK(651, 0x7e, 2, 1), JUNK(652, 0xbe, 4, 1), JUNK(653, 0xfe, 3, 1),   /* (not known) */
    JUNK(654, FSTJ_SCENE, 0, 1),                                                                  /* (a journal's) */
    JUNK(700, FST_PARAM, 1, -1), JUNK(701, FST_KEY_DOWN, 3, -1), JUNK(702, FST_SCENE_NOW, 0, -1),
    JUNK(703, FST_OPEN, 9, -1), JUNK(704, FST_DSTEP, 10, -1), JUNK(705, FST_DSTEP, 0, -1),
    JUNK(706, FST_STATE, FRS_STATE_BYTES + 1u, -1), JUNK(707, FST_ANCHOR, 2, -1), JUNK(708, FST_LATCH, 4, -1),
    EV(800, FST_KEY_DOWN, 4, 3, 74, 1, 0, -1), EV(1200, FST_KEY_UP, 2, 3, 0, 0, 0, -1),
    EV(1500, FST_HOLD, 4, 2, 40, 1, 0xff, -1),
    EV(2000, FST_MIDI, 4, 0x90, 60, 100, 3, -1),                                                  /* (3 ms ahead) */
    EV(2500, FST_HOLD_OFF, 2, 2, 0, 0, 0, -1), EV(2600, FST_MIDI, 4, 0x80, 60, 0, 0, -1),
    EV(3000, FST_SCENE_NOW, 2, 0, 0, 0, 0, 2),
    EV(3500, FST_LATCH, 4, 0, 55, 0xff, 0, -1),
    EV(4000, FST_STORE, 4, 0, 0, 0, 0, 2),
    EV(4200, FST_ANCHOR, 2, 0, 0, 0, 0, 4), EV(4600, FST_PARAM, 2, P_TIMBRE, 100, 0, 0, -1),
    EV(5000, FST_ANCHOR_BACK, 2, 0xdc, 0x05, 0, 0, -1),                                           /* (1500 ms) */
    EV(6000, FST_PARAM, 2, P_ESPACE, 20, 0, 0, -1),
    EV(7000, FST_SCENE, 2, 0, 0, 0, 0, 5),
    EV(7500, FST_TRACK_PLAY, 2, 1, 0, 0, 0, -1), EV(8000, FST_PLAY_ALL, 2, 1, 0, 0, 0, -1),
    {8500, FST_DSTEP, 4, {0, 60, 64, 67}, -1, 0, 0}, EV(8501, FST_PARAM, 2, P_DQ_ON, 1, 0, 0, -1),
    EV(9000, FST_STATE, 0, 0, 0, 0, 0, STATE2),
    EV(9500, FST_CUT_STOP, 0, 0, 0, 0, 0, -1), EV(9500, FST_CUT, 0, 0, 0, 0, 0, -1),   /* (as a recorder writes the
                                                                                         * iPad's CUT: the CUT skipped) */
    EV(10000, FST_PLAY, 0, 0, 0, 0, 0, -1), EV(10500, FST_CLEAR, 2, 0, 0, 0, 0, -1),
    EV(11000, FST_RECORDING, 4, 0, 1, 0, 0, -1), EV(11200, FST_LATCH, 4, 0, 52, 0xff, 0, -1),
    /* the FM-1's gestures (part A's): a key's velocity and pressure, a single note, ENV held, COMPARE, REC, RELEASE ALL
     * and RESTORE */
    EV(11500, FST_KEY_DOWN, 4, 4, 64, 0, 30, -1),                                                 /* (velocity 30) */
    EV(11800, FST_KEY_PRESSURE, 2, 4, 110, 0, 0, -1), EV(12000, FST_KEY_PRESSURE, 2, 4, 20, 0, 0, -1),
    EV(12300, FST_KEY_UP, 2, 4, 0, 0, 0, -1),
    EV(12400, FST_KEY_SINGLE, 4, 5, 62, 0, 90, -1), EV(12450, FST_PARAM, 2, P_SWELL, 0, 0, 0, -1),   /* (ENV's alone) */
    EV(12500, FST_SWELL, 2, 1, 0, 0, 0, -1), EV(12600, FST_KEY_PRESSURE, 2, 5, 70, 0, 0, -1),
    EV(12900, FST_KEY_UP, 2, 5, 0, 0, 0, -1), EV(13200, FST_SWELL, 2, 0, 0, 0, 0, -1),
    EV(13250, FST_PARAM, 2, P_TIMBRE, 20, 0, 0, -1),                                             /* (COMPARE's edit) */
    EV(13300, FST_COMPARE, 2, 1, 0, 0, 0, -1), EV(13700, FST_COMPARE, 2, 0, 0, 0, 0, -1),
    EV(13800, FST_RECORDING, 4, 0, 1, 0, 0, -1), EV(13800, FST_RECORD, 2, 1, 0, 0, 0, -1),     /* (GLINT: LIVE) */
    EV(13900, FST_KEY_DOWN, 4, 6, 76, 1, 80, -1), EV(14100, FST_KEY_UP, 2, 6, 0, 0, 0, -1),
    EV(14300, FST_RECORD, 2, 1, 0, 0, 0, -1), EV(14400, FST_RECORD, 2, 1, 0, 0, 0, -1),         /* (STEP, OFF) */
    EV(14800, FST_RELEASE, 0, 0, 0, 0, 0, -1), EV(15300, FST_RELEASE_ALL, 0, 0, 0, 0, 0, -1)};  /* (RESTORE) */
#define NEV (sizeof ev / sizeof ev[0])
#define NAT 7u

static fst_start start;
static uint8_t state2[FRS_SCENE_BYTES];  /* (a state, as FSTJ_SCENE carries it: in a scene's room) */
static uint32_t state2_n;

/* the start, captured from an engine as a performer left it; the state that comes later (ARP SNAP another) */
static void capture(void)
{
    uint8_t sc[FRS_SCENE_BYTES];
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_scene_factory(1, sc);
    frs_scene_take(sc);
    frs_latch(0, 45, FRS_CHORD_HARMONY);
    frs_param_set(P_DESACCORD, 70);
    frs_render(L, R, 256);
    frs_param_set(P_ARP_SNAP, 3);
    state2_n = frs_state_save(state2);
    frs_param_set(P_ARP_SNAP, 1);
    check(frs_param_get(P_METRO) >= 1u, "the start's state: METRONOME on (REC, the FM-1's at the start)");
    fst_capture(&start, 0);
    frs_close();
}

/* what an op leaves, at a place: every setting and the notes */
typedef struct {
    uint8_t v[256];
    frs_view_t w;
} kept_t;
static void kept(kept_t *k)
{
    uint32_t p;
    memset(k, 0, sizeof *k);
    for (p = 0; p < frs_param_count() && p < 256u; p++)
        k->v[p] = (uint8_t)frs_param_get(p);
    frs_view(&k->w);
}
static int kept_same(const kept_t *a, const kept_t *b)
{
    const frs_view_t *x = &a->w, *y = &b->w;
    return !memcmp(a->v, b->v, sizeof a->v) && !memcmp(x->latched, y->latched, sizeof x->latched) &&
           !memcmp(x->held, y->held, sizeof x->held) && x->narp == y->narp && !memcmp(x->arp, y->arp, sizeof x->arp) &&
           !memcmp(x->seq, y->seq, sizeof x->seq) && !memcmp(x->bnote, y->bnote, sizeof x->bnote) && x->bass == y->bass &&
           !memcmp(x->dnote, y->dnote, sizeof x->dnote) && x->playing == y->playing && x->seq_pos == y->seq_pos &&
           x->bseq_pos == y->bseq_pos && x->dseq_pos == y->dseq_pos && !memcmp(x->kept, y->kept, sizeof x->kept);
}
static const uint32_t AT[NAT] = {1000, 3600, 6000, 9600, 13000, 14200, END};   /* (13000: a key pressed, the drone
                                                                                * swelling; 14200: GLINT recording) */
static kept_t at_ref[NAT], at_live[NAT], at_open;

/* ------------------------------------------------------------------ the replay: a file, fst_apply */
static uint8_t jb[1u << 16];
static uint8_t *journal(size_t *sn)
{
    fst_ev e;
    uint32_t last = 0, k;
    size_t jn = fst_jhead(&start, 44100, 1793000000, jb);
    for (k = 0; k < NEV; k++) {
        if (ev[k].only)
            continue;
        memset(&e, 0, sizeof e);
        e.ms = ev[k].ms;
        if (ev[k].scene == STATE2) {
            e.op = FSTJ_SCENE;
            e.a[0] = FST_STATE;
            e.a[2] = (uint8_t)state2_n;
            jn += fst_jevent(&e, &last, state2, 0, jb + jn);
        } else if (ev[k].scene >= 0) {
            static uint8_t sc[FRS_SCENE_BYTES];
            frs_scene_factory((uint32_t)ev[k].scene, sc);
            e.op = FSTJ_SCENE;
            e.a[0] = ev[k].op;
            jn += fst_jevent(&e, &last, sc, 0, jb + jn);
        } else {
            e.op = ev[k].op;
            e.n = ev[k].n;
            memcpy(e.a, ev[k].a, sizeof ev[k].a);
            jn += fst_jevent(&e, &last, 0, 0, jb + jn);
        }
    }
    memset(&e, 0, sizeof e);
    e.ms = END;
    e.op = FSTJ_END;
    jn += fst_jevent(&e, &last, 0, 0, jb + jn);
    return fst_finish(jb, jn, sn);
}

static fst_file file;
static fst_player player;
static uint64_t replay(const uint8_t *s, size_t n)
{
    fst_start st;
    fst_dec e;
    uint64_t frames = 1;                 /* (fst_fresh's first block) */
    uint32_t t0, i = 0;
    if (fst_read(s, n, &file)) {
        printf("replay: fst_read refused it\n");
        return 0;
    }
    fst_start_of(&file, &st);
    fst_fresh(&st, 44100.0f);
    frs_count_in(0);                     /* (quiet, as on air and live) */
    frs_record_metronome(0);
    frs_metronome(0);
    fst_begin(&player, &file);
    t0 = frs_ms();
    hash = 1469598103934665603u;
    while (frs_ms() - t0 < END) {
        while (fst_next(&player, frs_ms() - t0, &e)) {
            fst_apply(&player, &e, frames);
            if (e.op == FST_STATE) {
                frs_record_metronome(0);
                frs_metronome(0);
            }
        }
        while (i < NAT && frs_ms() - t0 >= AT[i])
            kept(&at_ref[i++]);
        frs_render(L, R, 256);
        hash_block(256);
        frames += 256u;
    }
    while (i < NAT)
        kept(&at_ref[i++]);
    return hash;
}

/* ------------------------------------------------------------------ live: fst_op, each op as it comes */
static fst_live lv;
static uint32_t said_missed, said_refused, said_wrong;
/* control: 0 none, 1 a setting more, 2 the scene taken at once another, 3 a key's velocity another (30: 127), 4 the
 * single note a KEY_DOWN, SKIP + op: that op's events not given */
#define SKIP 0x100
static uint64_t live(int control)
{
    uint32_t t0, k = 0, i = 0;
    said_missed = said_refused = said_wrong = 0;
    if (fst_live_open(&lv, &start, 44100.0f)) {
        printf("live: fst_live_open refused it\n");
        return 0;
    }
    kept(&at_open);
    t0 = frs_ms();
    hash = 1469598103934665603u;
    while (frs_ms() - t0 < END) {
        while (k < NEV && ev[k].ms <= frs_ms() - t0) {
            const ev_t *e = &ev[k++];
            static uint8_t sc[FRS_SCENE_BYTES];
            uint8_t a[4];
            int r;
            if (control == SKIP + e->op) {
                said_wrong += e->r != 0;
                continue;
            }
            if (control == 3 && e->op == FST_KEY_DOWN && e->a[3] == 30u) {
                memcpy(a, e->a, 4);
                a[3] = 127;
                r = fst_op(&lv, e->op, a, 4, 0);
            } else if (control == 4 && e->op == FST_KEY_SINGLE)
                r = fst_op(&lv, FST_KEY_DOWN, e->a, e->n, 0);
            else if (e->scene == STATE2)
                r = fst_op(&lv, e->op, state2, state2_n, 0);
            else if (e->scene >= 0) {
                frs_scene_factory((uint32_t)(control == 2 && e->op == FST_SCENE_NOW ? 6 : e->scene), sc);
                r = fst_op(&lv, e->op, 0, 0, sc);
            } else if (e->only)          /* (a LATCH's args not there at all) */
                r = fst_op(&lv, e->op, e->op == FST_LATCH ? 0 : e->a, e->n, 0);
            else
                r = fst_op(&lv, e->op, e->a, e->n, 0);
            said_wrong += r != e->r;
            said_missed += r == 1;
            said_refused += r == -1;
        }
        if (control == 1 && frs_ms() - t0 >= 5200u && frs_ms() - t0 < 5206u) {
            static const uint8_t a[2] = {P_PENTE, 9};
            fst_op(&lv, FST_PARAM, a, 2, 0);
        }
        while (i < NAT && frs_ms() - t0 >= AT[i])
            kept(&at_live[i++]);
        frs_render(L, R, 256);
        hash_block(256);
    }
    while (i < NAT)
        kept(&at_live[i++]);
    return hash;
}

int main(void)
{
    uint8_t *s;
    size_t sn = 0;
    uint64_t ref, got, c1, c2;
    uint32_t i, nk = 0, nm = 0, nr = 0;
    char what[96];
    for (i = 0; i < NEV; i++) {
        if (i && ev[i].ms < ev[i - 1u].ms) {
            printf("FAIL the take: not in order at %u ms\n", ev[i].ms);
            fails++;
        }
        nk += !ev[i].only;
        nm += ev[i].r == 1;
        nr += ev[i].r == -1;
    }
    capture();
    s = journal(&sn);
    check(s != 0, "the take journaled and finished");
    if (!s)
        return 1;
    ref = replay(s, sn);
    check(ref != 0, "replayed from its file");
    got = live(0);
    check(got == ref, "live, op by op: every sample the replay's");
    for (i = 0; i < NAT; i++) {
        snprintf(what, sizeof what, "live at %u ms: every setting and the notes the replay's", AT[i]);
        check(kept_same(&at_ref[i], &at_live[i]), what);
    }
    check(lv.done == nk, "each op of the take done");
    check(lv.missed == nm && said_missed == nm, "ops not known: ignored, counted");
    check(lv.refused == nr && said_refused == nr, "ops too short, a scene's without it, FST_OPEN, a step, a state: refused, counted");
    check(said_wrong == 0, "each op's answer: 0 done, 1 not known, -1 refused");
    check(frs_param_get(P_METRO) == 0u, "quiet: the start's metronome (on in the state) off");
    check(frs_param_get(P_ARP_SNAP) == 3u && at_ref[2].v[P_ARP_SNAP] == 1u, "the state taken: ARP SNAP its, the start's before");
    {
        static const uint8_t m[2] = {P_METRO, 2};
        int r = fst_op(&lv, FST_PARAM, m, 2, 0);
        frs_render(L, R, 256);           /* (P_METRO shows the metronome as it is, from the next millisecond) */
        check(r == 0 && frs_param_get(P_METRO) == 0u, "METRONOME set: quiet still");
    }
    check(at_live[0].v[P_FILTRE] == 30u && at_open.v[P_FILTRE] != 30u &&
          memcmp(at_live[0].w.latched, at_open.w.latched, sizeof at_open.w.latched), "at 1000 ms: FILTER, the drones latched (the ops act)");
    check(memcmp(at_ref[1].v, at_ref[0].v, sizeof at_ref[0].v) != 0, "the scene taken at once: its settings");
    c1 = live(1);
    c2 = live(2);
    check(c1 != ref && c2 != ref && c1 != c2, "controls: a setting more, another scene: other renders");
    check(!kept_same(&at_ref[1], &at_live[1]), "control: another scene, other settings at 3600 ms");
    check(live(3) != ref, "control: KEY_DOWN's velocity heard (30 against 127)");
    check(live(4) != ref, "control: KEY_SINGLE a single note (a KEY_DOWN: the chord)");
    {   /* each gesture of the FM-1's acts: without it, another render */
        static const uint8_t G[] = {FST_KEY_PRESSURE, FST_SWELL, FST_COMPARE, FST_RECORD, FST_RELEASE, FST_RELEASE_ALL};
        static const char *GN[] = {"KEY_PRESSURE", "SWELL", "COMPARE", "RECORD", "RELEASE", "RELEASE_ALL"};
        for (i = 0; i < sizeof G; i++) {
            snprintf(what, sizeof what, "control: without %s, another render", GN[i]);
            check(live(SKIP + G[i]) != ref, what);
        }
        live(SKIP + FST_RECORD);
        check(!kept_same(&at_ref[5], &at_live[5]), "control: without RECORD, GLINT's take not there at 14200 ms");
    }
    free(s);
    frs_close();
    printf("live: %d ok, %d FAIL\n", oks, fails);
    return fails != 0;
}
