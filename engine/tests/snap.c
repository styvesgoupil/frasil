/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* A listener's engine opened on a snap goes on as the engine it was taken from (Perform LIVE, card #131: the running
 * state, frs_live_save and frs_live_load, and fst_live's queue: each op in its millisecond, the snap at its place).
 *   A: WHITEOUT (GLINT's arpeggio at random, the bass's at random, the LFOs at random and drifting), drones and notes
 *   latched, SPARKLE, MOTION, EVOLVE, BEAT, a key on GLINT now and then, settings turned, from the start to 60 s.
 *   B: opened at 30 s on A's snap (the start as fst_capture takes it, the running state with it), the same ops after:
 *   the brain bit for bit A's at every 100 ms to the end (the notes, the arpeggio's, the steps, the settings: frs_view;
 *   the running state's brain at the end), the sound within what the buffers held (the cloud's room, the echoes, a
 *   filter's state: the level each 100 ms, said).
 *   C: #114's listener, opened on the same start without the running state: its notes and LFOs elsewhere (the control).
 *   D: opened at the start as A, its ops late by a few milliseconds (a listener's jitter) up to the snap, then the snap
 *   at its place in its queue (fst_live_at_state, inside a millisecond): from there A's brain again.
 *   E: as A exactly, the snap at its place with cmp: left (the very same), none loaded.
 * Then: a save, a load, a save: the same bytes; half precision's round trip; another engine's state refused; the sizes
 * (said). G: PULSE's strike coming in (card #148), loaded, the bass's level as it was. engine/tests/run.sh, or: snap */
#include "../frasil_engine.c"
#include "../frasil_stream.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

#define SNAP_MS 30000u
#define END_MS 60000u
#define WIN 4410u                        /* 100 ms */
#define NW ((END_MS - SNAP_MS) / 100u + 8u)

typedef struct {
    uint32_t ms;
    uint8_t op, n, a[4];
} op_t;
#define OP(ms, op, n, a0, a1, a2, a3) {ms, op, n, {a0, a1, a2, a3}}
static const op_t OPS[] = {
    OP(200, FST_LATCH, 4, 0, 49, 0xff, 0), OP(210, FST_LATCH, 4, 0, 56, 0xff, 0),
    OP(300, FST_LATCH, 4, 1, 73, 0xff, 0), OP(301, FST_LATCH, 4, 1, 76, 0xff, 0), OP(302, FST_LATCH, 4, 1, 80, 0xff, 0),
    OP(303, FST_LATCH, 4, 1, 85, 0xff, 0),
    OP(400, FST_PARAM, 2, P_BQ_ON, 1, 0, 0), OP(401, FST_PARAM, 2, P_BA_MODE, 3, 0, 0),
    OP(402, FST_LATCH, 4, 2, 37, 0xff, 0), OP(403, FST_LATCH, 4, 2, 44, 0xff, 0),
    OP(500, FST_PARAM, 2, P_GX_SPARK, 90, 0, 0), OP(501, FST_PARAM, 2, P_MOUVEMENT, 110, 0, 0),
    OP(502, FST_PARAM, 2, P_PA_EVOLVE, 80, 0, 0), OP(503, FST_PARAM, 2, P_PA_BEAT, 60, 0, 0),
    OP(504, FST_PARAM, 2, P_MAREE, 100, 0, 0), OP(505, FST_PARAM, 2, P_SWELL, 40, 0, 0),
    OP(4000, FST_KEY_DOWN, 4, 3, 78, 1, 90), OP(4600, FST_KEY_UP, 2, 3, 0, 0, 0),
    OP(9000, FST_PARAM, 2, P_ARP_CHANCE, 60, 0, 0), OP(12000, FST_PARAM, 2, P_TIMBRE, 90, 0, 0),
    OP(17000, FST_KEY_DOWN, 4, 5, 68, 0, 70), OP(17300, FST_KEY_PRESSURE, 2, 5, 90, 0, 0),
    OP(17900, FST_KEY_UP, 2, 5, 0, 0, 0), OP(22000, FST_LATCH, 4, 0, 61, 0xff, 0),
    OP(26000, FST_PARAM, 2, P_ARP_RATE, 7, 0, 0), OP(29990, FST_PARAM, 2, P_FILTRE, 70, 0, 0),
    /* after the snap */
    OP(30500, FST_KEY_DOWN, 4, 4, 75, 1, 100), OP(31200, FST_KEY_UP, 2, 4, 0, 0, 0),
    OP(34000, FST_PARAM, 2, P_ARP_MODE, 4, 0, 0), OP(38000, FST_LATCH, 4, 1, 87, 0xff, 0),
    OP(41000, FST_KEY_DOWN, 4, 6, 70, 0, 80), OP(41100, FST_KEY_PRESSURE, 2, 6, 100, 0, 0),
    OP(42500, FST_KEY_UP, 2, 6, 0, 0, 0), OP(45000, FST_PARAM, 2, P_TEMPO, 50, 0, 0),
    OP(50000, FST_LATCH, 4, 0, 49, 0xff, 0), OP(53000, FST_PARAM, 2, P_GX_SPARK, 127, 0, 0),
    OP(57000, FST_PARAM, 2, P_TIMBRE, 40, 0, 0),
};
#define NOPS (sizeof OPS / sizeof OPS[0])

static fst_live lv;
static float L[256], R[256];
static fst_start start0, snap_start;
static uint8_t snap_live[FRS_LIVE_BYTES], brain_a[FRS_LIVE_BYTES], brain_x[FRS_LIVE_BYTES], tmp[FRS_LIVE_BYTES],
    tmp2[FRS_LIVE_BYTES];
static uint8_t e_live[FRS_LIVE_BYTES];
static uint32_t snap_n, brain_an, snap_ms, e_n;

typedef struct {
    float rms[NW];
    uint64_t view[NW];                   /* frs_view and every setting, hashed */
    uint32_t n;
} trace_t;
static trace_t ta, tb, tc, td, tf;

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = p;
    size_t i;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 1099511628211u;
    return h;
}
static uint64_t view_hash(void)
{
    frs_view_t v;
    uint32_t p;
    uint8_t x;
    uint64_t h = 1469598103934665603u;
    memset(&v, 0, sizeof v);
    frs_view(&v);
    h = fnv(h, &v, sizeof v);
    for (p = 0; p < NPARAM; p++) {
        x = (uint8_t)frs_param_get(p);
        h = fnv(h, &x, 1);
    }
    for (p = 0; p < NLFO; p++)             /* (the LFOs where in their turns, their draws) */
        h = fnv(h, &lfo[p], sizeof lfo[p]);
    h = fnv(h, &arp.rng, 4);
    h = fnv(h, &barp.rng, 4);
    h = fnv(h, &spk, sizeof spk);
    return h;
}

/* a MIDI clock (F: card #123's, followed): one every 20 ms give or take one, as a DAW's comes; the clock and the ops
 * (from OPS[op_i] on) given to the queue together as the blocks reach them, in their milliseconds' order (a listener's
 * worklet does the same: clk_at the next clock's, 0 none; clk_k its count, the jitter's) */
static uint32_t clk_at, clk_k, op_i;
static void clocks(uint32_t lim)
{
    static const uint8_t F8[4] = {0xF8, 0, 0, 0};
    while (clk_at && (clk_at < lim || (op_i < sizeof OPS / sizeof OPS[0] && OPS[op_i].ms < lim))) {
        if (op_i < sizeof OPS / sizeof OPS[0] && OPS[op_i].ms < lim && OPS[op_i].ms <= clk_at) {
            fst_live_at(&lv, OPS[op_i].ms, OPS[op_i].op, OPS[op_i].a, OPS[op_i].n, 0);
            op_i++;
            continue;
        }
        if (clk_at >= lim)
            break;
        fst_live_at(&lv, clk_at, FST_MIDI, F8, 4, 0);
        clk_at += 20u + (clk_k * 7u % 3u) - 1u;
        clk_k++;
    }
}

/* render in blocks to the millisecond `until`, a window's level and view each 100 ms from `from` */
static void run(uint32_t until, trace_t *t, uint32_t from)
{
    static double acc;
    static uint32_t in;
    if (t && frs_ms() <= from + 1u) {
        acc = 0.0;
        in = 0;
    }
    while (frs_ms() < until) {
        uint32_t i;
        clocks(frs_ms_after(256));
        frs_render(L, R, 256);
        if (!t || frs_ms() <= from)
            continue;
        for (i = 0; i < 256u; i++) {
            acc += (double)L[i] * L[i] + (double)R[i] * R[i];
            if (++in == WIN) {
                if (t->n < NW) {
                    t->rms[t->n] = (float)sqrt(acc / (2.0 * WIN));
                    t->view[t->n] = view_hash();
                    t->n++;
                }
                acc = 0.0;
                in = 0;
            }
        }
    }
}

/* the ops from `from` to before `to`, in their order (a stream's: a snap among them goes between), shifted, late_by
 * a few milliseconds late (0: on time) */
static void queue(uint32_t from, uint32_t to, int32_t shift, uint32_t late_by)
{
    uint32_t i;
    for (i = 0; i < NOPS; i++)
        if (OPS[i].ms >= from && OPS[i].ms < to) {
            uint32_t ms = (uint32_t)((int32_t)OPS[i].ms + shift);
            if (late_by)
                ms += late_by + i % 3u;
            fst_live_at(&lv, ms, OPS[i].op, OPS[i].a, OPS[i].n, 0);
        }
}
#define ALL 0xFFFFFFFFu

static void open_at(const fst_start *s)
{
    frs_close();
    if (fst_live_open(&lv, s, 44100.0f)) {
        printf("snap: fst_live_open refused it\n");
        exit(1);
    }
}

static uint32_t brain_of(uint8_t *out, const uint8_t *scene)   /* the brain alone, ENV held, the engine alone's (the
                                                                 * LEAD's after it, its brain too: card #152) */
{
    static uint8_t ld[LIVE_LEAD_BYTES];
    uint32_t m = 0, n = live_save(out, FRS_LIVE_BYTES, scene, swell_held, alone_lv, 0, ld, sizeof ld, &m);
    if (n && m && n + m <= FRS_LIVE_BYTES) {
        memcpy(out + n, ld, m);
        n += m;
    }
    return n;
}

static void compare(const trace_t *x, const char *who, int same_brain, double *mean_db, double *max_db,
                    uint32_t *views)
{
    uint32_t i, n = x->n < ta.n ? x->n : ta.n, k = 0;
    double sum = 0.0, mx = 0.0;
    *views = 0;
    for (i = 0; i < n; i++) {
        if (x->view[i] == ta.view[i])
            (*views)++;
        if (i >= 30u) {                  /* (after 3 s: the cloud's room filled again) */
            double d = fabs(20.0 * log10((x->rms[i] + 1e-6) / (ta.rms[i] + 1e-6)));
            sum += d;
            mx = d > mx ? d : mx;
            k++;
        }
    }
    *mean_db = k ? sum / k : 0.0;
    *max_db = mx;
    printf("%s: %u of %u windows the brain A's, the level %.2f dB from A's on average (%.2f at most) after 3 s\n", who,
           *views, n, *mean_db, *max_db);
    (void)same_brain;
}

/* an FM-1's STATE (the simulator's pfsave: joined, unpacked) into the engine alone: its start, then its running state,
 * read to its last byte (its 27 keys, its parts, no engine alone's), the clock the FM-1's next millisecond */
/* G (card #148): PULSE's strike coming in (dsp/bass.c PULSE_RISE: PUNCH and the attack over 5.8 ms) goes with a state.
 * NEW SHARD's bass held, PULSE 1/2 BEAT, RELEASE 88, PUNCH 100; at each of eight strikes the state at its block's end,
 * the engine on a block (its samples), on two more, the state loaded, a block: the samples the ones it had, within
 * half precision's (0.05 %; the strike's ramp lost on loading, its last chunk's: 7.6 % off, PUNCH's level) */
static void pulse_state(void)
{
    static float ref[256];
    uint8_t sc[FRS_SCENE_BYTES];
    uint32_t seen = 0, k = 0, i, j, n;
    float worst = 0.0f, top;
    bass_view_t v;
    frs_close();
    frs_init(44100.0f);
    frs_scene_new(sc);
    frs_scene_take(sc);
    frs_param_set(P_BS_PULSE, 3);
    frs_param_set(P_BS_RELEASE, 88);
    frs_param_set(P_BS_PUNCH, 100);
    frs_latch(2, 45, 0);
    for (i = 0; i < 2000u && k < 8u; i++) {
        frs_render(L, R, 256);
        frasil_bass_view(&v);
        if (v.pulses == seen)
            continue;
        n = frs_live_save(tmp, sizeof tmp, sc);
        frs_render(ref, R, 256);
        frs_render(L, R, 256);
        frs_render(L, R, 256);
        if (!n || frs_live_load(tmp, n, sc)) {
            check(0, "G: a state taken on PULSE's strike, loaded");
            return;
        }
        frs_render(L, R, 256);
        frasil_bass_view(&v);
        seen = v.pulses;                 /* (not in a state: the engine's own count) */
        for (j = 0, top = 0.0f; j < 256u; j++)
            top = fabsf(ref[j]) > top ? fabsf(ref[j]) : top;
        for (j = 0; j < 256u; j++)
            if (fabsf(L[j] - ref[j]) > worst * top)
                worst = fabsf(L[j] - ref[j]) / top;
        k++;
    }
    check(k == 8u && worst < 0.01f, "G: PULSE's strike coming in, in the state: the bass after it as it was");
    printf("snap: PULSE's strike in a state, 8 strikes: the bass a block on within %.2f %% of its own\n", 100.0f * worst);
}

static uint32_t lead_keys(void)          /* (the keys on the LEAD: live.c's LIVE_K_LEAD, core.c's K_LEAD) */
{
    uint32_t k, n = 0;
    for (k = 0; k < FRS_NKEYS; k++)
        n += key[k].kind == LIVE_K_LEAD;
    return n;
}

static void fm1_state(const char *file)
{
    static uint8_t b[8192], both[FRS_LIVE_BYTES];
    fst_start s;
    FILE *f = fopen(file, "rb");
    uint32_t n = f ? (uint32_t)fread(b, 1, sizeof b, f) : 0u, sn, i, ln, ms, lp, lm;
    if (f)
        fclose(f);
    check(n > 2u + FRS_SCENE_BYTES, "the FM-1's STATE read");
    if (n <= 2u + FRS_SCENE_BYTES)
        return;
    memset(&s, 0, sizeof s);
    sn = b[0] | b[1] << 8;
    check(sn == FRS_SCENE_BYTES, "the FM-1's STATE: its scene this layout's");
    memcpy(s.scene, b + 2, FRS_SCENE_BYTES);
    frs_lead_none(s.scene + FRS_SCENE_BYTES);   /* (its scene_t: the LEAD in its own part) */
    i = 2u + sn;
    s.state_n = b[i];
    memcpy(s.state, b + i + 1u, s.state_n);
    i += 1u + s.state_n;
    s.moves_n = b[i];
    memcpy(s.moves, b + i + 1u, 2u * s.moves_n);
    i += 1u + 2u * s.moves_n;
    s.tracks = b[i++];
    ln = i + 2u <= n ? (uint32_t)(b[i] | b[i + 1u] << 8) : 0u;
    lp = i + 2u + ln;                    /* (card #152: the LEAD's part after it, u16 its length, to the end) */
    lm = lp + 2u <= n ? (uint32_t)(b[lp] | b[lp + 1u] << 8) : 0u;
    check(ln > LIVE_HEAD && lp + 2u + lm == n, "the FM-1's STATE: its running state after the tracks, the LEAD's, to the end");
    check(lm > LIVE_HEAD && !memcmp(b + lp + 2u, "FRld", 4) && !memcmp(b + lp + 2u + 10u, b + i + 2u + 10u, 4),
          "the FM-1's STATE: the LEAD's part, the same instant's");
    ms = frs_live_ms(b + i + 2u, ln);
    check(ms > 0u && (b[i + 2u + 5u] & LIVE_F_DSP) && !(b[i + 2u + 5u] & LIVE_F_ALONE),
          "the FM-1's running state: its sound's part, no engine alone's");
    open_at(&s);
    check(!fst_live_state(&lv, b + i + 2u, ln, s.scene), "the FM-1's running state loaded in the engine alone, its first part");
    check(frs_ms() == ms, "the engine alone runs on from the FM-1's next millisecond");
    check(!lead_keys(), "its first part alone: the FM-1's key on the LEAD let go");
    memcpy(both, b + i + 2u, ln);        /* (both, as a listener joins them) */
    memcpy(both + ln, b + lp + 2u, lm);
    open_at(&s);
    check(!fst_live_state(&lv, both, ln + lm, s.scene), "the FM-1's running state loaded, the LEAD's part after it");
    check(frs_ms() == ms, "the engine alone runs on from the FM-1's next millisecond, the LEAD's with it");
#if FRS_LEAD
    check(lead_keys() == 1u && ldk.n == 1u && key[ldk.k[0]].kind == K_LEAD, "both: the FM-1's key on the LEAD, held");
#else
    check(!lead_keys(), "both, an engine without the LEAD: the FM-1's key on it let go, the LEAD's part left");
#endif
    run(ms + 2000u, 0, 0);
    check(lv.refused == 0u, "two seconds on from it, nothing refused");
    printf("snap: an FM-1's STATE, %u bytes, its running state %u, the LEAD's %u\n", n, ln, lm);
}

int main(int argc, char **argv)
{
    uint8_t sc[FRS_SCENE_BYTES];
    uint32_t i, w = 0, views, n2, n3;
    double mean, mx, mean_c, mx_c;
    char name[17];
    /* WHITEOUT */
    for (i = 0; !frs_scene_factory(i, sc); i++) {
        memcpy(name, sc, 16);
        name[16] = 0;
        if (!strcmp(name, "WHITEOUT")) {
            w = i;
            break;
        }
    }
    check(!strcmp(name, "WHITEOUT"), "WHITEOUT among the factory scenes");
    frs_scene_factory(w, sc);
    memset(&start0, 0, sizeof start0);
    memcpy(start0.scene, sc, FRS_SCENE_BYTES);
    start0.tracks = 7;

    /* A, and its snap at 30 s (a block's end: as the FM-1's main loop takes one between two blocks) */
    open_at(&start0);
    queue(0, ALL, 0, 0);
    frs_render(L, R, 255);
    run(SNAP_MS, 0, 0);
    snap_ms = frs_ms();
    e_n = frs_live_save(e_live, sizeof e_live, 0);   /* (E's: before fst_capture, a STORE that marks the sound saved) */
    fst_capture(&snap_start, 0);
    snap_n = frs_live_save(snap_live, sizeof snap_live, snap_start.scene);
    brain_an = brain_of(brain_x, snap_start.scene);
    check(snap_n > LIVE_HEAD && frs_live_ms(snap_live, snap_n) == snap_ms, "A's state at 30 s, its millisecond");
    {
        uint32_t b0 = live_save(tmp, sizeof tmp, snap_start.scene, 0, 0, 0, 0, 0, 0);
        uint32_t s0 = live_save(tmp, sizeof tmp, snap_start.scene, 0, 0, 1, 0, 0, 0);
        printf("snap: at %u ms, %u bytes: the brain %u, the sound %u, the engine alone's %u (an FM-1's: %u; the start's "
               "scene %u, state %u, moves %u)\n", snap_ms, snap_n, b0, s0 - b0, brain_an - b0, s0, FRS_SCENE_BYTES,
               snap_start.state_n, snap_start.moves_n);
    }
    run(END_MS, &ta, snap_ms);
    brain_an = brain_of(brain_a, snap_start.scene);

    /* B: opened on the snap */
    open_at(&snap_start);
    check(!fst_live_state(&lv, snap_live, snap_n, snap_start.scene), "B: the snap's state loaded");
    check(frs_ms() == snap_ms, "B: the snap's millisecond");
    queue(snap_ms, ALL, 0, 0);
    run(END_MS, &tb, snap_ms);
    n2 = brain_of(tmp, snap_start.scene);
    check(n2 == brain_an && !memcmp(tmp, brain_a, n2), "B: the brain A's at 60 s, bit for bit");
    compare(&tb, "B", 1, &mean, &mx, &views);
    check(views == ta.n && tb.n == ta.n, "B: every 100 ms the brain A's (notes, steps, settings, LFOs, draws)");
    check(mean < 1.0 && mx < 6.0, "B: the sound A's within what the buffers held");

    /* C: #114's, the start alone (its clock its own: the ops shifted to it) */
    open_at(&snap_start);
    {
        uint32_t t0 = frs_ms();
        queue(snap_ms, ALL, (int32_t)t0 - (int32_t)snap_ms, 0);
        run(t0 + (END_MS - snap_ms), &tc, t0);
    }
    compare(&tc, "C (no state, #114's)", 0, &mean_c, &mx_c, &views);
    check(views < ta.n / 2u, "C: without the state the brain is elsewhere (the control)");

    /* D: late ops (a listener's jitter) up to the snap, then the snap at its place */
    open_at(&start0);
    queue(0, snap_ms, 0, 3u);
    check(!fst_live_at_state(&lv, snap_ms, snap_live, snap_n, snap_start.scene, 1), "D: the snap queued at its place");
    queue(snap_ms, ALL, 0, 0);
    check(fst_live_state_ms(&lv) == snap_ms, "D: the snap's millisecond waiting");
    frs_render(L, R, 255);
    run(END_MS, &td, snap_ms);
    check(lv.loads == 1u && lv.same == 0u, "D: the snap loaded (its brain had drifted)");
    n2 = brain_of(tmp, snap_start.scene);
    check(n2 == brain_an && !memcmp(tmp, brain_a, n2), "D: from the snap on, the brain A's at 60 s");
    compare(&td, "D (late, then the snap)", 1, &mean, &mx, &views);
    check(views == td.n, "D: after the snap, every 100 ms the brain A's");

    /* E: as A, A's state at the snap (before its STORE) at its place with cmp: the very same, left */
    open_at(&start0);
    queue(0, snap_ms, 0, 0);
    fst_live_at_state(&lv, snap_ms, e_live, e_n, 0, 1);
    queue(snap_ms, ALL, 0, 0);
    frs_render(L, R, 255);
    run(SNAP_MS + 100u, 0, 0);
    check(lv.same == 1u && lv.loads == 0u, "E: A's own state found the very same, left");

    /* F: as A, on a MIDI clock (card #123's: its tempo as followed, when the next is due, the beat measured), then
     * opened on its snap at 30 s: every 100 ms the brain A's, the clock's own fields with it */
    open_at(&start0);
    clk_at = 100u, clk_k = 0, op_i = 0;
    frs_render(L, R, 255);
    run(SNAP_MS, 0, 0);
    {
        uint32_t at = clk_at, k = clk_k, oi = op_i, fn;
        static uint8_t f_live[FRS_LIVE_BYTES], f_brain[FRS_LIVE_BYTES];
        fst_start f_start;
        uint32_t f_ms = frs_ms();
        float clk_ms = grid.clk_ms;
        trace_t fa;
        fst_capture(&f_start, 0);
        fn = frs_live_save(f_live, sizeof f_live, f_start.scene);
        memset(&fa, 0, sizeof fa);
        memcpy(&ta, &fa, sizeof fa);     /* (A's trace the clock's now) */
        run(END_MS, &ta, f_ms);
        n2 = brain_of(f_brain, f_start.scene);
        check(clk_ms > 19.0f && clk_ms < 21.5f, "F: the grid follows the clock (some 20 ms a clock)");
        open_at(&f_start);
        check(!fst_live_state(&lv, f_live, fn, f_start.scene), "F: its snap loaded");
        clk_at = at, clk_k = k, op_i = oi;   /* (the clock and the ops go on as they came to A) */
        run(END_MS, &tf, f_ms);
        n3 = brain_of(tmp, f_start.scene);
        check(n3 == n2 && !memcmp(tmp, f_brain, n2), "F: on a MIDI clock, the brain A's at 60 s, bit for bit");
        compare(&tf, "F (a MIDI clock)", 1, &mean, &mx, &views);
        check(views == ta.n && tf.n == ta.n, "F: on a MIDI clock, every 100 ms the brain A's");
        clk_at = 0;
    }

    /* a save, a load, a save; another's refused; half precision */
    n2 = frs_live_save(tmp, sizeof tmp, snap_start.scene);
    check(!frs_live_load(tmp, n2, snap_start.scene), "load what was saved");
    n3 = frs_live_save(tmp2, sizeof tmp2, snap_start.scene);
    check(n3 == n2 && !memcmp(tmp, tmp2, n2), "a save, a load, a save: the same bytes");
    tmp[4] = LIVE_V + 1u;
    check(frs_live_load(tmp, n2, snap_start.scene) == -1, "another version's refused");
    tmp[4] = LIVE_V;
    check(frs_live_load(tmp, n2 - 1u, snap_start.scene) == -1, "cut short: refused");
    check(frs_live_save(tmp, 40, snap_start.scene) == 0u, "no room: nothing saved");
    {
        static const float F[] = {0.0f, 1.0f, -1.5f, 0.333f, 0.999f, 1e-5f, -0.7071f, 6.1e-5f, 1e-9f};
        int good = 1;
        for (i = 0; i < sizeof F / sizeof F[0]; i++) {
            float b = rs_float(rs_half(F[i]));
            if (fabsf(b - F[i]) > fabsf(F[i]) * 0.0006f + 6e-8f)
                good = 0;
        }
        check(good && rs_half(rs_float(0x3C00u)) == 0x3C00u, "half precision: there and back");
    }
    pulse_state();
    if (argc > 1)
        fm1_state(argv[1]);
    printf("snap: %d ok, %d failed\n", oks, fails);
    return fails ? 1 : 0;
}
