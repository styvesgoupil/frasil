/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* A skip against a render: a listener who joins a stream at T hears the performance a listener from the top hears. Takes
 * made through the real format (fst_jhead, fst_jevent, fst_finish, fst_read) on every factory scene, with scene changes,
 * CUT, tempo, tracks stopped and played, keys and fingers, MIDI (at once and ahead of its stamp), an engine opened again.
 * Every message the brain sends the DSP from T on, and every setting it pushes, compared with a render's: its millisecond
 * in the stream, kind, note, value. Controls that must differ: without them this test could pass on nothing.
 *   engine/tests/run.sh (-DFRASIL_EV_LOG=skip_log), or: skip */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../frasil_engine.h"
#include "../frasil_params.h"
#include "../frasil_stream.h"

#define BIG 2000000
typedef struct { uint32_t ms; uint8_t t; int8_t n; float v; } msg_t;
static msg_t *got, *ref;
static int ngot, nref, logging;
static fst_air air;

void skip_log(uint8_t type, int note, float v);
void skip_log(uint8_t type, int note, float v)
{
    if (logging && ngot < BIG && fst_air_ms(&air) < 0x80000000u) {   /* (not as an engine opens again: no clock yet) */
        got[ngot].ms = fst_air_ms(&air);
        got[ngot].t = type;
        got[ngot].n = (int8_t)note;
        got[ngot].v = v;
        ngot++;
    }
}

static uint32_t rs;
static uint32_t rnd(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 17;
    rs ^= rs << 5;
    return rs;
}

/* a take */
typedef struct { uint32_t ms; uint8_t op, n; uint8_t a[6]; int scene; } ev_t;
static ev_t ev[200000];
static int nev;
static void put(uint32_t ms, uint8_t op, uint8_t n, int a0, int a1, int a2, int a3, int scene)
{
    ev_t e;
    memset(&e, 0, sizeof e);
    e.ms = ms;
    e.op = op;
    e.n = n;
    e.a[0] = (uint8_t)a0;
    e.a[1] = (uint8_t)a1;
    e.a[2] = (uint8_t)a2;
    e.a[3] = (uint8_t)a3;
    e.scene = scene;
    ev[nev++] = e;
}

static int by_ms(const void *a, const void *b)
{
    const ev_t *x = a, *y = b;
    return x->ms < y->ms ? -1 : x->ms > y->ms;
}

/* midi 0: none; 1: notes at once and ahead of their stamps */
static void perform(uint32_t len, uint32_t seed, int nscenes, int midi)
{
    uint32_t t = 500;
    int s = 0;
    nev = 0;
    rs = seed;
    put(t, FST_LATCH, 4, 0, 48, -1, 0, -1);
    t += 1500;
    put(t, FST_LATCH, 4, 1, 72, -1, 0, -1);
    put(t + 1, FST_LATCH, 4, 1, 76, -1, 0, -1);
    put(t + 2, FST_LATCH, 4, 1, 79, -1, 0, -1);
    put(t + 3, FST_LATCH, 4, 2, 36, -1, 0, -1);
    while (t < len - 3000u) {
        uint32_t k;
        t += 120u + rnd() % 900u;
        k = rnd() % 10u;
        if (k == 0u) {
            int n = 60 + (int)(rnd() % 24u);
            put(t, FST_HOLD, 4, 1, n, 1, -1, -1);
            put(t + 150u + rnd() % 300u, FST_HOLD, 4, 1, n, 0, -1, -1);
        } else if (k == 1u) {
            uint32_t i, x = rnd() % 128u;
            for (i = 0; i < 24u; i++)
                put(t + i * 16u, FST_PARAM, 2, P_TIMBRE, (int)((x + i) & 127u), 0, 0, -1);
        } else if (k == 2u) {
            put(t, FST_PARAM, 2, P_ESPACE, (int)(rnd() % 128u), 0, 0, -1);
        } else if (k == 3u) {
            put(t, FST_LATCH, 4, 0, 48 + (int)(rnd() % 24u), -1, 0, -1);
        } else if (k == 4u) {
            put(t, FST_TRACK_PLAY, 2, 2, (int)(rnd() & 1u), 0, 0, -1);
        } else if (k == 5u) {
            put(t, FST_PARAM, 2, P_MOUVEMENT, (int)(rnd() % 128u), 0, 0, -1);
        } else if (k == 6u) {
            int key = (int)(rnd() % 8u), n = 55 + (int)(rnd() % 24u);
            put(t, FST_KEY_DOWN, 4, key, n, (int)(rnd() % 3u), 0, -1);
            put(t + 90u + rnd() % 500u, FST_KEY_UP, 2, key, 0, 0, 0, -1);
        } else if (k == 7u && midi) {
            int n = 40 + (int)(rnd() % 40u), ahead = (int)(rnd() % 5u);
            put(t, FST_MIDI, 4, 0x90, n, 96, ahead, -1);
            put(t + 80u + rnd() % 400u, FST_MIDI, 4, 0x80, n, 0, (int)(rnd() % 5u), -1);
        } else if (k == 8u && nscenes && rnd() % 12u == 0u) {
            s = (s + 1) % nscenes;
            put(t, FST_SCENE, 2, 0, 0, 0, 0, s);
        }
    }
    if (nscenes) {
        put(len / 2u + 777u, FST_CUT, 0, 0, 0, 0, 0, -1);
        put(len / 2u + 2777u, FST_PLAY_ALL, 2, 1, 0, 0, 0, -1);
    }
    qsort(ev, (size_t)nev, sizeof ev[0], by_ms);
}

static fst_start start;
static int blank;                        /* take(): a NEW SHARD's scene, not a factory one (the held notes' cases) */
static uint8_t scene[FRS_SCENE_BYTES];

/* the take on scene i as a stream; open: the engine opened again there on the next scene (0: not) */
static uint8_t *take(uint32_t i, uint32_t len, int nscenes, uint32_t open, size_t *out)
{
    uint32_t last = 0, opened = 0;
    int k;
    size_t cap = 8u * 1024u * 1024u, j;
    uint8_t *b = malloc(cap), *s;
    static fst_start again;
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_step_stays(1);
    if (open) {
        frs_scene_factory((i + 1u) % (uint32_t)nscenes, scene);
        frs_scene_take(scene);
        fst_capture(&again, 0);
    }
    if (blank)
        frs_scene_new(scene);
    else
        frs_scene_factory(i, scene);
    frs_scene_take(scene);
    fst_capture(&start, 0);
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
        if (ev[k].scene >= 0) {
            static uint8_t sc2[FRS_SCENE_BYTES];
            frs_scene_factory((uint32_t)ev[k].scene, sc2);
            e.op = FSTJ_SCENE;
            e.a[0] = ev[k].op;
            j += fst_jevent(&e, &last, sc2, 0, b + j);
        } else {
            e.op = ev[k].op;
            e.n = ev[k].n;
            memcpy(e.a, ev[k].a, 6);
            j += fst_jevent(&e, &last, 0, 0, b + j);
        }
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

static fst_file file;
static float L[4096], R[4096];
static int open_it(const uint8_t *b, size_t n)
{
    int r = fst_read(b, n, &file);
    logging = 0;                         /* (closing the last engine sends what it sends) */
    if (r || fst_air_open(&air, &file, 44100.0f))
        return r ? r : -9;
    ngot = 0;
    logging = 1;
    return 0;
}

static void play_to(uint32_t ms, uint32_t quantum)
{
    while (fst_air_ms(&air) < ms) {
        frs_render(L, R, quantum);
        fst_air_after(&air);
    }
}

/* a run of `got` against `ref` over [t, t + span): 0 the same, 1 different, 2 nothing compared */
static int compare(uint32_t t, uint32_t span, int *count)
{
    int a = 0, b = 0, c = 0;
    while (a < nref && ref[a].ms < t)
        a++;
    while (b < ngot && got[b].ms < t)
        b++;
    while (a < nref && ref[a].ms < t + span) {
        if (b >= ngot) {
            if (getenv("SKIP_DEBUG"))
                printf("  ran out after %d, at ref %u t%u n%d\n", c, ref[a].ms, ref[a].t, ref[a].n);
            return 1;
        }
        if (ref[a].ms != got[b].ms || ref[a].t != got[b].t || ref[a].n != got[b].n || ref[a].v != got[b].v) {
            if (getenv("SKIP_DEBUG"))
                printf("  first difference, after %d: ref %u t%u n%d v%.4f, got %u t%u n%d v%.4f\n", c, ref[a].ms, ref[a].t, ref[a].n, (double)ref[a].v,
                       got[b].ms, got[b].t, got[b].n, (double)got[b].v);
            return 1;
        }
        a++;
        b++;
        c++;
    }
    if (b < ngot && got[b].ms < t + span) {
        if (getenv("SKIP_DEBUG"))
            printf("  more than the render's after %d: got %u t%u n%d\n", c, got[b].ms, got[b].t, got[b].n);
        return 1;
    }
    *count = c;
    return c ? 0 : 2;
}

/* the brain as it stands: everything frs_view shows, every setting */
typedef struct { frs_view_t v; uint32_t p[256]; } state_t;
static state_t was, now;
static void state_of(state_t *s)
{
    uint32_t i, n = frs_param_count();
    memset(s, 0, sizeof *s);
    frs_view(&s->v);
    for (i = 0; i < n && i < 256u; i++)
        s->p[i] = frs_param_get(i);
}

static void as_reference(void)
{
    msg_t *x = ref;
    ref = got;
    got = x;
    nref = ngot;
    ngot = 0;
}

/* ---- what sounds after a skip: heard, not only set. A take's level over a second, and what plays then */
#include <math.h>
typedef struct { double db, db3; int notes[16], n; } heard_t;
static double level_db(uint32_t frames)
{
    double e = 0.0;
    uint32_t done = 0, i;
    while (done < frames) {
        frs_render(L, R, 256u);
        fst_air_after(&air);
        for (i = 0; i < 256u; i++)
            e += (double)L[i] * L[i] + (double)R[i] * R[i];
        done += 256u;
    }
    return 10.0 * log10(e / (2.0 * done) + 1e-20);
}
static int by_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
/* from the place reached: the first second's level, what plays at its end (drones, glints, the bass, in that order of
 * kind: 0..127 the drones', 200.. the glints', 400.. the bass's), the fourth second's level */
static void hear(heard_t *h)
{
    float lv[FRS_LIVE];
    uint32_t i;
    h->db = level_db(44100u);
    frs_live(lv);
    h->n = 0;
    for (i = 0; i < 8u; i++)
        if (lv[i] >= 0.0f && lv[8 + i] > 1e-4f)
            h->notes[h->n++] = (int)lv[i];
    for (i = 0; i < 4u; i++)
        if (lv[16 + i] >= 0.0f && lv[20 + i] > 0.0f)
            h->notes[h->n++] = 200 + (int)lv[16 + i];
    if (lv[24] >= 0.0f && lv[25] > 1e-4f)
        h->notes[h->n++] = 400 + (int)lv[24];
    qsort(h->notes, (size_t)h->n, sizeof h->notes[0], by_int);
    level_db(2u * 44100u);
    h->db3 = level_db(44100u);
}
static void notes_str(const heard_t *h, char *o)
{
    int i;
    o[0] = 0;
    for (i = 0; i < h->n; i++)
        o += sprintf(o, "%s%s%d", i ? " " : "", h->notes[i] >= 400 ? "B" : h->notes[i] >= 200 ? "G" : "D", h->notes[i] % 200);
    if (!h->n)
        sprintf(o, "-");
}

/* one case: the take as `ev` holds it, a listener from the top against two who skip to T (warmed 5 s, and not) */
/* silent: 1 nothing must sound; 2 its cloud and echoes count: only the warmed skip is held to the render (a skip's cloud
 * is empty, the warm-up fills it) */
static int held_case(const char *name, uint32_t sc, uint32_t len, uint32_t T, int silent)
{
    heard_t ref, w5, w0;
    size_t n;
    uint8_t *blob = take(sc, len, 7, 0, &n);
    char a[200], b[200], c[200];
    int bad = 0;
    if (!blob || open_it(blob, n))
        return printf("FAIL %s: the take does not read\n", name), 1;
    logging = 0;
    play_to(T, 256u);
    hear(&ref);
    open_it(blob, n);
    logging = 0;
    fst_air_skip(&air, T - 5000u);
    play_to(T, 256u);
    hear(&w5);
    open_it(blob, n);
    logging = 0;
    fst_air_skip(&air, T);
    hear(&w0);
    notes_str(&ref, a);
    notes_str(&w5, b);
    notes_str(&w0, c);
    if (silent == 2) {
        bad = ref.db < -60.0 || fabs(w5.db - ref.db) > 6.0 || fabs(w5.db3 - ref.db3) > 6.0 || strcmp(a, b) || strcmp(a, c);
    } else if (silent) {
        bad = ref.db > -90.0 || w5.db > -90.0 || w0.db > -90.0 || w5.n || w0.n;
    } else {
        bad = ref.db < -60.0                                  /* (the case itself plays) */
              || fabs(w5.db - ref.db) > 6.0 || fabs(w5.db3 - ref.db3) > 6.0   /* warmed: heard as the render, from its first second */
              || w0.db < ref.db - 30.0 || fabs(w0.db3 - ref.db3) > 6.0        /* not warmed: heard at once, the render's by the 4th */
              || strcmp(a, b) || strcmp(a, c);                               /* the same notes sounding */
    }
    printf("%s %-34s render %6.1f dB (4th s %6.1f) [%s] | skip, 5 s warmed %6.1f (%6.1f) [%s] | skip alone %6.1f (%6.1f) [%s]\n",
           bad ? "FAIL" : "  ok", name, ref.db, ref.db3, a, w5.db, w5.db3, b, w0.db, w0.db3, c);
    free(blob);
    return bad;
}

static int held_tests(void)
{
    const uint32_t len = 300000u, T = 240000u;   /* (four minutes after it began) */
    uint32_t sc;
    int fails = 0;
    blank = 1;
    nev = 0;
    put(1000, FST_HOLD, 4, 2, 40, 1, -1, -1);
    fails += held_case("a bass held by a finger", 0, len, T, 0);
    nev = 0;
    put(1000, FST_LATCH, 4, 2, 40, -1, 0, -1);
    fails += held_case("a bass latched", 0, len, T, 0);
    nev = 0;
    put(1000, FST_LATCH, 4, 0, 48, -1, 0, -1);
    fails += held_case("a drone chord latched", 0, len, T, 0);
    nev = 0;
    put(1000, FST_HOLD, 4, 0, 50, 1, -1, -1);
    fails += held_case("a drone chord held by a finger", 0, len, T, 0);
    nev = 0;
    put(400, FST_PARAM, 2, P_SQ_TYPE, 0, 0, 0, -1);   /* (GLINT's keys: no arpeggio striking it again) */
    put(500, FST_PARAM, 2, P_GL_SUSTAIN, 100, 0, 0, -1);
    put(1000, FST_HOLD, 4, 1, 76, 1, -1, -1);
    fails += held_case("a GLINT held, SUSTAIN 100", 0, len, T, 0);
    nev = 0;                             /* (dry: no cloud, no echoes; the glint alone) */
    put(400, FST_PARAM, 2, P_SQ_TYPE, 0, 0, 0, -1);
    put(500, FST_PARAM, 2, P_GL_DECAY, 127, 0, 0, -1);
    put(501, FST_PARAM, 2, P_GL_SPACE, 0, 0, 0, -1);
    put(502, FST_PARAM, 2, P_GD_MIX, 0, 0, 0, -1);
    put(T - 2000u, FST_HOLD, 4, 1, 76, 1, -1, -1);
    fails += held_case("a GLINT ringing, struck 2 s before", 0, len, T, 0);
    ev[nev - 1].ms = T - 3000u;
    fails += held_case("a GLINT ringing, struck 3 s before", 0, len, T, 0);
    nev = 0;
    put(400, FST_PARAM, 2, P_SQ_TYPE, 0, 0, 0, -1);
    put(500, FST_PARAM, 2, P_GL_DECAY, 127, 0, 0, -1);
    put(T - 2000u, FST_HOLD, 4, 1, 76, 1, -1, -1);
    fails += held_case("the same, its cloud and echoes", 0, len, T, 2);
    nev = 0;
    put(400, FST_PARAM, 2, P_SQ_TYPE, 0, 0, 0, -1);
    put(1000, FST_HOLD, 4, 0, 48, 1, -1, -1);
    put(1000, FST_HOLD, 4, 2, 40, 1, -1, -1);
    put(1000, FST_HOLD, 4, 1, 76, 1, -1, -1);
    put(60000, FST_HOLD, 4, 0, 48, 0, -1, -1);
    put(60000, FST_HOLD, 4, 2, 40, 0, -1, -1);
    put(60000, FST_HOLD, 4, 1, 76, 0, -1, -1);
    fails += held_case("all let go long before (silent)", 0, len, T, 1);
    nev = 0;
    put(500, FST_PARAM, 2, P_SQ_TYPE, 1, 0, 0, -1);
    put(1000, FST_LATCH, 4, 1, 72, -1, 0, -1);
    put(1001, FST_LATCH, 4, 1, 76, -1, 0, -1);
    put(1002, FST_LATCH, 4, 1, 79, -1, 0, -1);
    put(1003, FST_LATCH, 4, 0, 48, -1, 0, -1);
    fails += held_case("GLINT's arpeggio latched, a drone", 0, len, T, 0);
    blank = 0;
    for (sc = 0; sc < 7u; sc++) {        /* each factory scene alone, as a SHARD plays on air: its sequences, its LFOs */
        char nm[64];
        frs_scene_factory(sc, scene);
        nev = 0;
        put(0, FST_RECORDING, 4, 0, 0, 0, 0, -1);   /* (a take holds something: what the iPad writes first) */
        snprintf(nm, sizeof nm, "factory %.12s, alone", (const char *)scene);
        fails += held_case(nm, sc, len, T, 0);
    }
    return fails;
}

int main(void)
{
    uint32_t count = 0, sc;
    int fails = 0, compares = 0, total = 0, states = 0, tempo = 0, latch = 0, late = 0, gone = 0, buffers = 0, opens = 0, midi_seen = 0;
    const uint32_t len = 240000u, Ts[3] = {60000u, 121000u, 200000u}, span = 25000u;
    size_t n;
    got = malloc(sizeof(msg_t) * BIG);
    ref = malloc(sizeof(msg_t) * BIG);
    frs_init(44100.0f);
    while (frs_scene_factory(count, scene) == 0)
        count++;
    printf("%u factory scenes, engine %s\n", count, frs_version());
    if (getenv("SKIP_HELD_ONLY"))
        return held_tests() ? 1 : 0;
    for (sc = 0; sc < count; sc++) {
        uint8_t *blob;
        int k, c = 0, r;
        perform(len, 4242u + sc, (int)count, 1);
        blob = take(sc, len, (int)count, 0, &n);
        if (!blob || (r = open_it(blob, n)) != 0) {
            printf("FAIL scene %u: the take does not read (%d)\n", sc, blob ? r : -99);
            return 1;
        }
        midi_seen += !(file.flags & FST_F_HANDS);
        /* from the top, 256 frames at a time: what every listener's skip must equal */
        play_to(len, 256u);
        as_reference();
        /* any other buffer: the same (a browser's 128, an odd one) */
        if (open_it(blob, n))
            return 1;
        play_to(100000u, 128u);
        r = compare(0, 100000u, &c);
        if (open_it(blob, n))
            return 1;
        play_to(100000u, 333u);
        if (r || compare(0, 100000u, &c)) {
            printf("FAIL scene %u: another buffer, other messages\n", sc);
            fails++;
        } else {
            buffers++;
        }
        for (k = 0; k < 3; k++) {
            uint32_t T = Ts[k];
            if (open_it(blob, n))
                return 1;
            logging = 0;
            fst_air_skip(&air, T);
            logging = 1;
            play_to(T + span, 128u);
            r = compare(T, span, &c);
            compares++;
            total += c;
            if (r) {
                printf("FAIL scene %u T %u: a skip's messages are not a render's (%d, %d compared)\n", sc, T, r, c);
                fails++;
            }
        }
        free(blob);
        /* the controls, on a take with no CUT and no scene to wipe out what came before: a lasting change, one each */
        perform(len, 7000u + sc, 0, 1);
        blob = take(sc, len, (int)count, 0, &n);
        {
            const uint32_t Tp = 30000u, T = 90000u;
            uint32_t tp, Ts2;
            int m, m2;
            uint8_t *b2;
            if (!blob || open_it(blob, n))
                return 1;
            play_to(T, 256u);
            Ts2 = fst_air_ms(&air);
            state_of(&was);
            play_to(T + span + 10u, 256u);
            as_reference();
#define SKIP_STATE(differs) do { logging = 0; fst_air_skip(&air, Ts2); logging = 1; state_of(&now); \
                                 play_to(T + span, 128u); (differs) = compare(T, span, &c) == 1 || memcmp(&was, &now, sizeof was) != 0; } while (0)
            /* the same take, skipped: must be the same, or the controls prove nothing */
            open_it(blob, n);
            SKIP_STATE(m);
            if (m) {
                printf("FAIL scene %u: the controls' take, skipped, differs (messages or the brain's state at T)\n", sc);
                fails++;
            } else {
                states++;
            }
            /* TEMPO for a second */
            open_it(blob, n);
            logging = 0;
            fst_air_skip(&air, Tp);
            tp = frs_param_get(P_TEMPO);
            frs_param_set(P_TEMPO, tp + 3u);
            fst_air_skip(&air, Tp + 1000u);
            frs_param_set(P_TEMPO, tp);
            SKIP_STATE(m);
            tempo += m;
            /* one more note latched */
            open_it(blob, n);
            logging = 0;
            fst_air_skip(&air, T - 2000u);
            frs_latch(0, 91, -1);
            SKIP_STATE(m);
            latch += m;
            /* a hand's event gone: the last SPACE before T */
            for (m = nev - 1; m >= 0 && !(ev[m].ms < T && ev[m].op == FST_PARAM && ev[m].a[0] == P_ESPACE); m--)
                ;
            if (m >= 0) {
                ev_t old = ev[m];
                ev[m].a[0] = P_TIMBRE;
                b2 = take(sc, len, (int)count, 0, &n);
                open_it(b2, n);
                SKIP_STATE(m2);
                gone += m2;
                free(b2);
                ev[m] = old;
            }
            /* MIDI taken 40 ms late */
            for (m = 0, tp = 0; m < nev; m++)
                if (ev[m].op == FST_MIDI && ev[m].ms >= T) {
                    ev[m].a[3] = (uint8_t)(ev[m].a[3] + 40u);
                    tp++;
                }
            b2 = take(sc, len, (int)count, 0, &n);
            open_it(b2, n);
            SKIP_STATE(m);
            late += tp && m;
            free(b2);
        }
        free(blob);
        /* the engine opened again mid-take: a skip across it, the messages after it as a render's */
        perform(len, 9000u + sc, 0, 1);
        blob = take(sc, len, (int)count, 90000u, &n);
        if (!blob || open_it(blob, n)) {
            printf("FAIL scene %u: the opened take does not read\n", sc);
            return 1;
        }
        play_to(150000u, 256u);
        as_reference();
        open_it(blob, n);
        logging = 0;
        fst_air_skip(&air, 120000u);
        logging = 1;
        play_to(150000u, 128u);
        r = compare(120000u, 25000u, &c);
        if (r) {
            printf("FAIL scene %u: a skip across an opening is not a render's (%d)\n", sc, r);
            fails++;
        } else {
            opens++;
        }
        logging = 0;
        free(blob);
    }
    printf("%d skips on %u scenes, %d messages the same; %d brain states at T the same; %d buffers the same; %d skips across an opening the same\n",
           compares, count, total, states, buffers, opens);
    printf("controls that must differ, of %u: TEMPO for a second %d, a note latched %d, a hand's event dropped %d, MIDI 40 ms late %d\n", count,
           tempo, latch, gone, late);
    if (midi_seen != (int)count)
        fails++, printf("FAIL takes with no MIDI: %d of %u had it\n", midi_seen, count);
    if (tempo != (int)count || latch + 1 < (int)count || gone != (int)count || late != (int)count)
        fails++, printf("FAIL the controls show too little: this test may prove nothing\n");
    {
        int h = held_tests();
        printf("held notes after a skip: %s\n", h ? "FAILED" : "heard as the render's");
        fails += h;
    }
    printf("skip: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
