/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* ATTACK SHAPE (card #122), on the engine as the app builds it (FRS_LEAD, dsp/lead.c):
 *   CURVE, the default, bit for bit as before it: a note on each layer (the drones, GLINT, BASS, the LEAD and its
 *   filter's ATTACK with it) at three ATTACKs, the whole mix's hash the one the engine of before card #122 renders
 *   (SHAPE_BEFORE, made with -DSHAPE_BASELINE on history/dev's engine); LINE turned and back before the note, the same.
 *   LINE a straight line: the gain (the drones', the bass's, the LEAD's: their envelope's square), GLINT's onset and
 *   the LEAD's filter envelope up at one pace from the strike to the top, at ATTACK's time (as many blocks, as much
 *   each), then DECAY; CURVE's not (its pace at the start some three times its end's).
 *   No setting (NPARAM as before: a recording's HEAD the same): of the moment (no scene takes them, none dirty), the
 *   state's (STATE_ASHAPE's pair: saved, back, a state without it leaving them), Perform LIVE's start carrying them.
 *   engine/tests/line-streams/: takes made on this engine with LINE (this file's `make`: in their start's state, and
 *   turned by FST_STATE ops as they play), each hashed as this engine plays it (LINE) and as history/dev ef2cbb5's
 *   engine plays it (CURVE: engine/tests/shape_old.c, run.sh builds it on that engine and plays them there). Here: each
 *   one read, its HEAD's settings 268, LINE heard; its twin (the same take, every shape CURVE) the hash ef2cbb5 gives
 *   the take itself: an engine before card #122 reads every take of this one, its shapes CURVE, the rest as heard.
 *   engine/tests/run.sh, or: shape [make DIR | hash FILE...] */
#include "../frasil_engine.c"
#include "frasil_stream.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if !FRS_LEAD
#error "ATTACK SHAPE's test: built with -DFRS_LEAD=1 and dsp/lead.c (engine/tests/run.sh)"
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
#define BLOCK_S (256.0f / 44100.0f)

static const uint32_t ATK[4] = {P_ATTAQUE, P_GL_ATTACK, P_BS_ATTACK, P_LD_ATTACK};
static const uint32_t AT3[3] = {0, 64, 100};
#ifndef SHAPE_BASELINE
/* the renders of before card #122 (history/dev ef2cbb5's engine, this test built with -DSHAPE_BASELINE), each track's
 * at AT3's ATTACKs */
static const uint64_t SHAPE_BEFORE[4][3] = {
    {0x35b5e366ca539661u, 0x75cfa68391eb728bu, 0x054a67a761f5e485u},
    {0xd3bf47f32be0f0b9u, 0x5b26ae5bc1d5ba7fu, 0xb9de1589505d0a2bu},
    {0x6fa0a04bdc349b73u, 0xe7c257b643bee94bu, 0x64f3ca76934d53c9u},
    {0xb1815d7e634516d9u, 0xfaec5ed1c189eb47u, 0xa0c96c907d1ff57fu},
};
#endif

static uint8_t sc0[FRS_SCENE_BYTES];
static void open_scene(void)
{
    frs_close();
    frs_init(44100.0f);
    frs_scene_factory(0, sc0);
    frs_scene_take(sc0);
}

/* a note on track t, ATTACK a (the LEAD's filter's too), after a block, 1.5 s of the mix (let go at 1 s): its hash.
 * turned: LINE (each shape) over that block, then CURVE again */
static uint64_t take(uint32_t t, uint32_t a, int turned)
{
    uint64_t h = 1469598103934665603u;
    uint32_t b, i, x;
    open_scene();
    frs_param_set(ATK[t], a);
    if (t == 3u)
        frs_param_set(P_LD_FATTACK, a);
#ifndef SHAPE_BASELINE
    for (i = 0; turned && i < 5u; i++)
        ashape_set(i, 1);
    frs_render(L, R, 256);
    for (i = 0; turned && i < 5u; i++)
        ashape_set(i, 0);
#else
    (void)turned;
    frs_render(L, R, 256);
#endif
    frs_param_set(P_KB, t);
    frs_key_down(0, 60, 100, 0);
    for (b = 0; b < 259u; b++) {
        if (b == 172u)
            frs_key_up(0);
        frs_render(L, R, 256);
        for (i = 0; i < 256u; i++) {
            memcpy(&x, &L[i], 4);
            h = (h ^ x) * 1099511628211u;
            memcpy(&x, &R[i], 4);
            h = (h ^ x) * 1099511628211u;
        }
    }
    return h;
}

#ifndef SHAPE_BASELINE
/* the level that rises on track t now (which 0: its gain, the envelope squared, but GLINT's onset; 1: the LEAD's
 * filter envelope) */
static float level(uint32_t t, int which)
{
    if (t == 0u) {
        frasil_view_t v[FR_NV];
        float m = 0.0f;
        uint32_t i;
        frasil_view(v);
        for (i = 0; i < FR_NV; i++)
            if (v[i].note >= 0 && v[i].gate && v[i].level > m)
                m = v[i].level;
        return m * m;
    }
    if (t == 1u) {
        glint_view_t v[GLINT_NV];
        uint32_t i;
        frasil_glint_view(v);
        for (i = 0; i < GLINT_NV; i++)
            if (v[i].note >= 0 && v[i].gate)
                return v[i].env;
        return 0.0f;
    }
    if (t == 2u) {
        bass_view_t v;
        frasil_bass_view(&v);
        return v.note >= 0 ? v.level * v.level : 0.0f;
    }
    {
        lead_view_t v;
        frasil_lead_view(&v);
        return which ? v.fenv : v.level * v.level;
    }
}
static int stage(uint32_t t)
{
    if (t == 2u) {
        bass_view_t v;
        frasil_bass_view(&v);
        return v.stage;
    }
    {
        lead_view_t v;
        frasil_lead_view(&v);
        return v.stage;
    }
}

/* track t's rise (which: its gain, or the LEAD's filter's), ATTACK a, its time ta (s, the DSP's), the shape line:
 * the blocks from the strike, the level at a quarter, half, three quarters and past ATTACK's time */
static void rise(uint32_t t, int which, uint32_t a, float ta, int line, const char *name)
{
    char w[160];
    float g[300], pace = BLOCK_S / ta, q1, q3, top;
    uint32_t k, k0 = 0, n = (uint32_t)(1.3f * ta / BLOCK_S) + 4u, b1, b2, b3;
    frs_close();                         /* (an engine as it opens: nothing sounding, each track on KEYS) */
    frs_init(44100.0f);
    frs_param_set(P_GL_DECAY, 127);      /* (held at the top: GLINT's ring and the others' DECAY out of the way) */
    frs_param_set(P_GL_SUSTAIN, 127);
    frs_param_set(P_BS_SUSTAIN, 127);
    frs_param_set(P_LD_SUSTAIN, 127);
    frs_param_set(P_LD_FSUSTAIN, 127);
    frs_param_set(ATK[t], a);
    if (which)
        frs_param_set(P_LD_FATTACK, a);
    ashape_set(which ? 4u : t, line);
    frs_param_set(P_SQ_TYPE, 0);         /* (GLINT on KEYS: a note held, no arpeggio) */
    frs_param_set(P_KB, t);
    for (k = 0; k < 40u; k++)            /* (the settings where they glide to: ATTACK's time its own from the strike) */
        frs_render(L, R, 256);
    frs_key_down(0, 60, 100, 0);
    for (k = 0; k < n && k < 300u; k++) {
        frs_render(L, R, 256);
        g[k] = level(t, which);
        if (!k0 && g[k] > 0.0f)
            k0 = k + 1u;                 /* (the block it began in, plus one: its end) */
    }
    if (!k0) {
        snprintf(w, sizeof w, "%s: nothing rose", name);
        check(0, w);
        return;
    }
    b1 = k0 - 1u + (uint32_t)(0.25f * ta / BLOCK_S + 0.5f);
    b2 = k0 - 1u + (uint32_t)(0.5f * ta / BLOCK_S + 0.5f);
    b3 = k0 - 1u + (uint32_t)(0.75f * ta / BLOCK_S + 0.5f);
    q1 = (g[b2] - g[b1]) / (float)(b2 - b1);   /* (its pace over the second quarter, and the third) */
    q3 = (g[b3] - g[b2]) / (float)(b3 - b2);
    top = g[k0 - 1u + (uint32_t)(1.1f * ta / BLOCK_S)];
    if (line) {
        snprintf(w, sizeof w, "%s LINE: %.4f then %.4f a block (ATTACK's %.4f), %.3f halfway, %.3f past ATTACK's time",
                 name, (double)q1, (double)q3, (double)pace, (double)g[b2], (double)top);
        check(fabsf(q1 - pace) < 0.01f * pace && fabsf(q3 - pace) < 0.01f * pace &&
                  fabsf(g[b2] - 0.5f) < 0.01f + 1.5f * pace && top > 0.999f, w);   /* (halfway: within a block or so, the
                                                                                    * strike somewhere in its first) */
        if (t >= 2u && !which) {
            snprintf(w, sizeof w, "%s LINE: DECAY's from the top", name);
            check(stage(t) == 1, w);
        }
    } else {
        snprintf(w, sizeof w, "%s CURVE: %.4f then %.4f a block, %.3f halfway (not a line)", name, (double)q1,
                 (double)q3, (double)g[b2]);
        check(q1 > 1.5f * q3 && g[b2] > 0.55f, w);
    }
}

/* the state's ATTACK SHAPE pair (STATE_ASHAPE): its value in a state of n bytes, or -1 */
static int pair_of(const uint8_t *s, uint32_t n)
{
    uint32_t i;
    for (i = 5; i + 1u < n; i += 2u)
        if (s[i] == STATE_ASHAPE)
            return s[i + 1u];
    return -1;
}

static void state(void)
{
    static uint8_t a[FRS_STATE_BYTES], b[FRS_STATE_BYTES], lv[FRS_LIVE_BYTES];
    static fst_start start;
    uint32_t i, j, n, m;
    check(NPARAM == P_LD_FRELEASE + 1u && NPARAM == 268u, "ATTACK SHAPE: no setting (NPARAM 268, the LEAD's last)");
    open_scene();
    check(st.ashape == 0u, "an engine opened: every shape CURVE");
    for (i = 0; i < 5u; i++)
        ashape_set(i, 1);
    check(!frs_dirty() && st.ashape == ASH_ALL, "LINE turned: the scene as saved (not dirty)");
    frs_scene_take(sc0);
    check(st.ashape == ASH_ALL, "a scene taken: the shapes as they were (none in it)");
    ashape_set(1, 0);                    /* GLINT's CURVE, the LEAD's filter's, the rest LINE */
    ashape_set(4, 0);
    n = frs_state_save(a);
    check(n <= FRS_STATE_BYTES && pair_of(a, n) == (ASH_DRONE | ASH_BASS | ASH_LEAD),
          "the state: its pair, 253 then a bit an envelope's LINE");
    for (i = 5, m = 5; i + 1u < n; i += 2u)   /* (the same state as an engine before card #122 saves it: no pair) */
        if (a[i] != STATE_ASHAPE) {
            b[m++] = a[i];
            b[m++] = a[i + 1u];
        }
    memcpy(b, a, 5);
    open_scene();
    check(frs_state_load(a, n) == 0 && st.ashape == (ASH_DRONE | ASH_BASS | ASH_LEAD), "back: the shapes as saved");
    check(frs_state_load(b, m) == 0 && st.ashape == 0u, "a state without it (an engine's before): every one CURVE");
    for (i = 0; i < 5u; i++)
        ashape_set(i, 0);
    n = frs_state_save(a);
    check(pair_of(a, n) == -1 && n == m && !memcmp(a, b, m), "every one CURVE: the state as before card #122, no pair");
    {                                    /* (an FST_STATE op of the FM-1's LIVE: the pair alone) */
        static const uint8_t T[] = {'F', 'R', 's', 't', 3, STATE_ASHAPE, ASH_GLINT | ASH_LFILT};
        check(frs_state_load(T, sizeof T) == 0 && st.ashape == (ASH_GLINT | ASH_LFILT), "the pair alone: the shapes it says");
    }
    open_scene();                        /* Perform LIVE: the start (its state) and the running state, a listener on them */
    ashape_set(0, 1);
    ashape_set(3, 1);
    frs_param_set(P_KB, 3);
    frs_key_down(0, 62, 100, 0);
    for (i = 0; i < 8u; i++)
        frs_render(L, R, 256);
    fst_capture(&start, 0);
    n = frs_live_save(lv, sizeof lv, start.scene);
    j = n && !fst_fresh(&start, 44100.0f) && frs_live_load(lv, n, start.scene) >= 0;
    check(j && st.ashape == (ASH_DRONE | ASH_LEAD), "a listener opened on the artist's start: the shapes the artist's");
}

/* ------------------------------------------------------------------- takes recorded with LINE, on any engine */
typedef struct { uint32_t ms; uint8_t op, n, a[6]; } tev_t;
static tev_t tev[64];
static uint32_t ntev;
static void tput(uint32_t ms, uint8_t op, uint8_t n, int a0, int a1, int a2, int a3)
{
    tev_t *e = &tev[ntev++];
    memset(e, 0, sizeof *e);
    e->ms = ms;
    e->op = op;
    e->n = n;
    e->a[0] = (uint8_t)a0;
    e->a[1] = (uint8_t)a1;
    e->a[2] = (uint8_t)a2;
    e->a[3] = (uint8_t)a3;
}
#define TAKE_MS 26000u
static const char *const TAKE[2] = {"line-start", "line-turned"};

/* NORTH RIVER, ATTACKs slow enough to hear their shape (GLINT on its keys), each track a note in turn, twice (the
 * drones', GLINT's, BASS's, the LEAD's: its filter's with it). Take 0: every shape LINE before it begins (its start's
 * state); 1: CURVE at the start, then FST_STATE (the pair alone, as the FM-1's LIVE tells it): every one LINE at 5.9 s,
 * GLINT's and BASS's CURVE again at 14.9 s. twin: the same take, every shape CURVE (its ops' pairs 0) */
static uint8_t *take_stream(int k, int twin, size_t *out)
{
    static fst_start start;
    static uint8_t sb[FST_SCENE_BYTES];
    uint8_t *b = malloc(1024u * 1024u), *s;
    uint32_t last = 0, i, r;
    size_t j;
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_step_stays(1);
    frs_scene_take(sc0);
    frs_param_set(P_ATTAQUE, 70);
    frs_param_set(P_GL_ATTACK, 100);
    frs_param_set(P_BS_ATTACK, 80);
    frs_param_set(P_LD_ATTACK, 80);
    frs_param_set(P_LD_FATTACK, 80);
    frs_param_set(P_SQ_TYPE, 0);
    if (k == 0 && !twin)
        for (i = 0; i < 5u; i++)
            ashape_set(i, 1);
    fst_capture(&start, 0);
    ntev = 0;
    for (r = 0; r < 2u; r++) {
        uint32_t t = 1000u + 12000u * r;
        tput(t, FST_KEY_DOWN, 4, 0, 48, 0, 100);
        tput(t + 2500u, FST_KEY_UP, 2, 0, 0, 0, 0);
        tput(t + 3000u, FST_KEY_DOWN, 4, 1, 72, 1, 100);
        tput(t + 4000u, FST_KEY_UP, 2, 1, 0, 0, 0);
        tput(t + 5000u, FST_KEY_DOWN, 4, 2, 36, 2, 100);
        tput(t + 7000u, FST_KEY_UP, 2, 2, 0, 0, 0);
        tput(t + 8000u, FST_KEY_LEAD, 4, 3, 60, 3, 100);
        tput(t + 10000u, FST_KEY_UP, 2, 3, 0, 0, 0);
    }
    if (k == 1) {                        /* (FST_STATE's: FSTJ_SCENE's inline bytes, a[4] the pair's value) */
        tput(5900u, FSTJ_SCENE, 3, FST_STATE, 0, 7, 0);
        tev[ntev - 1].a[4] = twin ? 0 : ASH_ALL;
        tput(14900u, FSTJ_SCENE, 3, FST_STATE, 0, 7, 0);
        tev[ntev - 1].a[4] = twin ? 0 : ASH_DRONE | ASH_LEAD | ASH_LFILT;
    }
    for (i = 1; i < ntev; i++)           /* (in time) */
        for (r = i; r > 0 && tev[r - 1u].ms > tev[r].ms; r--) {
            tev_t x = tev[r];
            tev[r] = tev[r - 1u];
            tev[r - 1u] = x;
        }
    j = fst_jhead(&start, 44100, 1793000000, b);
    for (i = 0; i < ntev; i++) {
        fst_ev e;
        memset(&e, 0, sizeof e);
        e.ms = tev[i].ms;
        e.op = tev[i].op;
        e.n = tev[i].n;
        memcpy(e.a, tev[i].a, 6);
        if (e.op == FSTJ_SCENE) {
            static const uint8_t T[5] = {'F', 'R', 's', 't', 3};
            memset(sb, 0, sizeof sb);
            memcpy(sb, T, 5);
            sb[5] = STATE_ASHAPE;
            sb[6] = tev[i].a[4];
            e.a[4] = 0;
            j += fst_jevent(&e, &last, sb, 0, b + j);
            continue;
        }
        j += fst_jevent(&e, &last, 0, 0, b + j);
    }
    {
        fst_ev e;
        memset(&e, 0, sizeof e);
        e.ms = TAKE_MS;
        e.op = FSTJ_END;
        j += fst_jevent(&e, &last, 0, 0, b + j);
    }
    s = fst_finish(b, j, out);
    free(b);
    return s;
}

/* a stream from the top to its end and 2 s more, as the radio plays it: its hash (0: not read, nparam its HEAD's) */
static uint64_t hear_stream(const uint8_t *b, size_t n, uint32_t *nparam)
{
    static fst_file f;
    static fst_air a;
    uint64_t h = 0xcbf29ce484222325ull;
    uint32_t i, x;
    if (fst_read(b, n, &f) || fst_air_open(&a, &f, 44100.0f))
        return 0;
    if (nparam)
        *nparam = f.nparam;
    while (fst_air_ms(&a) < f.ms + 2000u) {
        frs_render(L, R, 256u);
        fst_air_after(&a);
        for (i = 0; i < 256u; i++) {
            memcpy(&x, &L[i], 4);
            h = (h ^ x) * 1099511628211u;
            memcpy(&x, &R[i], 4);
            h = (h ^ x) * 1099511628211u;
        }
    }
    return h;
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
    *n = b && sz > 0 ? fread(b, 1, (size_t)sz, f) : 0u;
    fclose(f);
    return b;
}

/* engine/tests/line-streams/: each take read and heard (LINE: hashes.txt's), its twin heard as ef2cbb5 hears it */
static void line_streams(const char *dir)
{
    char path[1024], line[256], name[128], w[200];
    unsigned long long hl, hc;
    FILE *f;
    uint32_t seen = 0;
    snprintf(path, sizeof path, "%s/hashes.txt", dir);
    f = fopen(path, "r");
    check(f != 0, "engine/tests/line-streams/hashes.txt");
    while (f && fgets(line, sizeof line, f)) {
        size_t n, tn;
        uint8_t *b, *t;
        uint32_t np = 0, k;
        uint64_t h, ht;
        if (line[0] == '#' || sscanf(line, "%127s %llx %llx", name, &hl, &hc) != 3)
            continue;
        for (k = 0; k < 2u && strncmp(name, TAKE[k], strlen(TAKE[k])) != 0; k++)
            ;
        snprintf(path, sizeof path, "%s/%s", dir, name);
        b = slurp(path, &n);
        h = b ? hear_stream(b, n, &np) : 0u;
        t = k < 2u ? take_stream((int)k, 1, &tn) : 0;
        ht = t ? hear_stream(t, tn, 0) : 0u;
        snprintf(w, sizeof w, "%s: read, its HEAD's settings %u, LINE heard (%016llx), its twin all CURVE as ef2cbb5 "
                 "hears it (%016llx)", name, (unsigned)np, (unsigned long long)h, (unsigned long long)ht);
        check(h && np == NPARAM && h == hl && ht == hc && hl != hc, w);
        free(b);
        free(t);
        seen++;
    }
    if (f)
        fclose(f);
    check(seen == 2u, "line-streams: its two takes");
}

static int make(const char *dir)
{
    char path[1024];
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        size_t n;
        uint8_t *s = take_stream((int)k, 0, &n);
        FILE *f;
        snprintf(path, sizeof path, "%s/%s.frstream", dir, TAKE[k]);
        f = s ? fopen(path, "wb") : 0;
        if (!f || fwrite(s, 1, n, f) != n)
            return printf("make: %s not written\n", path), 1;
        fclose(f);
        free(s);
    }
    printf("make: 2 takes in %s\n", dir);
    return 0;
}
#endif

int main(int argc, char **argv)
{
    uint32_t t, j;
#ifndef SHAPE_BASELINE
    frs_scene_factory(0, sc0);
    if (argc >= 3 && !strcmp(argv[1], "make"))
        return make(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "hash")) {   /* (each one's as this engine hears it: NAME HASH) */
        int i;
        for (i = 2; i < argc; i++) {
            size_t n;
            uint8_t *b = slurp(argv[i], &n);
            printf("%s %016llx\n", argv[i], (unsigned long long)(b ? hear_stream(b, n, 0) : 0u));
            free(b);
        }
        return 0;
    }
#else
    (void)argc;
    (void)argv;
#endif
#ifdef SHAPE_BASELINE
    for (t = 0; t < 4u; t++)
        printf("    {0x%016llxu, 0x%016llxu, 0x%016llxu},\n", (unsigned long long)take(t, AT3[0], 0),
               (unsigned long long)take(t, AT3[1], 0), (unsigned long long)take(t, AT3[2], 0));
    (void)j;
    return 0;
#else
    static const char *const TRACK[4] = {"the drones", "GLINT", "BASS", "the LEAD"};
    char w[128];
    for (t = 0; t < 4u; t++)
        for (j = 0; j < 3u; j++) {
            uint64_t h = take(t, AT3[j], 0), h2 = take(t, AT3[j], 1);
            snprintf(w, sizeof w, "%s, ATTACK %u: CURVE as before card #122 (bit for bit), LINE turned and back the same",
                     TRACK[t], (unsigned)AT3[j]);
            check(h == SHAPE_BEFORE[t][j] && h2 == h, w);
        }
    /* LINE: the drones' (ATTACK 70: 0.005 x 6000^(70/127) s), GLINT's (100: 0.002 x 750^(100/127)), BASS's and the
     * LEAD's (80: 0.005 x 1000^(80/127)), the LEAD's filter's (the same) */
    for (j = 0; j < 2u; j++) {
        rise(0, 0, 70, 0.005f * powf(6000.0f, 70.0f / 127.0f), (int)!j, "the drones");
        rise(1, 0, 100, 0.002f * powf(750.0f, 100.0f / 127.0f), (int)!j, "GLINT");
        rise(2, 0, 80, 0.005f * powf(1000.0f, 80.0f / 127.0f), (int)!j, "BASS");
        rise(3, 0, 80, 0.005f * powf(1000.0f, 80.0f / 127.0f), (int)!j, "the LEAD");
        rise(3, 1, 80, 0.005f * powf(1000.0f, 80.0f / 127.0f), (int)!j, "the LEAD's filter");
    }
    state();
    line_streams("engine/tests/line-streams");
    printf("shape: %d ok, %d FAIL\n", oks, fails);
    return fails != 0;
#endif
}
