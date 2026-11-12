/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The LEAD in a shard, a stream and Perform LIVE (card #152), on the engine as STREAM's stream.wasm builds it (FRS_LEAD,
 * dsp/lead.c):
 *   MOSS (frs_lead_none) each setting's default; frs_lead_take as a scene's (past its max its default, saved: not dirty;
 *   a MODE changed so, its voice held on), frs_lead_put as an edit (dirty; MODE changed: let go), frs_lead_get, the
 *   wheels; frs_param_edit on its settings (the River's editor's web_param), PRESET a preset's sound, CUSTOM (its
 *   last) its name alone, a PRESET past the presets taken CUSTOM (admin #870, #875).
 *   The ops, one by one (fst_op): FST_KEY_LEAD, FST_KEY_UP on it, FST_LEAD (how 1, how 0; too short: refused),
 *   FST_LEAD_WHEELS, FST_PARAM16 on a LEAD setting past 255 (PRESET: a preset's sound), FST_PARAM on one under it; a
 *   start's LEAD taken with it.
 *   A take with the LEAD (a start with one, its keys MONO gliding, the wheels, POLY, a scene gone to with its own LEAD,
 *   the LEAD put whole) journaled, finished ("FRS~") and played on air: every sample the same take heard live op by op,
 *   its scenes the FM-1's scene_t and their LEAD by FST_LEAD after them; on air twice, the same (the control: the
 *   scene's LEAD MOSS in the file, another sound); and an "FRS}" stream (3.2's) on this engine: its LEAD MOSS.
 *   The running state's LEAD part ("FRld"): A plays the take, its keys on the LEAD held across a snap; B opened on the
 *   snap's two parts: every 100 ms A's brain (the first part's and the LEAD's: its keys, its settings, the voices' notes,
 *   gates and stages, their envelopes within half precision), and at the end; C opened on the first part alone (a page
 *   that keeps it alone): the keys on the LEAD let go, none stuck after; D as A, the snap at its place: left, the very
 *   same, its two parts or its first alone.
 *   engine/tests/run.sh, or: lead */
#include "../frasil_engine.c"
#include "../frasil_stream.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if !FRS_LEAD
#error "the LEAD's test: built with -DFRS_LEAD=1 and dsp/lead.c (engine/tests/run.sh)"
#endif

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
static void blocks(uint32_t n)
{
    while (n--)
        frs_render(L, R, 256);
}
static lead_view_t view(void)
{
    lead_view_t v;
    frasil_lead_view(&v);
    return v;
}
static int lead_is(const uint8_t *b)
{
    uint8_t g[FRS_LEAD_BYTES];
    frs_lead_get(g);
    return !memcmp(g, b, sizeof g);
}

/* ------------------------------------------------------------------ the API */
static void api(void)
{
    uint8_t m[FRS_LEAD_BYTES], b[FRS_LEAD_BYTES], sc[FRS_SCENE_BYTES];
    uint32_t i, ok = 1, def, max;
    frs_lead_none(m);
    for (i = 0; i < FRS_LEAD_BYTES; i++)
        ok &= !frs_param_info(P_LD_WAVE + i, 0, &max, &def) && m[i] == def;
    check(ok && FRS_SHARD_BYTES == FRS_SCENE_BYTES + FRS_LEAD_BYTES && P_LD_WAVE + FRS_LEAD_BYTES == NPARAM,
          "MOSS: each LEAD setting its default, the settings' last 25");
    frs_close();
    frs_init(44100.0f);
    frs_scene_factory(0, sc);
    frs_scene_take(sc);
    check(lead_is(m), "an engine opened: its LEAD MOSS");
    frs_param_set(P_KB, 3);              /* MONO, a key held */
    frs_key_down(0, 60, 100, 0);
    blocks(4);
    check(view().held == 1u, "a key on the LEAD: its voice held");
    memcpy(b, m, sizeof b);
    b[P_LD_CUT - P_LD_WAVE] = 90;
    b[P_LD_MODE - P_LD_WAVE] = 1;
    b[P_LD_WAVE - P_LD_WAVE] = 200;      /* (past its max: its default) */
    frs_lead_take(b);
    b[0] = m[0];
    check(lead_is(b) && !frs_dirty(), "frs_lead_take: as a scene's, past its max its default, saved (not dirty)");
    blocks(4);
    check(view().held == 1u, "frs_lead_take, MODE changed: the voice held on (as the FM-1's scene load)");
    frs_key_up(0);                       /* (POLY now: its note's voice let go) */
    blocks(4);
    check(view().held == 0u, "its key up in the new MODE: let go");
    frs_key_down(1, 64, 100, 0);
    frs_key_down(2, 67, 100, 0);
    blocks(4);
    check(view().held == 2u, "POLY: two voices held");
    b[P_LD_MODE - P_LD_WAVE] = 0;
    b[P_LD_CUT - P_LD_WAVE] = 70;
    frs_lead_put(b);
    blocks(4);
    check(lead_is(b) && frs_dirty() && view().held == 0u, "frs_lead_put: an edit (dirty), MODE changed: its voices let go");
    frs_key_up(1);
    frs_key_up(2);
    frs_lead_wheels(127u, 8191);
    frs_key_down(3, 60, 100, 0);
    blocks(4);
    check(fabsf(view().mod - 1.0f) < 1e-6f, "the wheels: mod 127 is 1");
    frs_key_up(3);
    frs_param_edit(P_LD_CUT, 77);        /* (the River's editor's way: web_param, frs_param_edit) */
    frs_param_edit(P_LD_DFB, 99);
    check(frs_param_get(P_LD_CUT) == 77u && frs_param_get(P_LD_DFB) == 99u, "frs_param_edit: the LEAD's settings edited");
    frs_param_edit(P_LD_PRESET, 9);
    check(frs_param_get(P_LD_PRESET) == 9u && frs_param_get(P_LD_CUT) == 48u, "frs_param_edit on PRESET: its sound");
    {                                    /* CUSTOM (admin #873, #875): PRESET's last, no preset's sound; a number past
                                          * the presets taken (a scene's, a shard's: one stored before admin #870's
                                          * twenty) CUSTOM, its settings as they came */
        uint32_t max, def;
        char w[FRS_TEXT];
        frs_param_info(P_LD_PRESET, 0, &max, &def);
        frs_param_text(P_LD_PRESET, max, w);
        frs_param_edit(P_LD_CUT, 66);
        frs_param_edit(P_LD_PRESET, max);
        check(max == 20u && def == 0u && !strcmp(w, "CUSTOM") && frs_param_get(P_LD_PRESET) == max &&
                  frs_param_get(P_LD_CUT) == 66u && frs_param_get(P_LD_WAVE) == 56u,
              "PRESET's CUSTOM, its last (20): its name alone, the settings as they are (HONEY's, FILTER 66)");
        frs_param_text(P_LD_PRESET, 19, w);
        check(!strcmp(w, "THORN"), "PRESET 19: THORN, the last preset");
        memcpy(b, m, sizeof b);
        b[P_LD_PRESET - P_LD_WAVE] = 31; /* (TEMPEST's before admin #870) */
        b[P_LD_CUT - P_LD_WAVE] = 33;
        frs_lead_take(b);
        check(frs_param_get(P_LD_PRESET) == max && frs_param_get(P_LD_CUT) == 33u && frs_param_get(P_LD_WAVE) == m[0],
              "frs_lead_take, PRESET past the presets (31): CUSTOM, the settings as they came");
    }
    {                                    /* its settings a scene's; LFO TARGET's 62 .. 83 its 22, as the FM-1's */
        static const uint16_t T[22] = {P_LD_CUT, P_LD_RESO, P_LD_WAVE, P_LD_DETUNE, P_LD_BREATH, P_LD_DRIVE, P_LD_GLIDE,
                                       P_LD_FENV, P_LD_VIB, P_LD_ATTACK, P_LD_DECAY, P_LD_SUSTAIN, P_LD_RELEASE,
                                       P_LD_DTIME, P_LD_DFB, P_LD_DMIX, P_LD_SPACE, P_MX_LEAD, P_LD_FATTACK,
                                       P_LD_FDECAY, P_LD_FSUSTAIN, P_LD_FRELEASE};
        char w[FRS_TEXT];
        uint32_t k = 1;
        for (i = 0; i < FRS_LEAD_BYTES; i++)
            k &= frs_param_kind(P_LD_WAVE + i) == FRS_P_SCENE;
        for (i = 0; i < 22u; i++)
            k &= frs_lfo_target(62u + i) == (int)T[i];
        frs_param_text(P_LD_PRESET, 9, w);
        check(k && frs_lfo_targets() == 83u && frs_lfo_target(84) == -1 && !strcmp(w, "HONEY"),
              "its settings FRS_P_SCENE's, LFO TARGET 62 .. 83 its 22, their words (PRESET 9 HONEY)");
    }
    {                                    /* what sounds (frs_view); PANIC and CUT silence it */
        frs_view_t v;
        frs_param_set(P_LD_MODE, 1);
        frs_key_down(4, 60, 100, 0);
        frs_key_down(5, 64, 100, 0);
        blocks(8);
        frs_view(&v);
        check(v.lead_held == 2u && v.lead_note[0] >= 0 && v.lead_note[1] >= 0 && v.lead_level[0] > 0.1f &&
                  v.lead_meter > 0.0f, "frs_view: the LEAD's voices, their levels, its meter");
        frs_panic();
        blocks(2);
        frs_view(&v);
        check(v.lead_note[0] < 0 && v.lead_note[1] < 0 && view().sounding == 0u, "frs_panic: the LEAD silent");
        frs_key_down(6, 67, 100, 0);
        blocks(8);
        frs_cut();
        blocks(4);
        check(view().sounding == 0u, "frs_cut: the LEAD silent");
        frs_key_up(6);
    }
}

/* ------------------------------------------------------------------ the ops, one by one */
static fst_live lv;
static fst_start st0;
static uint8_t lead0[FRS_LEAD_BYTES];
static void start0(void)                 /* a factory scene, the LEAD HONEY's (its preset 9) and FILTER 40 */
{
    uint8_t sc[FRS_SCENE_BYTES];
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_scene_factory(1, sc);
    frs_scene_take(sc);
    frs_param_set(P_LD_PRESET, 9);
    frs_param_set(P_LD_CUT, 40);
    fst_capture(&st0, 0);
    memcpy(lead0, st0.scene + FRS_SCENE_BYTES, sizeof lead0);
}

static void ops(void)
{
    uint8_t a[2u + FRS_LEAD_BYTES], m[FRS_LEAD_BYTES];
    static const uint8_t down[4] = {1, 62, 3, 90}, up[2] = {1, 0}, wh[4] = {64, 0x00, 0x30, 0};
    static const uint8_t pre[4] = {P_LD_PRESET & 0xFF, P_LD_PRESET >> 8, 9, 0}, lo[2] = {P_LD_GLIDE, 99};
    lead_view_t v;
    start0();
    check(lead0[P_LD_PRESET - P_LD_WAVE] == 9 && lead0[P_LD_CUT - P_LD_WAVE] == 40,
          "fst_capture: the LEAD after the scene");
    check(!fst_live_open(&lv, &st0, 44100.0f) && lead_is(lead0), "a start's LEAD, taken with it");
    check(!fst_op(&lv, FST_KEY_LEAD, down, 4, 0), "FST_KEY_LEAD: done");
    blocks(4);
    v = view();
    check(v.held == 1u && v.note == 62 + 12 * ((int)frs_param_get(P_LD_OCTAVE) - 2) && frs_param_get(P_KB) == 3u,
          "FST_KEY_LEAD: its note on the LEAD, OCTAVE on it, the keys on it");
    fst_op(&lv, FST_KEY_UP, up, 2, 0);
    blocks(4);
    check(view().held == 0u, "FST_KEY_UP: its voice let go");
    frs_lead_none(m);
    a[0] = 1;
    memcpy(a + 1, m, sizeof m);
    check(!fst_op(&lv, FST_LEAD, a, 1u + FRS_LEAD_BYTES, 0) && lead_is(m) && !frs_dirty(),
          "FST_LEAD how 1: a scene's, saved");
    a[0] = 0;
    a[1 + P_LD_CUT - P_LD_WAVE] = 99;
    check(!fst_op(&lv, FST_LEAD, a, 1u + FRS_LEAD_BYTES, 0) && frs_param_get(P_LD_CUT) == 99u && frs_dirty(),
          "FST_LEAD how 0: put, an edit");
    check(fst_op(&lv, FST_LEAD, a, FRS_LEAD_BYTES, 0) == -1 && lv.refused == 1u, "FST_LEAD too short: refused");
    check(!fst_op(&lv, FST_LEAD_WHEELS, wh, 4, 0), "FST_LEAD_WHEELS: done");
    fst_op(&lv, FST_KEY_LEAD, down, 4, 0);
    blocks(4);
    check(fabsf(view().mod - 64.0f / 127.0f) < 1e-6f, "FST_LEAD_WHEELS: the mod wheel as told");
    fst_op(&lv, FST_KEY_UP, up, 2, 0);
    check(!fst_op(&lv, FST_PARAM16, pre, 4, 0) && frs_param_get(P_LD_PRESET) == 9u && frs_param_get(P_LD_CUT) == 48u,
          "FST_PARAM16 on PRESET (260): the preset's sound (HONEY's FILTER)");
    check(!fst_op(&lv, FST_PARAM, lo, 2, 0) && frs_param_get(P_LD_GLIDE) == 99u, "FST_PARAM on GLIDE (247)");
}

/* ------------------------------------------------------------------ a take, on air and live */
#define END 6000u
#define SNAP 1700u
enum { O_OP, O_SCENE, O_LEAD };          /* (an op; a scene gone to with its LEAD L2; the LEAD put whole, L3) */
typedef struct {
    uint32_t ms;
    uint8_t kind, op, n, a[4];
} tk_t;
static const tk_t TAKE[] = {
    {200, O_OP, FST_KEY_LEAD, 4, {1, 60, 3, 100}},  {600, O_OP, FST_KEY_LEAD, 4, {2, 67, 3, 90}},   /* (MONO: gliding) */
    {900, O_OP, FST_LEAD_WHEELS, 4, {80, 0x00, 0x28, 0}},                                    /* (+2048: a semitone) */
    {1200, O_OP, FST_KEY_UP, 2, {2, 0, 0, 0}},
    {1500, O_OP, FST_PARAM16, 4, {P_LD_MODE & 0xFF, P_LD_MODE >> 8, 1, 0}},                  /* POLY (261) */
    {1600, O_OP, FST_KEY_LEAD, 4, {3, 55, 3, 80}}, {1610, O_OP, FST_KEY_LEAD, 4, {4, 59, 3, 80}},
    {1620, O_OP, FST_KEY_LEAD, 4, {5, 62, 3, 80}}, {1650, O_OP, FST_PARAM, 2, {P_LD_GLIDE, 60, 0, 0}},
    /* (the snap at 1700: four keys on it) */
    {2500, O_SCENE, FST_SCENE, 0, {0}},            {2800, O_OP, FST_KEY_UP, 2, {3, 0, 0, 0}},
    {2810, O_OP, FST_KEY_UP, 2, {4, 0, 0, 0}},     {2820, O_OP, FST_KEY_UP, 2, {5, 0, 0, 0}},
    {2830, O_OP, FST_KEY_UP, 2, {1, 0, 0, 0}},     {3200, O_LEAD, FST_LEAD, 0, {0}},
    {3300, O_OP, FST_KEY_LEAD, 4, {6, 64, 3, 110}}, {3900, O_OP, FST_KEY_PRESSURE, 2, {6, 90, 0, 0}},
    {4200, O_OP, FST_KEY_UP, 2, {6, 0, 0, 0}},     {4300, O_OP, FST_LATCH, 4, {0, 48, 0xFF, 0}},
};
#define NTAKE (sizeof TAKE / sizeof TAKE[0])
static uint8_t sc2[FST_SCENE_BYTES], lead3[1u + FRS_LEAD_BYTES];

static void take_parts(void)             /* L2: the factory scene 2 and PRISM's LEAD (14), POLY; L3: put, FILTER 30 */
{
    frs_close();
    frs_init(44100.0f);
    frs_scene_factory(2, sc2);
    frs_scene_take(sc2);
    frs_param_set(P_LD_PRESET, 14);
    frs_param_set(P_LD_MODE, 1);
    frs_lead_get(sc2 + FRS_SCENE_BYTES);
    lead3[0] = 0;
    memcpy(lead3 + 1, sc2 + FRS_SCENE_BYTES, FRS_LEAD_BYTES);
    lead3[1 + P_LD_CUT - P_LD_WAVE] = 30;
}

static uint8_t *journal(size_t *out)     /* the take as a recorder journals it, finished */
{
    static uint8_t jb[65536];
    static uint8_t room[FST_SCENE_BYTES];
    uint32_t last = 0, k;
    size_t jn = fst_jhead(&st0, 44100, 1793000000, jb);
    fst_ev e;
    for (k = 0; k < NTAKE; k++) {
        memset(&e, 0, sizeof e);
        e.ms = TAKE[k].ms;
        if (TAKE[k].kind == O_SCENE) {
            e.op = FSTJ_SCENE;
            e.a[0] = FST_SCENE;
            jn += fst_jevent(&e, &last, sc2, 0, jb + jn);
        } else if (TAKE[k].kind == O_LEAD) {
            e.op = FSTJ_SCENE;           /* (FST_LEAD inline, as a state: fst_ev holds 14) */
            e.a[0] = FST_LEAD;
            e.a[2] = (uint8_t)sizeof lead3;
            memcpy(room, lead3, sizeof lead3);
            jn += fst_jevent(&e, &last, room, 0, jb + jn);
        } else {
            e.op = TAKE[k].op;
            e.n = TAKE[k].n;
            memcpy(e.a, TAKE[k].a, 4);
            jn += fst_jevent(&e, &last, 0, 0, jb + jn);
        }
    }
    memset(&e, 0, sizeof e);
    e.ms = END;
    e.op = FSTJ_END;
    jn += fst_jevent(&e, &last, 0, 0, jb + jn);
    return fst_finish(jb, jn, out);
}

static fst_air air;
static fst_file file;
static uint64_t on_air(const uint8_t *s, size_t n, int *lead_at_open)
{
    if (fst_read(s, n, &file) || fst_air_open(&air, &file, 44100.0f))
        return 0;
    *lead_at_open = lead_is(lead0);
    hash = 1469598103934665603u;
    while (fst_air_ms(&air) < END) {
        frs_render(L, R, 256);
        fst_air_after(&air);
        hash_block(256);
    }
    return hash;
}

/* the take given to fst_live op by op, each in its millisecond (fst_live_at), from ms0 its start's: those after from, up
 * to `to` (fst_live's queue in their order) */
static uint32_t ms0;
static void give_to(uint32_t from, uint32_t to)
{
    uint32_t k;
    for (k = 0; k < NTAKE; k++) {
        const tk_t *t = &TAKE[k];
        if (t->ms <= from || t->ms > to)
            continue;
        if (t->kind == O_SCENE) {        /* (the FM-1's scene_t, then its LEAD as a scene's) */
            uint8_t a[1u + FRS_LEAD_BYTES];
            a[0] = 1;
            memcpy(a + 1, sc2 + FRS_SCENE_BYTES, FRS_LEAD_BYTES);
            fst_live_at(&lv, ms0 + t->ms, FST_SCENE, 0, 0, sc2);
            fst_live_at(&lv, ms0 + t->ms, FST_LEAD, a, sizeof a, 0);
        } else if (t->kind == O_LEAD) {
            fst_live_at(&lv, ms0 + t->ms, FST_LEAD, lead3, sizeof lead3, 0);
        } else {
            fst_live_at(&lv, ms0 + t->ms, t->op, t->a, t->n, 0);
        }
    }
}
static void give(uint32_t from) { give_to(from, END); }
static void run_to(uint32_t ms, int hashing)   /* blocks until the engine's clock reaches ms0 + ms */
{
    while (frs_ms() < ms0 + ms) {
        frs_render(L, R, 256);
        if (hashing)
            hash_block(256);
    }
}

static uint32_t brain_of(uint8_t *out, const uint8_t *scene)   /* the brain, both parts (snap.c's) */
{
    static uint8_t ld[LIVE_LEAD_BYTES];
    uint32_t m = 0, n = live_save(out, FRS_LIVE_BYTES, scene, swell_held, alone_lv, 0, ld, sizeof ld, &m);
    if (n && m && n + m <= FRS_LIVE_BYTES) {
        memcpy(out + n, ld, m);
        n += m;
    }
    return n;
}
#define NW ((END - SNAP) / 100u)
typedef struct {
    uint8_t brain[NW][FRS_LIVE_BYTES];
    uint32_t bn[NW];
    lead_view_t v[NW];
} trace_t;
static trace_t ta, tb;
static fst_start snap_start;
static uint8_t snap_st[FRS_LIVE_BYTES], e_st[FRS_LIVE_BYTES];
static uint32_t snap_n, snap_first, e_n, e_first;
static void trace(trace_t *t)
{
    uint32_t w;
    for (w = 0; w < NW; w++) {
        run_to(SNAP + 100u * (w + 1u), 0);
        t->bn[w] = brain_of(t->brain[w], snap_start.scene);
        t->v[w] = view();
    }
}
static int same_voices(const lead_view_t *a, const lead_view_t *b)
{
    uint32_t j;
    if (a->note != b->note || a->gate != b->gate || a->stage != b->stage || a->held != b->held ||
        a->sounding != b->sounding || fabsf(a->level - b->level) > 0.002f || fabsf(a->fenv - b->fenv) > 0.002f)
        return 0;
    for (j = 0; j < LEAD_NV; j++)
        if (a->vnote[j] != b->vnote[j] || fabsf(a->venv[j] - b->venv[j]) > 0.002f)
            return 0;
    return 1;
}

static void streams(void)
{
    size_t n = 0, n2 = 0;
    uint8_t *s, *old;
    uint64_t h1, h2, hl;
    int at1 = 0, at2 = 0;
    start0();
    take_parts();
    s = journal(&n);
    check(s && !fst_read(s, n, &file) && !memcmp(file.layout, "FRS~", 4) && file.sbytes == FST_SCENE_BYTES &&
              file.nparam == NPARAM,
          "a take with the LEAD finished: \"FRS~\", its scenes shards");
    h1 = on_air(s, n, &at1);
    h2 = on_air(s, n, &at2);
    check(h1 && h1 == h2 && at1 && at2, "on air: its start's LEAD at the start; twice, every sample the same");
    check(view().ons > 0u, "on air: the LEAD sang");
    /* live, op by op: the same take, its scenes the FM-1's scene_t, their LEAD by FST_LEAD after them */
    fst_live_open(&lv, &st0, 44100.0f);
    ms0 = frs_ms();
    give(0);
    hash = 1469598103934665603u;
    run_to(END, 1);
    hl = hash;
    check(hl == h1 && lv.refused == 0u && lv.missed == 0u, "live, op by op: every sample the file's on air");
    check(lead_is(lead3 + 1), "live: the LEAD put whole at the end");
    free(s);
    {                                    /* the control: the scene's LEAD MOSS in the file, another sound */
        uint8_t keep[FRS_LEAD_BYTES];
        int at3 = 0;
        memcpy(keep, sc2 + FRS_SCENE_BYTES, sizeof keep);
        frs_lead_none(sc2 + FRS_SCENE_BYTES);
        s = journal(&n);
        memcpy(sc2 + FRS_SCENE_BYTES, keep, sizeof keep);
        check(s && on_air(s, n, &at3) != h1, "the control: the scene's LEAD MOSS in the file, another sound");
        free(s);
    }
    /* an "FRS}" stream (3.2's) on this engine: its LEAD MOSS */
    {
        FILE *f = fopen("engine/tests/old-streams-3.2/c-1-arp.frstream", "rb");
        uint8_t m[FRS_LEAD_BYTES];
        old = malloc(1 << 20);
        n2 = f ? fread(old, 1, 1 << 20, f) : 0u;
        if (f)
            fclose(f);
        frs_lead_none(m);
        check(n2 && !fst_read(old, n2, &file) && !memcmp(file.layout, "FRS}", 4) && file.sbytes == FRS_SCENE_BYTES &&
                  !fst_air_open(&air, &file, 44100.0f) && lead_is(m),
              "an \"FRS}\" stream: read, opened, its LEAD MOSS");
        free(old);
    }
}

/* ------------------------------------------------------------------ the running state's LEAD part */
static void snaps(void)
{
    uint32_t w, sameb = 0, samev = 0, m, k, stuck = 0;
    const uint8_t *ld;
    start0();
    take_parts();
    /* A: from the start, its snap at 1700 (four keys on the LEAD), on to the end */
    fst_live_open(&lv, &st0, 44100.0f);
    ms0 = frs_ms();
    give(0);
    run_to(SNAP, 0);
    e_n = frs_live_save(e_st, sizeof e_st, 0);   /* (D's: before fst_capture, a STORE that marks the sound saved) */
    e_first = 16u + (uint32_t)(e_st[14] | e_st[15] << 8);
    fst_capture(&snap_start, 0);
    snap_n = frs_live_save(snap_st, sizeof snap_st, snap_start.scene);
    snap_first = 16u + (uint32_t)(snap_st[14] | snap_st[15] << 8);
    ld = snap_st + snap_first;
    m = frs_live_ms(snap_st, snap_n);
    check(snap_n > snap_first + 16u && !memcmp(ld, "FRld", 4) && ld[4] == LIVE_LEAD_V && (ld[5] & LIVE_F_DSP) &&
              (ld[6] | ld[7] << 8) == FRS_LEAD_BYTES && (ld[8] | ld[9] << 8) == P_LD_WAVE &&
              !memcmp(ld + 10, snap_st + 10, 4) && snap_first + 16u + (uint32_t)(ld[14] | ld[15] << 8) == snap_n &&
              !memcmp(ld + 16, &st.v[P_LD_WAVE], FRS_LEAD_BYTES),
          "the snap: \"FRlv\", then \"FRld\" (its version, the sound's part, 25 settings from P_LD_WAVE, the same "
          "millisecond, its settings first)");
    check(view().held == 3u && view().sounding == 4u && ldk.n == 4u,
          "the snap: four keys on the LEAD, three voices held (POLY's; MONO's let go by MODE), one fading");
    printf("lead: the snap at %u ms, %u bytes: the first part %u, the LEAD's %u (four keys, four voices)\n", m, snap_n,
           snap_first, snap_n - snap_first);
    trace(&ta);
    /* B: on the snap's two parts */
    fst_live_open(&lv, &snap_start, 44100.0f);
    check(!fst_live_state(&lv, snap_st, snap_n, snap_start.scene) && frs_ms() == m, "B: the snap's two parts loaded");
    check(view().held == 3u && view().sounding == 4u && ldk.n == 4u, "B: its four keys and voices on the LEAD");
    give(SNAP);
    trace(&tb);
    for (w = 0; w < NW; w++) {
        sameb += ta.bn[w] == tb.bn[w] && !memcmp(ta.brain[w], tb.brain[w], ta.bn[w]);
        samev += same_voices(&ta.v[w], &tb.v[w]);
    }
    check(sameb == NW, "B: every 100 ms A's brain, the LEAD's with it, bit for bit");
    check(samev == NW, "B: every 100 ms A's LEAD voices (notes, gates, stages; envelopes within half precision)");
    if (sameb != NW || samev != NW)
        printf("lead: B the same brain %u, the same voices %u of %u\n", sameb, samev, (unsigned)NW);
    /* C: the first part alone (a page that keeps it alone): the keys on the LEAD let go, none stuck */
    fst_live_open(&lv, &snap_start, 44100.0f);
    check(!fst_live_state(&lv, snap_st, snap_first, snap_start.scene), "C: the first part alone loaded");
    for (k = 0; k < FRS_NKEYS; k++)
        stuck += key[k].kind == K_LEAD;
    blocks(8);
    check(!stuck && ldk.n == 0u && view().held == 0u, "C: no key on the LEAD, its voices let go");
    give(SNAP);
    run_to(3400u, 0);
    check(view().held == 1u, "C: a key on the LEAD after it sings (none stuck)");
    /* D: as A, its own state at its place: left, the very same (its two parts; its first alone) */
    for (k = 0; k < 2u; k++) {
        fst_live_open(&lv, &st0, 44100.0f);
        ms0 = frs_ms();
        give_to(0, SNAP);
        fst_live_at_state(&lv, m, e_st, k ? e_first : e_n, 0, 1);
        give(SNAP);
        run_to(SNAP + 100u, 0);
        check(lv.same == 1u && lv.loads == 0u, k ? "D: A's state's first part alone at its place: left, the very same"
                                                : "D: A's state at its place: left, the very same");
        if (lv.same != 1u || lv.loads)
            printf("lead: D same %u, loads %u, refused %u, full %u\n", lv.same, lv.loads, lv.refused, lv.full);
    }
}

int main(void)
{
    api();
    ops();
    streams();
    snaps();
    printf("lead: %d ok, %d FAIL\n", oks, fails);
    return fails ? 1 : 0;
}
