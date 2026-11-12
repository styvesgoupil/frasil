/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* ANCHOR, the engine alone (frs_anchor_*), as an iPad would call it, then in a stream.
 *   The API: set, a scene's every setting and its three sequences played on, BACK: each the anchor's, bit for bit; the
 *   notes latched sounding on. MORPH: under way (state 2, how far), the choices at its half, at its end every setting the
 *   anchor's. A setting set meanwhile: the hand's. A scene taken meanwhile: no anchor, no way back. STORE meanwhile:
 *   where it goes. None set: nothing.
 *   A stream: a performance with an ANCHOR set twice, a BACK and a MORPH with a hand in it, played live (the calls in
 *   frs_tick_hook, at their milliseconds) and journaled as a recorder would (FSTJ_SCENE for FST_ANCHOR with
 *   frs_anchor_get's bytes, FST_ANCHOR_BACK, FST_PARAM), through fst_finish and fst_read, played on air: every sample the
 *   live one's. Joined at any place (fst_air_skip) inside the MORPH: the settings and the way back where the render's
 *   are. Controls that must differ: without the BACKs, a MORPH made a BACK.
 *   engine/tests/run.sh, or: anchor */
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

static float L[4096], R[4096];
static void run_ms(uint32_t ms)          /* (256 frames a block, as the FM-1) */
{
    uint32_t end = frs_ms() + ms;
    while (frs_ms() < end)
        frs_render(L, R, 256);
}

/* what an anchor brings back, now: the settings a scene keeps, the three sequences */
typedef struct {
    uint8_t v[256];
    uint8_t seq[32], bnote[32], dnote[32][8];
} kept_t;
static int scene_p(uint32_t p) { return frs_param_kind(p) == FRS_P_SCENE || p == P_TEMPO; }
static void kept(kept_t *k)
{
    frs_view_t v;
    uint32_t p;
    memset(k, 0, sizeof *k);
    for (p = 0; p < frs_param_count(); p++)
        if (scene_p(p))
            k->v[p] = (uint8_t)frs_param_get(p);
    frs_view(&v);
    memcpy(k->seq, v.seq, 32);
    memcpy(k->bnote, v.bnote, 32);
    memcpy(k->dnote, v.dnote, sizeof k->dnote);
}
static int kept_diff(const kept_t *a, const kept_t *b)   /* how many differ */
{
    uint32_t i, n = 0;
    for (i = 0; i < 256u; i++)
        n += a->v[i] != b->v[i];
    for (i = 0; i < 32u; i++)
        n += (a->seq[i] != b->seq[i]) + (a->bnote[i] != b->bnote[i]) + (memcmp(a->dnote[i], b->dnote[i], 8) != 0);
    return (int)n;
}

/* played on: the sound (continuous and choices), the presets, HARMONY, an LFO's TARGET and AMOUNTs, MASTER EQ, each
 * track's PLAYS, RATE, SEQ LENGTH, TEMPO, BASS TONE, LOFI TAPE II, SWELL, a step of each sequence */
static void play_on(void)
{
    static const uint16_t PV[][2] = {
        {P_FILTRE, 10}, {P_TIMBRE, 120}, {P_MX_DRONE, 40}, {P_FL_CUT, 30}, {P_ROOT, 4}, {P_SCALE, 5}, {P_CHORD, 6},
        {P_CH_TYPE, 2}, {P_BS_MODEL, 2}, {P_DR_PRESET, 5}, {P_GL_PRESET, 3}, {P_EQ_PRESET, 4}, {P_L1_T1, 5},
        {P_SQ_LEN, 7}, {P_TEMPO, 120}, {P_BQ_STEPS, 7}, {P_BQ_LEN, 3}, {P_BS_WEIGHT, 90}, {P_BS_SPACE, 70},
        {P_GL_VIB_AUTO, 50}, {P_DQ_LEN, 7}, {P_DQ_RATE, 6}, {P_SQ_TYPE, 2}, {P_BQ_ON, 2}, {P_DQ_ON, 1},
        {P_SQ_STEP, 2}, {P_SQ_NOTE, 60}, {P_BQ_STEP, 1}, {P_BQ_NOTE, 40}, {P_TP_FLUTTER, 100}, {P_TP_NOISE, 60},
        {P_L1_AMT1, 30}, {P_L3_AMT4, 150}, {P_SWELL, 80}};
    uint32_t i;
    static const uint8_t ch[3] = {60, 64, 67};
    for (i = 0; i < sizeof PV / sizeof PV[0]; i++)
        frs_param_set(PV[i][0], PV[i][1]);
    frs_dseq_step(3, ch, 3);
}

static void api_tests(void)
{
    uint8_t sc[FRS_SCENE_BYTES], an[FRS_SCENE_BYTES], st2[FRS_SCENE_BYTES];
    kept_t a, b;
    frs_view_t v;
    uint32_t lat[4], f0;
    float k;
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_scene_factory(0, sc);
    frs_scene_take(sc);
    check(frs_anchor_state(0) == 0 && frs_anchor_get(an) == 0, "none at the start");
    check(frs_anchor_back(0) == -1, "BACK with none: nothing");
    frs_latch(0, 50, FRS_CHORD_HARMONY);
    run_ms(200);
    frs_view(&v);
    memcpy(lat, v.latched, sizeof lat);
    check(lat[0] | lat[1] | lat[2] | lat[3], "chords latched");
    frs_anchor_set();
    kept(&a);
    check(frs_anchor_state(&k) == 1 && k == 0.0f, "set");
    check(frs_anchor_get(an) == FRS_SCENE_BYTES, "its bytes, a scene's");
    play_on();
    run_ms(300);
    kept(&b);
    check(kept_diff(&a, &b) > 25, "played on: more than 25 settings and steps differ");
    check(frs_anchor_back(0) == 0, "BACK");
    kept(&b);
    check(kept_diff(&a, &b) == 0, "BACK: every setting and every step the anchor's, bit for bit");
    check(frs_anchor_state(0) == 0, "BACK: the anchor let go");
    frs_view(&v);
    check(!memcmp(v.latched, lat, sizeof lat), "BACK: the chords latched sound on");
    run_ms(100);
    kept(&b);
    check(kept_diff(&a, &b) == 0, "BACK: still the anchor's after a while");

    /* MORPH */
    frs_anchor_set();
    play_on();
    run_ms(100);
    f0 = frs_param_get(P_FILTRE);
    check(frs_anchor_back(FRS_ANCHOR_MS) == 0, "MORPH");
    check(frs_anchor_state(&k) == 2 && k < 0.05f, "MORPH: under way, at its start");
    check(frs_param_get(P_ROOT) == 4 && frs_param_get(P_SQ_TYPE) == 2, "MORPH: the choices as they were, before its half");
    check(frs_param_get(P_FILTRE) == a.v[P_FILTRE], "MORPH: a setting that glides, its value the anchor's at once");
    check(frs_param_get(P_SWELL) == a.v[P_SWELL], "MORPH: SWELL glides, its value the anchor's at once");
    check(frs_param_get(P_L1_AMT1) == 30u, "MORPH: an LFO's AMOUNT as it was, before its half");
    check(frs_param_heard(P_L1_AMT1) == 30u, "MORPH: an AMOUNT heard as it is (x0.30: it switches at the half)");
    check(frs_param_heard(P_FILTRE) == f0,"MORPH: heard at its start where it was");
    check(frs_param_heard(P_ROOT) == 4, "MORPH: a choice heard as it is");
    run_ms(FRS_ANCHOR_MS / 4u);
    {
        uint32_t h = frs_param_heard(P_FILTRE), lo = a.v[P_FILTRE] < f0 ? a.v[P_FILTRE] : f0, hi = a.v[P_FILTRE] < f0 ? f0 : a.v[P_FILTRE];
        check(hi - lo > 4 && h > lo && h < hi, "MORPH: heard on its way, a quarter in");
    }
    run_ms(FRS_ANCHOR_MS / 4u + 50u);
    check(frs_anchor_state(&k) == 2 && k > 0.5f && k < 0.55f, "MORPH: past its half");
    check(frs_param_get(P_ROOT) == a.v[P_ROOT] && frs_param_get(P_SQ_TYPE) == a.v[P_SQ_TYPE] &&
              frs_param_get(P_DR_PRESET) == a.v[P_DR_PRESET], "MORPH: the choices the anchor's past its half");
    check(frs_param_get(P_L1_AMT1) == a.v[P_L1_AMT1] && frs_param_get(P_L1_T1) == a.v[P_L1_T1],
          "MORPH: an AMOUNT the anchor's past its half, with its TARGET");
    check(frs_param_heard(P_L1_AMT1) == a.v[P_L1_AMT1], "MORPH: an AMOUNT heard the anchor's past its half (a multiplier, 100 x1.00)");
    run_ms(FRS_ANCHOR_MS / 2u);
    check(frs_anchor_state(0) == 0, "MORPH: ended");
    kept(&b);
    check(kept_diff(&a, &b) == 0, "MORPH: every setting and every step the anchor's at its end, bit for bit");
    {
        uint32_t p, same = 1;
        for (p = 0; p < frs_param_count(); p++)
            same &= frs_param_heard(p) == frs_param_get(p);
        check(same && frs_param_heard(frs_param_count()) == 0, "MORPH ended: every setting heard as it is");
    }
    frs_view(&v);
    check(!memcmp(v.latched, lat, sizeof lat), "MORPH: the chords latched sound on");

    /* a hand during it */
    frs_anchor_set();
    play_on();
    frs_anchor_back(FRS_ANCHOR_MS);
    run_ms(500);
    frs_param_set(P_FILTRE, 99);
    frs_param_set(P_ROOT, 8);
    run_ms(FRS_ANCHOR_MS);
    kept(&b);
    check(frs_param_get(P_FILTRE) == 99 && frs_param_get(P_ROOT) == 8, "a hand during MORPH: the hand's");
    check(kept_diff(&a, &b) == 2, "a hand during MORPH: the rest the anchor's");
    frs_param_set(P_FILTRE, a.v[P_FILTRE]);
    frs_param_set(P_ROOT, a.v[P_ROOT]);

    /* STORE during it: where it goes */
    frs_anchor_set();
    play_on();
    frs_anchor_back(FRS_ANCHOR_MS);
    run_ms(300);
    frs_scene_store(st2, sc);
    check(frs_param_get(P_ROOT) == a.v[P_ROOT], "STORE during MORPH: its choices at once");
    check(frs_anchor_state(0) == 2, "STORE during MORPH: it goes on");
    run_ms(FRS_ANCHOR_MS);
    check(!frs_dirty(), "STORE during MORPH: at its end, the sound the scene's as stored");

    /* a scene taken during it */
    frs_anchor_set();
    play_on();
    frs_anchor_back(FRS_ANCHOR_MS);
    run_ms(300);
    frs_scene_factory(2, sc);
    frs_scene_take(sc);
    {
        uint32_t root = frs_param_get(P_ROOT);
        check(frs_anchor_state(0) == 0, "a scene taken during MORPH: no way back");
        run_ms(FRS_ANCHOR_MS);
        check(frs_param_get(P_ROOT) == root, "a scene taken during MORPH: no switch at its half");
    }
    frs_anchor_set();
    frs_scene_take(sc);
    check(frs_anchor_state(0) == 0, "a scene taken while anchored: the anchor gone");

    /* the anchor put as bytes (a replay's) */
    frs_scene_factory(0, sc);
    frs_scene_take(sc);
    frs_anchor_set();
    frs_anchor_get(an);
    kept(&a);
    play_on();
    frs_anchor_drop();
    check(frs_anchor_state(0) == 0 && frs_anchor_back(0) == -1, "dropped: nothing to go back to");
    frs_anchor_put(an);
    frs_anchor_back(0);
    kept(&b);
    check(kept_diff(&a, &b) == 0, "put as its bytes, BACK: the anchor's, bit for bit");
    frs_close();
}

/* ------------------------------------------------------------------ a stream --- */
enum { E_PARAM, E_SET, E_BACK, E_LATCH, E_END };
typedef struct { uint32_t ms; int kind, a, b; } aev_t;
static const aev_t PERF[] = {
    {200, E_LATCH, 50, 0}, {500, E_SET, 0, 0},
    {700, E_PARAM, P_FILTRE, 12}, {760, E_PARAM, P_ROOT, 4}, {820, E_PARAM, P_CH_TYPE, 2}, {900, E_PARAM, P_MX_DRONE, 30},
    {980, E_PARAM, P_SQ_TYPE, 2}, {1040, E_PARAM, P_BS_WEIGHT, 100}, {1100, E_PARAM, P_TEMPO, 110},
    {1150, E_PARAM, P_SWELL, 100}, {1200, E_PARAM, P_L1_AMT1, 40},
    {1300, E_LATCH, 57, 0}, {2000, E_BACK, 0, 0},
    {2600, E_SET, 0, 0},
    {2800, E_PARAM, P_FILTRE, 120}, {2850, E_PARAM, P_TIMBRE, 10}, {2900, E_PARAM, P_SCALE, 5}, {2950, E_PARAM, P_DR_PRESET, 7},
    {3000, E_PARAM, P_EQ_PRESET, 5}, {3050, E_PARAM, P_GL_PRESET, 2}, {3100, E_PARAM, P_DQ_ON, 1},
    {3150, E_PARAM, P_SWELL, 110},
    {3500, E_BACK, 3500, 0}, {4400, E_PARAM, P_TIMBRE, 90}, {8000, E_END, 0, 0}};   /* (a MORPH of 3.5 s, ended in the
                                                                                    * stream: the FM-1's, FRS_ANCHOR_MS, 8 s) */
#define NPERF (sizeof PERF / sizeof PERF[0])

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

static int mode;                         /* 0 all of it, 1 no BACK, 2 the MORPH a BACK */
static uint32_t live_t0, live_at;
static uint8_t jb[1u << 20];
static size_t jn;
static uint32_t jlast;

static void jput(uint32_t ms, uint8_t op, uint8_t n, const uint8_t *a, const void *scene)
{
    fst_ev e;
    memset(&e, 0, sizeof e);
    e.ms = ms;
    e.op = op;
    e.n = n;
    memcpy(e.a, a, 4);                   /* (FSTJ_SCENE: a[0] the op it stands for) */
    jn += fst_jevent(&e, &jlast, scene, 0, jb + jn);
}

static void live_tick(void *user)        /* the calls a host makes, at their milliseconds, and what it journals */
{
    uint32_t now = frs_ms() - live_t0;
    (void)user;
    while (live_at < NPERF && PERF[live_at].ms <= now) {
        const aev_t *e = &PERF[live_at++];
        uint8_t a[4] = {0, 0, 0, 0};
        if (e->kind == E_PARAM) {
            frs_param_set((uint32_t)e->a, (uint32_t)e->b);
            a[0] = (uint8_t)e->a;
            a[1] = (uint8_t)e->b;
            jput(e->ms, FST_PARAM, 2, a, 0);
        } else if (e->kind == E_LATCH) {
            frs_latch(0, (uint32_t)e->a, FRS_CHORD_HARMONY);
            a[1] = (uint8_t)e->a;
            a[2] = (uint8_t)(int8_t)FRS_CHORD_HARMONY;
            jput(e->ms, FST_LATCH, 4, a, 0);
        } else if (e->kind == E_SET) {
            static uint8_t an[FST_SCENE_BYTES];   /* (as a recorder journals a scene: a shard's room, its LEAD) */
            frs_anchor_set();
            frs_anchor_get(an);
            frs_lead_get(an + FRS_SCENE_BYTES);
            a[0] = FST_ANCHOR;
            jput(e->ms, FSTJ_SCENE, 0, a, an);
        } else if (e->kind == E_BACK && mode != 1) {
            uint32_t ms = mode == 2 ? 0u : (uint32_t)e->a;
            frs_anchor_back(ms);
            a[0] = (uint8_t)ms;
            a[1] = (uint8_t)(ms >> 8);
            jput(e->ms, FST_ANCHOR_BACK, 2, a, 0);
        }
    }
}

static fst_start start;
static uint8_t scene0[FRS_SCENE_BYTES];
static uint64_t live(int m, uint8_t **stream, size_t *sn)
{
    fst_ev e;
    mode = m;
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_scene_factory(0, scene0);
    frs_scene_take(scene0);
    fst_capture(&start, 0);
    jlast = 0;
    jn = fst_jhead(&start, 44100, 1793000000, jb);
    fst_fresh(&start, 44100.0f);
    frs_count_in(0);
    frs_record_metronome(0);             /* (as fst_air_open has it) */
    frs_metronome(0);
    live_t0 = frs_ms();
    live_at = 0;
    hash = 1469598103934665603u;
    frs_tick_hook(live_tick, 0);
    while (frs_ms() - live_t0 < 8000u) {
        frs_render(L, R, 256);
        hash_block(256);
    }
    frs_tick_hook(0, 0);
    memset(&e, 0, sizeof e);
    e.ms = 8000;
    e.op = FSTJ_END;
    jn += fst_jevent(&e, &jlast, 0, 0, jb + jn);
    *stream = fst_finish(jb, jn, sn);
    return hash;
}

static fst_air air;
static fst_file file;
static uint64_t replay(const uint8_t *s, size_t n, uint32_t quantum)
{
    uint32_t done = 0;
    int rc = fst_read(s, n, &file);
    if (rc || fst_air_open(&air, &file, 44100.0f)) {
        printf("replay: fst_read %d\n", rc);
        return 0;
    }
    hash = 1469598103934665603u;
    while (fst_air_ms(&air) < 8000u) {
        frs_render(L, R, quantum);
        fst_air_after(&air);
        hash_block(quantum);
        done += quantum;
    }
    (void)done;
    frs_tick_hook(0, 0);
    return hash;
}

static void stream_tests(void)
{
    uint8_t *s0, *s1, *s2;
    size_t n0, n1, n2;
    uint64_t h0 = live(0, &s0, &n0), h1 = live(1, &s1, &n1), h2 = live(2, &s2, &n2), r;
    kept_t a, b;
    float ka, kb;
    int sa, sb;
    uint32_t at;
    check(s0 && s1 && s2, "the takes journaled and finished");
    check(h0 != h1 && h0 != h2 && h1 != h2, "controls: without the BACKs, the MORPH a BACK: other renders");
    r = replay(s0, n0, 256);
    check(r == h0, "replayed on air: every sample the live one's");
    check(file.nscns == 2u, "its scenes beside SCN0: the two anchors");
    check(file.ms == 8000u, "its length");
    check(replay(s1, n1, 256) == h1 && replay(s2, n2, 256) == h2, "the controls replayed: theirs");
    /* joined inside the MORPH (5000 ms: past its half): the way back where a render from the top has it */
    {
        fst_read(s0, n0, &file);
        fst_air_open(&air, &file, 44100.0f);
        while (fst_air_ms(&air) < 5000u) {
            frs_render(L, R, 256);
            fst_air_after(&air);
        }
        kept(&a);
        sa = frs_anchor_state(&ka);
        at = fst_air_ms(&air);
        fst_read(s0, n0, &file);
        fst_air_open(&air, &file, 44100.0f);
        fst_air_skip(&air, at);
        kept(&b);
        sb = frs_anchor_state(&kb);
        check(sa == 2 && sb == 2, "joined inside the MORPH: under way, as rendered");
        check(ka == kb, "joined inside the MORPH: as far");
        check(kept_diff(&a, &b) == 0, "joined inside the MORPH: every setting as rendered");
        while (fst_air_ms(&air) < 8000u) {
            frs_render(L, R, 256);
            fst_air_after(&air);
        }
        kept(&b);
        check(frs_anchor_state(0) == 0, "joined: the MORPH ends");
        frs_tick_hook(0, 0);
    }
    free(s0);
    free(s1);
    free(s2);
    frs_close();
}

int main(void)
{
    api_tests();
    stream_tests();
    printf("anchor: %d ok, %d FAIL\n", oks, fails);
    return fails != 0;
}
