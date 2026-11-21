/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* Old streams on this engine: a stream recorded before ARP SNAP (P_ARP_SNAP, 3.0's, 1/4 at the start) plays as its engine
 * played it, ARP SNAP OFF, bit for bit; a stream whose state carries ARP SNAP plays with its own. engine/tests/old-streams/
 * holds takes made on the engine just before ARP SNAP (5b567f1, through this file's `make`, and the iPad's 39 of 985b2ff)
 * and their renders' hashes there (`hash`): from the top and joined half-way (fst_air_skip, as the radio joins). Controls
 * that must differ: ARP SNAP forced to 1/4 after the opening moves every arpeggio take's hash. A stream with no state (a
 * shard alone on air, shard_stream's) keeps the engine's own ARP SNAP, as before.
 *   engine/tests/old-streams-3.0.2/: takes of "FRS{" (3.0.2-b5's scenes, ae5ce21, this file's `make`: its swell takes
 * too, SWELL turned through scenes taken, gone to and an ANCHOR's way back), their scenes made over as they are played
 * ("FRS}": no name past sixteen, the LFOs' AMOUNTs x1.00, SWELL as it is), every hash their engine's.
 *   engine/tests/old-streams-3.2/: takes of "FRS}" (3.1 .. 3.2's scenes, 0e9a4b2: card #152's base, this file's `make`),
 * their LEAD MOSS now that a stream's scenes are shards ("FRS~"), every hash their engine's.
 *   But the takes that play BASS II's PULSE: since card #148 their strike comes in over 5.8 ms (dsp/bass.c PULSE_RISE),
 * their hashes that engine's (33 of the 81; PULSE_RISE 0, every one their own engine's again).
 *   engine/tests/old-streams-tape/: takes of "FRS{" with TAPE SPEED dragged down under the arpeggios and keys, then up
 * (card #160: JARDIN POURPRE's 64 -> 53), made and hashed on the iPad's engine of 2026-11-05 (68a4be7, 3.0.2-b5: its
 * byte log2's own, X0.79 at 53; this file's `make-tape`), every hash their engine's (frs_tape_302); read as 3.1 reads
 * the byte (X0.89 at 53, the grid on past the hands), every one moves.
 *   engine/tests/old-streams-3.3/: takes of timed MORPHs (card #202: NORTH RIVER to LONG NIGHT and back over 2:00;
 * MASTERING, CLOUD and MASTER EQ apart over 8 s and 15 s, LOAD SND ONLY, CUT), made and hashed on 3.3.1's engine
 * (ef2cbb5, card #202's base: this file's `make-morph`), every hash its engine's (their states without MORPH's mark:
 * frs_morph_331); played with this engine's MORPH, every one moves, a take of this engine's has its own.
 *   engine/tests/run.sh, or: old_streams engine/tests/old-streams engine/tests/old-streams-3.0.2 engine/tests/old-streams-3.2
 *   engine/tests/old-streams-tape engine/tests/old-streams-3.3
 *   old_streams make DIR             the takes, written into DIR (on the old engine: -DOLD_ENGINE; make-tape DIR: the
 *                                    tape's; make-morph DIR: the MORPHs')
 *   old_streams hash FILE...         each one's hashes: NAME TOP JOIN */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../frasil_engine.h"
#include "../frasil_params.h"
#include "../frasil_stream.h"

#ifdef FST_SCENE_BYTES                   /* (a journal's scene: a shard since card #152; an old engine's, its scene) */
#define JSCENE FST_SCENE_BYTES
#else
#define JSCENE FRS_SCENE_BYTES
#endif

static uint32_t rs;
static uint32_t rnd(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 17;
    rs ^= rs << 5;
    return rs;
}

/* ---------------------------------------------------------------- a take, through the real format */
typedef struct { uint32_t ms; uint8_t op, n; uint8_t a[6]; } ev_t;
static ev_t ev[20000];
static int nev;
static void put(uint32_t ms, uint8_t op, uint8_t n, int a0, int a1, int a2, int a3)
{
    ev_t *e = &ev[nev++];
    memset(e, 0, sizeof *e);
    e->ms = ms;
    e->op = op;
    e->n = n;
    e->a[0] = (uint8_t)a0;
    e->a[1] = (uint8_t)a1;
    e->a[2] = (uint8_t)a2;
    e->a[3] = (uint8_t)a3;
}
static int by_ms(const void *a, const void *b)
{
    const ev_t *x = a, *y = b;
    return x->ms < y->ms ? -1 : x->ms > y->ms;
}

/* SWELL turned through scenes: taken (FST_SCENE_NOW), gone to (FST_SCENE, under a MORPH or not), an ANCHOR set and its
 * way back (at once or gliding); scene: a factory scene's number + 1 in a[4] (an FSTJ_SCENE's bytes, take's) */
static void perform_swell(uint32_t len, uint32_t seed, uint32_t nsc)
{
    uint32_t t = 300;
    nev = 0;
    rs = seed;
    put(t + 2, FST_PLAY_ALL, 2, 1, 0, 0, 0);
    put(t + 10, FST_LATCH, 4, 0, 48, -1, 0);
    put(t + 20, FST_PARAM, 2, P_SWELL, 90, 0, 0);
    while (t < len - 3000u) {
        uint32_t k, ms;
        t += 400u + rnd() % 1200u;
        k = rnd() % 5u;
        if (k == 0u) {
            put(t, FST_PARAM, 2, P_SWELL, (int)(rnd() % 128u), 0, 0);
        } else if (k == 1u || k == 2u) {
            if (k == 2u)
                put(t, FST_PARAM, 2, P_MORPH, (int)(rnd() % 3u), 0, 0);
            put(t + 1u, FSTJ_SCENE, 3, k == 1u ? FST_SCENE_NOW : FST_SCENE, 0, 0, 0);
            ev[nev - 1].a[4] = (uint8_t)(1u + rnd() % nsc);
        } else if (k == 3u) {
            put(t, FSTJ_SCENE, 3, FST_ANCHOR, 0, 0, 0);
            ev[nev - 1].a[4] = (uint8_t)(1u + rnd() % nsc);
            put(t + 300u, FST_PARAM, 2, P_SWELL, (int)(rnd() % 128u), 0, 0);
            ms = rnd() % 2u ? 1500u : 0u;
            put(t + 700u, FST_ANCHOR_BACK, 2, (int)(ms & 255u), (int)(ms >> 8), 0, 0);
            t += 700u + ms;
        } else {
            put(t, FST_PARAM, 2, P_TIMBRE, (int)(rnd() % 128u), 0, 0);
        }
    }
    qsort(ev, (size_t)nev, sizeof ev[0], by_ms);
}

/* arp 1: GLINT and the bass on ARP, chords changing into them, keys and fingers (ARP SNAP's ground); 0: no arpeggio */
static void perform(uint32_t len, uint32_t seed, int arp)
{
    uint32_t t = 300;
    int key = 0;
    nev = 0;
    rs = seed;
    if (arp) {
        put(t, FST_PARAM, 2, P_SQ_TYPE, 1, 0, 0);
        put(t + 1, FST_PARAM, 2, P_BQ_ON, 1, 0, 0);
    }
    put(t + 2, FST_PLAY_ALL, 2, 1, 0, 0, 0);
    put(t + 10, FST_LATCH, 4, 0, 48, -1, 0);
    while (t < len - 2000u) {
        uint32_t k;
        t += 300u + rnd() % 1400u;
        k = rnd() % 6u;
        if (k == 0u) {
            int a = 60 + (int)(rnd() % 12u);
            put(t, FST_LATCH, 4, 1, a, -1, 0);
            put(t + 7u, FST_LATCH, 4, 1, a + 4, -1, 0);
            put(t + 14u, FST_LATCH, 4, 1, a + 7, -1, 0);
        } else if (k == 1u) {
            put(t, FST_LATCH, 4, 2, 36 + (int)(rnd() % 12u), -1, 0);
        } else if (k == 2u || k == 3u) {
            int n = (k == 2u ? 60 : 36) + (int)(rnd() % 24u);
            uint32_t d = 200u + rnd() % 600u;
            put(t, FST_KEY_DOWN, 4, key, n, k == 2u ? 1 : 2, 0);
            put(t + d, FST_KEY_UP, 2, key, 0, 0, 0);
            key = (key + 1) % 8;
        } else if (k == 4u) {
            int n = 60 + (int)(rnd() % 24u);
            put(t, FST_HOLD, 4, 1, n, 1, -1);
            put(t + 100u + rnd() % 400u, FST_HOLD, 4, 1, n, 0, -1);
        } else {
            put(t, FST_PARAM, 2, P_TIMBRE, (int)(rnd() % 128u), 0, 0);
        }
    }
    qsort(ev, (size_t)nev, sizeof ev[0], by_ms);
}

/* perform's arpeggios, keys and chords, TAPE SPEED moved under them as a hand drags it (card #160, JARDIN POURPRE's
 * 64 -> 53 a detent at a time), then lower faster, then up past X1.00 at once */
static void perform_tape(uint32_t len, uint32_t seed)
{
    int x;
    perform(len, seed, 1);
    for (x = 63; x >= 53; x--)
        put(8000u + 110u * (uint32_t)(63 - x), FST_PARAM, 2, P_TP_FLUTTER, x, 0, 0);
    for (x = 52; x >= 30; x--)
        put(16000u + 40u * (uint32_t)(52 - x), FST_PARAM, 2, P_TP_FLUTTER, x, 0, 0);
    put(26000u, FST_PARAM, 2, P_TP_FLUTTER, 100, 0, 0);
    qsort(ev, (size_t)nev, sizeof ev[0], by_ms);
}

/* scenes gone to under a MORPH (card #202), LOAD SND+NOTES (the state's, as the FM-1 starts) but where it says. North:
 * NORTH RIVER to LONG NIGHT over 2:00 and back (the user's report: "abrupt like a cut"), LONG NIGHT again over 30 s
 * before the way back ends. Mastering: RESILIENCE's chords through three of its own (msc: MASTERING's THRESHOLD, GAIN
 * and RATIO, CLOUD's level, MASTER EQ OFF, then CUSTOM's bands, then others, a short ATTACK and RELEASE) over 8 s, 8 s,
 * 15 s, LOAD SND ONLY over 4 s, then CUT */
static uint8_t msc[3][FRS_SCENE_BYTES];  /* (FSTJ_SCENE's a[4] 201 ..: take's) */
static void msc_put(uint32_t s, uint32_t p, uint8_t x) { msc[s][16u + p] = x; }   /* (scene_t's v, after its name) */
static void perform_morph(int mastering)
{
    nev = 0;
    if (!mastering) {
        put(2000u, FST_PARAM, 2, P_MORPH, 8, 0, 0);
        put(2001u, FSTJ_SCENE, 3, FST_SCENE, 0, 0, 0);
        ev[nev - 1].a[4] = 4;            /* (LONG NIGHT) */
        put(124000u, FSTJ_SCENE, 3, FST_SCENE, 0, 0, 0);
        ev[nev - 1].a[4] = 1;            /* (NORTH RIVER) */
        put(200000u, FST_PARAM, 2, P_MORPH, 6, 0, 0);
        put(200001u, FSTJ_SCENE, 3, FST_SCENE, 0, 0, 0);
        ev[nev - 1].a[4] = 4;
        return;
    }
    {
        static const uint8_t P[11] = {P_MS_RATIO, P_MS_THRESH, P_MS_GAIN, P_MX_CLOUD, P_EQ_PRESET, P_EQ_LOW, P_EQ_MID,
                                      P_EQ_HIGH, P_EQ_LOWF, P_EQ_MIDF, P_EQ_Q},
                             X[3][11] = {{3, 30, 2, 100, 0, 40, 10, 30, 40, 50, 30},
                                         {3, 10, 12, 40, 1, 34, 20, 30, 40, 50, 30},
                                         {5, 20, 6, 110, 1, 10, 40, 20, 90, 100, 90}};
        uint32_t s, k;
        for (s = 0; s < 3u; s++) {
            frs_scene_factory(2u, msc[s]);   /* (RESILIENCE's: its chords) */
            for (k = 0; k < 11u; k++)
                msc_put(s, P[k], X[s][k]);
        }
        msc_put(2u, P_EQ_HIGHF, 30);
        msc_put(2u, P_ATTAQUE, 20);
        msc_put(2u, P_RELACHE, 30);
    }
    put(1000u, FST_PARAM, 2, P_MORPH, 4, 0, 0);
    put(1001u, FSTJ_SCENE, 3, FST_SCENE, 0, 0, 0);
    ev[nev - 1].a[4] = 201;
    put(12000u, FSTJ_SCENE, 3, FST_SCENE, 0, 0, 0);
    ev[nev - 1].a[4] = 202;
    put(24000u, FST_PARAM, 2, P_MORPH, 5, 0, 0);
    put(24001u, FSTJ_SCENE, 3, FST_SCENE, 0, 0, 0);
    ev[nev - 1].a[4] = 203;
    put(42000u, FST_PARAM, 2, P_MORPH, 3, 0, 0);
    put(42001u, FST_PARAM, 2, P_LOAD, 1, 0, 0);
    put(42002u, FSTJ_SCENE, 3, FST_SCENE, 0, 0, 0);
    ev[nev - 1].a[4] = 202;
    put(49000u, FST_PARAM, 2, P_LOAD, 0, 0, 0);
    put(50000u, FST_PARAM, 2, P_MORPH, 0, 0, 0);
    put(50001u, FSTJ_SCENE, 3, FST_SCENE, 0, 0, 0);
    ev[nev - 1].a[4] = 203;
}

static int stateless;                    /* take(): its start without a state, as a shard alone on air has none */

/* the take on factory scene i; open: the engine opened again there, on the next scene (0: not); snap: ARP SNAP set before
 * the start is captured (-1: left as the engine has it) */
static uint8_t *take(uint32_t i, uint32_t nscenes, uint32_t len, uint32_t open, int snap, size_t *out)
{
    static uint8_t scene[FRS_SCENE_BYTES];
    static fst_start start, again;
    uint32_t last = 0, opened = 0;
    size_t j;
    int k;
    uint8_t *b = malloc(1024u * 1024u), *s;
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_step_stays(1);
#ifndef OLD_ENGINE
    if (snap >= 0)
        frs_param_set(P_ARP_SNAP, (uint32_t)snap);
#else
    (void)snap;
#endif
    if (open) {
        frs_scene_factory((i + 1u) % nscenes, scene);
        frs_scene_take(scene);
        fst_capture(&again, 0);
    }
    frs_scene_factory(i, scene);
    frs_scene_take(scene);
    fst_capture(&start, 0);
    if (stateless)
        start.state_n = 0;
    j = fst_jhead(&start, 44100, 1793000000, b);
    for (k = 0; k < nev; k++) {
        fst_ev e;
        memset(&e, 0, sizeof e);
        e.ms = ev[k].ms;
        if (open && !opened && e.ms >= open) {
            fst_ev o;
            memset(&o, 0, sizeof o);
            o.ms = e.ms;
            o.op = FSTJ_OPEN;
            o.a[0] = 0x44;
            o.a[1] = 0xAC;
            j += fst_jevent(&o, &last, &again, 0, b + j);
            opened = 1;
        }
        e.op = ev[k].op;
        e.n = ev[k].n;
        memcpy(e.a, ev[k].a, 6);
        if (e.op == FSTJ_SCENE) {        /* (a factory scene's bytes: perform_swell's, perform_morph's; from 201 one of
                                          * msc; a shard's room, its LEAD MOSS) */
            static uint8_t sb[JSCENE];
            if (ev[k].a[4] > 200u)
                memcpy(sb, msc[ev[k].a[4] - 201u], FRS_SCENE_BYTES);
            else
                frs_scene_factory(ev[k].a[4] - 1u, sb);
#ifdef FST_SCENE_BYTES
            frs_lead_none(sb + FRS_SCENE_BYTES);
#endif
            e.a[4] = 0;
            j += fst_jevent(&e, &last, sb, 0, b + j);
            continue;
        }
        j += fst_jevent(&e, &last, 0, 0, b + j);
    }
    {
        fst_ev e;
        memset(&e, 0, sizeof e);
        e.ms = len;
        e.op = FSTJ_END;
        j += fst_jevent(&e, &last, 0, 0, b + j);
    }
    s = fst_finish(b, j, out);
    free(b);
    return s;
}

static uint32_t factory_count(void)
{
    static uint8_t scene[FRS_SCENE_BYTES];
    uint32_t n = 0;
    while (frs_scene_factory(n, scene) == 0)
        n++;
    return n;
}

/* ---------------------------------------------------------------- a stream heard, as the radio plays it */
static fst_air air;
static fst_file file;
static float L[256], R[256];

static uint64_t fnv(uint64_t h, const float *x, uint32_t n)
{
    const uint8_t *p = (const uint8_t *)x;
    size_t i;
    for (i = 0; i < n * sizeof(float); i++)
        h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

/* blocks of 256 from where the stream stands until its millisecond `to` */
static uint64_t hear(uint64_t h, uint32_t to)
{
    while (fst_air_ms(&air) < to) {
        frs_render(L, R, 256u);
        fst_air_after(&air);
        h = fnv(fnv(h, L, 256u), R, 256u);
    }
    return h;
}

static int tape_31;                      /* open_it: TAPE SPEED's byte as 3.1 reads it after the opening (card #160's control) */
static int morph_as = -1;                /* and a scene's MORPH as 3.3.1 made it (1), as this engine makes it (0), or as
                                          * the stream says (-1; card #202's control) */

/* force >= 0: ARP SNAP set so right after the opening (the control) */
static int open_it(const uint8_t *b, size_t n, int force)
{
    if (fst_read(b, n, &file) || fst_air_open(&air, &file, 44100.0f))
        return -1;
#ifndef OLD_ENGINE
    if (force >= 0)
        frs_param_set(P_ARP_SNAP, (uint32_t)force);
    if (tape_31)
        frs_tape_302(0);
    if (morph_as >= 0)
        frs_morph_331(morph_as);
#else
    (void)force;
#endif
    return 0;
}

/* from the top to its end and 2 s more; joined half-way (5 s of warm-up skipped through), 8 s heard (join 0: not) */
static int hashes(const uint8_t *b, size_t n, int force, uint64_t *top, uint64_t *join)
{
    uint32_t half;
    if (open_it(b, n, force))
        return -1;
    *top = hear(0xcbf29ce484222325ull, file.ms + 2000u);
    if (!join)
        return 0;
    half = file.ms / 2u;
    if (open_it(b, n, force))
        return -1;
    fst_air_skip(&air, half > 5000u ? half - 5000u : 0u);
    hear(0, half);
    *join = hear(0xcbf29ce484222325ull, half + 8000u);
    return 0;
}

static uint8_t *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long sz;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(sz > 0 ? (size_t)sz : 1u);
    *n = fread(b, 1, (size_t)sz, f);
    fclose(f);
    return b;
}

static const char *base(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* ---------------------------------------------------------------- make, hash */
static int make(const char *dir)
{
    uint32_t i, nsc = factory_count();
    char path[1024];
    for (i = 0; i < nsc; i++) {
        static const struct { const char *kind; int arp; uint32_t len, open; } K[4] = {
            {"arp", 1, 40000u, 0u}, {"arp-open", 1, 40000u, 21000u}, {"plain", 0, 30000u, 0u}, {"swell", 0, 30000u, 0u}};
        int k;
        for (k = 0; k < 4; k++) {
            size_t n;
            uint8_t *s;
            FILE *f;
            if (k == 3)
                perform_swell(K[k].len, 2000u + i * 7u, nsc);
            else
                perform(K[k].len, 1000u + i * 7u + (uint32_t)k, K[k].arp);
            s = take(i, nsc, K[k].len, K[k].open, -1, &n);
            if (!s)
                return printf("make: scene %u's %s take made nothing\n", i, K[k].kind), 1;
            snprintf(path, sizeof path, "%s/c-%u-%s.frstream", dir, i, K[k].kind);
            f = fopen(path, "wb");
            if (!f || fwrite(s, 1, n, f) != n)
                return printf("make: %s not written\n", path), 1;
            fclose(f);
            free(s);
        }
    }
    printf("make: %u takes in %s\n", nsc * 4u, dir);
    return 0;
}

/* perform_tape's takes, one a factory scene, opened again at 21 s or not (on 3.0.2-b5's engine: old-streams-tape/) */
static int make_tape(const char *dir)
{
    uint32_t i, nsc = factory_count();
    char path[1024];
    int k;
    for (i = 0; i < nsc; i++)
        for (k = 0; k < 2; k++) {
            size_t n;
            uint8_t *s;
            FILE *f;
            perform_tape(34000u, 3000u + i * 7u + (uint32_t)k);
            s = take(i, nsc, 34000u, k ? 21000u : 0u, -1, &n);
            if (!s)
                return printf("make: scene %u's tape take made nothing\n", i), 1;
            snprintf(path, sizeof path, "%s/c-%u-tape%s.frstream", dir, i, k ? "-open" : "");
            f = fopen(path, "wb");
            if (!f || fwrite(s, 1, n, f) != n)
                return printf("make: %s not written\n", path), 1;
            fclose(f);
            free(s);
        }
    printf("make: %u takes in %s\n", nsc * 2u, dir);
    return 0;
}

/* perform_morph's two takes (on the engine before card #202's, 3.3.1's: old-streams-3.3/) */
static const uint32_t MORPH_LEN[2] = {250000u, 56000u};
static int make_morph(const char *dir)
{
    static const char *const NAME[2] = {"m-north-long", "m-mastering"};
    uint32_t nsc = factory_count();
    char path[1024];
    int k;
    for (k = 0; k < 2; k++) {
        size_t n;
        uint8_t *s;
        FILE *f;
        perform_morph(k);
        s = take(k ? 2u : 0u, nsc, MORPH_LEN[k], 0u, -1, &n);   /* (RESILIENCE, NORTH RIVER) */
        if (!s)
            return printf("make: the %s take made nothing\n", NAME[k]), 1;
        snprintf(path, sizeof path, "%s/%s.frstream", dir, NAME[k]);
        f = fopen(path, "wb");
        if (!f || fwrite(s, 1, n, f) != n)
            return printf("make: %s not written\n", path), 1;
        fclose(f);
        free(s);
    }
    printf("make: 2 takes in %s\n", dir);
    return 0;
}

static int hash_files(int argc, char **argv)
{
    int i;
    for (i = 0; i < argc; i++) {
        size_t n;
        uint64_t t, j;
        uint8_t *b = slurp(argv[i], &n);
        if (!b || hashes(b, n, -1, &t, &j))
            return printf("hash: %s does not play\n", argv[i]), 1;
        printf("%s %016llx %016llx\n", base(argv[i]), (unsigned long long)t, (unsigned long long)j);
        free(b);
    }
    return 0;
}

#ifndef OLD_ENGINE
/* ---------------------------------------------------------------- the test */
static int check_kept(const char *dir, int control, int *nfiles, int *moved, int *arps)
{
    char path[1024], name[256], line[512];
    unsigned long long ht, hj;
    FILE *f;
    int fails = 0;
    snprintf(path, sizeof path, "%s/hashes.txt", dir);
    f = fopen(path, "r");
    if (!f)
        return printf("FAIL %s: not there\n", path), 1;
    while (fgets(line, sizeof line, f)) {
        size_t n;
        uint64_t t, j, t4;
        uint8_t *b;
        int arp;
        if (line[0] == '#' || sscanf(line, "%255s %llx %llx", name, &ht, &hj) != 3)
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, name);
        b = slurp(path, &n);
        if (!b || hashes(b, n, -1, &t, &j)) {
            printf("FAIL %s: does not play\n", name);
            fails++;
            continue;
        }
        (*nfiles)++;
        if (t != ht || j != hj) {
            printf("FAIL %s: not as its engine played it (top %s, joined %s)\n", name, t == ht ? "same" : "DIFFERS",
                   j == hj ? "same" : "DIFFERS");
            fails++;
        }
        if (!control) {                  /* (takes made with ARP SNAP: theirs in their state) */
            free(b);
            continue;
        }
        /* the control: ARP SNAP 1/4 after the opening, as the engine had it without the fix */
        hashes(b, n, 3, &t4, 0);
        arp = strstr(name, "-arp") != 0 || strstr(name, "-3.") != 0;   /* (the C takes' arp ones; the iPad's ARP takes: N-3) */
        *arps += arp;
        if (t4 != t)
            (*moved)++;
        else if (arp) {
            printf("FAIL %s: an arpeggio take that ARP SNAP 1/4 leaves alone (the control proves nothing)\n", name);
            fails++;
        }
        free(b);
    }
    fclose(f);
    return fails;
}

/* card #160: old-streams-tape/'s takes (check_kept: as 3.0.2-b5 played them) read with TAPE SPEED as 3.1 reads it after
 * the opening, as 3.1 .. 3.3 played them: every one moves (else the check proves nothing); a take of this engine opens
 * with 3.1's (frs_tape_302 off) */
static int check_tape(const char *dir, uint32_t nsc)
{
    char path[1024], name[256], line[512];
    unsigned long long ht, hj;
    FILE *f;
    int fails = 0, n = 0;
    uint8_t *b_302 = 0;                  /* (c-0-tape's, kept: its TAPE SPEED read once it is at 53) */
    size_t n_302 = 0;
    snprintf(path, sizeof path, "%s/hashes.txt", dir);
    f = fopen(path, "r");
    if (!f)
        return printf("FAIL %s: not there\n", path), 1;
    while (fgets(line, sizeof line, f)) {
        size_t sz;
        uint64_t t;
        uint8_t *b;
        if (line[0] == '#' || sscanf(line, "%255s %llx %llx", name, &ht, &hj) != 3)
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, name);
        b = slurp(path, &sz);
        tape_31 = 1;
        if (!b || hashes(b, sz, -1, &t, 0)) {
            tape_31 = 0;
            printf("FAIL %s: does not play\n", name);
            fails++;
            continue;
        }
        tape_31 = 0;
        n++;
        if (t == ht) {
            printf("FAIL %s: TAPE SPEED read as 3.1's plays it the same (the check proves nothing)\n", name);
            fails++;
        }
        if (!strcmp(name, "c-0-tape.frstream"))
            b_302 = b, n_302 = sz;
        else
            free(b);
    }
    fclose(f);
    if (!n || !b_302)
        return printf("FAIL %s: no takes, or no c-0-tape.frstream\n", dir), fails + 1;
    {                                    /* (its words at 53 once it is there: X0.79; a take of this engine's X0.89) */
        size_t sz;
        uint8_t *s;
        char txt[FRS_TEXT];
        if (open_it(b_302, n_302, -1))
            return printf("FAIL %s: c-0-tape.frstream does not play\n", dir), fails + 1;
        hear(0, 9500u);
        frs_param_text(P_TP_FLUTTER, frs_param_get(P_TP_FLUTTER), txt);
        if (frs_param_get(P_TP_FLUTTER) != 53u || strcmp(txt, "\xC3\x97" "0.79")) {
            printf("FAIL c-0-tape.frstream: TAPE SPEED %u reads %s at 9.5 s, 3.0.2-b5's is 53 x0.79\n",
                   frs_param_get(P_TP_FLUTTER), txt);
            fails++;
        }
        free(b_302);
        perform_tape(34000u, 5u);
        s = take(0u, nsc, 34000u, 0u, -1, &sz);
        if (!s || open_it(s, sz, -1))
            return printf("FAIL a tape take of this engine's: does not play\n"), fails + 1;
        hear(0, 9500u);
        frs_param_text(P_TP_FLUTTER, frs_param_get(P_TP_FLUTTER), txt);
        if (frs_param_get(P_TP_FLUTTER) != 53u || strcmp(txt, "\xC3\x97" "0.89")) {
            printf("FAIL a tape take of this engine's: TAPE SPEED %u reads %s at 9.5 s, 3.1's is 53 x0.89\n",
                   frs_param_get(P_TP_FLUTTER), txt);
            fails++;
        }
        free(s);
    }
    return fails;
}

/* MORPH's mark in the state the engine saves now (card #202, 254 then 1: frs_state_save): its value, -1 none */
static int morph_mark(void)
{
    uint8_t s[FRS_STATE_BYTES];
    uint32_t n = frs_state_save(s), i;
    for (i = 5; i + 1u < n; i += 2u)
        if (s[i] == 254u)
            return s[i + 1u];
    return -1;
}

/* card #202: old-streams-3.3/'s takes (check_kept: as 3.3.1 played them, their MORPHs on the scene left's ATTACK and
 * RELEASE, MASTERING, CLOUD and MASTER EQ at the scene's at once) played with a scene's MORPH as this engine makes it
 * after the opening: every one moves (else the check proves nothing); a state saved while one plays has no mark (a snap
 * of it plays it the same). A take made here has the mark, and 3.3.1's MORPH forced plays it otherwise */
static int check_morph(const char *dir, uint32_t nsc)
{
    char path[1024], name[256], line[512];
    unsigned long long ht, hj;
    FILE *f;
    int fails = 0, n = 0;
    snprintf(path, sizeof path, "%s/hashes.txt", dir);
    f = fopen(path, "r");
    if (!f)
        return printf("FAIL %s: not there\n", path), 1;
    while (fgets(line, sizeof line, f)) {
        size_t sz;
        uint64_t t;
        uint8_t *b;
        if (line[0] == '#' || sscanf(line, "%255s %llx %llx", name, &ht, &hj) != 3)
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, name);
        b = slurp(path, &sz);
        if (!b || open_it(b, sz, -1)) {
            printf("FAIL %s: does not play\n", name);
            fails++;
            free(b);
            continue;
        }
        if (morph_mark() != -1) {
            printf("FAIL %s: a state saved as it plays has MORPH's mark (%d), its engine's had none\n", name,
                   morph_mark());
            fails++;
        }
        morph_as = 0;
        hashes(b, sz, -1, &t, 0);
        morph_as = -1;
        n++;
        if (t == ht) {
            printf("FAIL %s: this engine's MORPH plays it the same (the check proves nothing)\n", name);
            fails++;
        }
        free(b);
    }
    fclose(f);
    if (n != 2)
        return printf("FAIL %s: %d takes, 2 made\n", dir, n), fails + 1;
    {
        size_t sz;
        uint8_t *s;
        uint64_t t, t1;
        perform_morph(0);
        while (nev && ev[nev - 1].ms >= 30000u)   /* (its first 30 s: NORTH RIVER, its 2:00 MORPH begun) */
            nev--;
        s = take(0u, nsc, 30000u, 0u, -1, &sz);
        if (!s || open_it(s, sz, -1))
            return printf("FAIL a MORPH take of this engine's: does not play\n"), fails + 1;
        if (morph_mark() != 1) {
            printf("FAIL a MORPH take of this engine's: opens without MORPH's mark (%d)\n", morph_mark());
            fails++;
        }
        hashes(s, sz, -1, &t, 0);
        morph_as = 1;
        hashes(s, sz, -1, &t1, 0);
        morph_as = -1;
        if (t == t1) {
            printf("FAIL a MORPH take of this engine's: 3.3.1's MORPH plays it the same\n");
            fails++;
        }
        free(s);
    }
    return fails;
}

/* a take made here with ARP SNAP set: it opens with its own, at the start and after an opening, and plays it */
static int check_new(uint32_t nsc)
{
    static const int SNAP[3] = {3, 4, 0};
    int k, fails = 0;
    for (k = 0; k < 3; k++) {
        size_t n;
        uint8_t *s;
        uint64_t t, j, t0, j0;
        perform(30000u, 77u + (uint32_t)k, 1);
        s = take((uint32_t)k, nsc, 30000u, k == 1 ? 15000u : 0u, SNAP[k], &n);
        if (!s || open_it(s, n, -1))
            return printf("FAIL new take %d: does not play\n", k), 1;
        if (frs_param_get(P_ARP_SNAP) != (uint32_t)SNAP[k]) {
            printf("FAIL new take %d: opens with ARP SNAP %u, recorded %d\n", k, frs_param_get(P_ARP_SNAP), SNAP[k]);
            fails++;
        }
        if (k == 1) {                    /* (past its opening: the opening's state is the same engine's, its ARP SNAP 4) */
            hear(0, 16000u);
            if (frs_param_get(P_ARP_SNAP) != 4u) {
                printf("FAIL new take 1: ARP SNAP %u after its opening, recorded 4\n", frs_param_get(P_ARP_SNAP));
                fails++;
            }
        }
        hashes(s, n, -1, &t, &j);
        hashes(s, n, SNAP[k], &t0, &j0);  /* (its own ARP SNAP set again after the opening: nothing moves) */
        if (t != t0 || j != j0) {
            printf("FAIL new take %d: its replay is not its own ARP SNAP's\n", k);
            fails++;
        }
        hashes(s, n, SNAP[k] == 0 ? 3 : 0, &t0, 0);   /* (another one: it does move) */
        if (t == t0) {
            printf("FAIL new take %d: another ARP SNAP plays the same (the check proves nothing)\n", k);
            fails++;
        }
        free(s);
    }
    {                                    /* no state at all (a shard alone on air: shard_stream): the engine's own ARP SNAP */
        size_t n;
        uint8_t *s;
        uint32_t def = 0;
        frs_param_info(P_ARP_SNAP, 0, 0, &def);
        perform(10000u, 99u, 1);
        stateless = 1;
        s = take(0u, nsc, 10000u, 0u, 0, &n);
        stateless = 0;
        if (!s || open_it(s, n, -1))
            return printf("FAIL a take with no state: does not play\n"), fails + 1;
        if (frs_param_get(P_ARP_SNAP) != def) {
            printf("FAIL a take with no state: ARP SNAP %u, the engine's own is %u\n", frs_param_get(P_ARP_SNAP), def);
            fails++;
        }
        free(s);
    }
    return fails;
}
#endif

#ifndef OLD_ENGINE
/* FST_CUT_STOP (the iPad's CUT since 2026-11-05), written just before an FST_CUT: RESILIENCE, its tracks stopped, the
 * pair, PLAY: the drone's chords back (frs_cut_stop's), the FST_CUT after it skipped; an FST_CUT alone as it was (the
 * chords gone). 0, or the failures */
static float cl[4800], cr[4800];
static int cut_stop_chords(int pair)
{
    uint8_t sc[FRS_SCENE_BYTES];
    uint32_t i, n = 0;
    static fst_player p;
    fst_dec e;
    frs_view_t v;
    for (i = 0; !frs_scene_factory(i, sc) && strncmp((const char *)sc, "RESILIENCE", 10); i++)
        ;
    frs_close();
    frs_init(48000.0f);
    frs_scene_take(sc);
    for (i = 0; i < 10u; i++)
        frs_render(cl, cr, 4800);
    frs_play_all(0);
    memset(&p, 0, sizeof p);
    memset(&e, 0, sizeof e);
    e.op = FST_CUT_STOP;
    if (pair)
        fst_apply(&p, &e, 0);
    e.op = FST_CUT;
    fst_apply(&p, &e, 0);
    e.op = FST_PLAY;
    fst_apply(&p, &e, 0);
    for (i = 0; i < 10u; i++)
        frs_render(cl, cr, 4800);
    frs_view(&v);
    for (i = 0; i < 128u; i++)
        n += v.latched[i >> 5] >> (i & 31u) & 1u;
    return (int)n;
}
static int check_cut_stop(void)
{
    int f = 0;
    if (cut_stop_chords(1) != 2)
        f++, printf("FAIL FST_CUT_STOP, then its FST_CUT, PLAY: the drone's two chords not back\n");
    if (cut_stop_chords(0) != 0)
        f++, printf("FAIL an FST_CUT alone after the tracks stopped, PLAY: not as its engine played it (the chords gone)\n");
    return f;
}
#endif

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "make"))
        return make(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "make-tape"))
        return make_tape(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "make-morph"))
        return make_morph(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "hash"))
        return hash_files(argc - 2, argv + 2);
#ifndef OLD_ENGINE
    {
        int nfiles = 0, moved = 0, arps = 0, nfrs = 0, m2 = 0, a2 = 0, n32 = 0, m3 = 0, a3 = 0, ntp = 0, m4 = 0, a4 = 0;
        int n33 = 0, m5 = 0, a5 = 0, fails;
        const char *dir = argc >= 2 ? argv[1] : "engine/tests/old-streams";
        const char *dir2 = argc >= 3 ? argv[2] : "engine/tests/old-streams-3.0.2";
        const char *dir3 = argc >= 4 ? argv[3] : "engine/tests/old-streams-3.2";
        const char *dir4 = argc >= 5 ? argv[4] : "engine/tests/old-streams-tape";
        const char *dir5 = argc >= 6 ? argv[5] : "engine/tests/old-streams-3.3";
        fails = check_kept(dir, 1, &nfiles, &moved, &arps);
        fails += check_kept(dir2, 0, &nfrs, &m2, &a2);
        fails += check_kept(dir3, 0, &n32, &m3, &a3);
        fails += check_kept(dir4, 0, &ntp, &m4, &a4);
        fails += check_kept(dir5, 0, &n33, &m5, &a5);
        fails += check_tape(dir4, factory_count());
        fails += check_morph(dir5, factory_count());
        fails += check_new(factory_count());
        fails += check_cut_stop();
        if (!nfiles || !nfrs || !n32 || !ntp || !n33)
            return printf("FAIL old_streams: no takes in %s\n",
                          !nfiles ? dir : !nfrs ? dir2 : !n32 ? dir3 : !ntp ? dir4 : dir5), 1;
        if (fails)
            return printf("old_streams: %d failed\n", fails), 1;
        printf("old_streams: ok (%d takes from before ARP SNAP as their engine played them, from the top and joined; "
               "ARP SNAP 1/4 moves %d of them, all %d arpeggio takes among them; %d takes of FRS{ as 3.0.2-b5 played them; "
               "%d of FRS} as 3.2 played them; %d of FRS{ with TAPE SPEED moved as 3.0.2-b5 played them, every one "
               "moved read as 3.1's, X0.79 at 53 and a new take's X0.89; %d of timed MORPHs as 3.3.1 played them, "
               "every one moved by this engine's MORPH, a new one with its own; 3 new takes with their own, one with no "
               "state with the engine's; FST_CUT_STOP's pair)\n",
               nfiles, moved, arps, nfrs, n32, ntp, n33);
        return 0;
    }
#else
    printf("usage: old_streams make DIR | make-tape DIR | make-morph DIR | hash FILE...\n");
    return 2;
#endif
}
