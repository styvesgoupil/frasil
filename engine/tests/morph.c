/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* A scene's MORPH (card #202: "sometimes it acts as an abrupt cut, even with very long transitions like 2 mn"), the
 * engine alone from the inside (frasil_engine.c's statics).
 *   The notes LOAD SND+NOTES swaps, NORTH RIVER to LONG NIGHT and back over 2:00 (the factory scenes; each drone voice's
 *   level as frs_live has it, squared as it is heard): the notes let go go out and the ones played come in over the
 *   MORPH, not on the scene left's ATTACK and RELEASE (the control, a MORPH as 3.3.1 made it, frs_morph_331: NORTH
 *   RIVER's let go at half their level in under a second, LONG NIGHT's in at half in a few). The ATTACK and RELEASE heard
 *   at its start at least its length, at its end the scene's own; a MORPH of a second on the scene left's (both longer
 *   than it), as before; CUT, LOAD SND ONLY and a drone's sequence as before.
 *   MASTERING's THRESHOLD and GAIN, CLOUD's level and MASTER EQ's seven as the DSP is given them (params_to_engine): at
 *   the start where they were, half-way between, at the end the scene's (3.3.1's: the scene's at once); MASTER EQ OFF
 *   to a scene's: from flat.
 *   Its mark in the state (frs_state_save): there, none while frs_morph_331; a state without it opened as a stream's
 *   (fst_fresh) plays 3.3.1's MORPH, loaded on a device (frs_state_load alone: the Vibe's) leaves this engine's.
 *   engine/tests/run.sh, or: morph */
#include <stdio.h>
#include <math.h>
#include "../frasil_engine.c"
#include "../frasil_stream.h"

static float L[256], R[256];
static int fails;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "  ok" : "FAIL", what);
    fails += !ok;
}

static void run_ms(uint32_t ms)          /* (256 frames a block, as the FM-1) */
{
    uint32_t end = frs_ms() + ms;
    while (frs_ms() < end)
        frs_render(L, R, 256);
}

static int in_chords(const scene_t *sc, int n)
{
    uint32_t c, k;
    for (c = 0; c < sc->nch; c++)
        for (k = 1; k < 5u; k++)
            if (sc->ch[c][k] == n)
                return 1;
    return 0;
}

/* a swap, measured every 50 ms from 60 ms after the scene is gone to, over the MORPH and 2 s more: the notes let go (the
 * scene left's, not the other's, sounding then) and the ones played from nothing (the other's alone, under a fifth of
 * their level then), each set's level summed. The 8 voices busy, a note played takes one let go at its level (later:
 * a crossfade of its own): neither set */
#define SW_N 2500u
typedef struct {
    int n_out, n_in;
    uint32_t dur;                        /* the MORPH's, ms */
    double out[SW_N], in[SW_N];
    float half_out, half_in;             /* when those let go are at half their level, those played at half their
                                          * level at the MORPH's end (s, 60 ms after it gone to; -1: never) */
} swap_t;
static swap_t sw;

static double sw_at(const double *x, uint32_t ms) { return x[(ms < 60u ? 0u : ms - 60u) / 50u]; }

static void swap(const uint8_t *from, const uint8_t *to, uint32_t morph, int as_331)
{
    scene_t a, b;
    float lv[FRS_LIVE];
    int outn[8], inn[8], i, j;
    uint32_t t, n;
    memcpy(&a, from, sizeof a);
    memcpy(&b, to, sizeof b);
    memset(&sw, 0, sizeof sw);
    frs_close();
    frs_init(44100.0f);
    frs_morph_331(as_331);
    frs_scene_take(from);
    run_ms(20000);
    frs_param_set(P_MORPH, morph);
    frs_scene_go(to);
    sw.dur = mo.on ? mo.dur : 0u;
    run_ms(60);
    frs_live(lv);
    for (i = 0; i < 8; i++) {
        int k = (int)lv[i];
        if (k < 0)
            continue;
        if (in_chords(&a, k) && !in_chords(&b, k))
            outn[sw.n_out++] = k;
        else if (in_chords(&b, k) && !in_chords(&a, k) && lv[8 + i] < 0.2f)
            inn[sw.n_in++] = k;
    }
    n = (sw.dur + 2000u) / 50u;
    for (t = 0; t < n && t < SW_N; t++) {
        frs_live(lv);
        for (i = 0; i < 8; i++)
            for (j = 0; j < sw.n_out || j < sw.n_in; j++) {
                double l = (double)lv[8 + i] * lv[8 + i];
                if (j < sw.n_out && (int)lv[i] == outn[j])
                    sw.out[t] += l;
                if (j < sw.n_in && (int)lv[i] == inn[j])
                    sw.in[t] += l;
            }
        run_ms(50);
    }
    sw.half_out = sw.half_in = -1.0f;
    for (t = 0; t < n && t < SW_N; t++) {
        if (sw.half_out < 0.0f && sw.out[t] < 0.5 * sw.out[0])
            sw.half_out = (float)(60u + 50u * t) * 0.001f;
        if (sw.half_in < 0.0f && sw.in[t] > 0.5 * sw_at(sw.in, sw.dur))
            sw.half_in = (float)(60u + 50u * t) * 0.001f;
    }
}

/* the ATTACK's and the RELEASE's times as the DSP is given them now (dsp/frasil.c's: 5 ms x 6000^x, 50 ms x 1200^x) */
static void env_times(float *att, float *rel)
{
    frasil_params_t b;
    params_to_engine(st.v, 0, &b);
    *att = 0.005f * powf(6000.0f, b.attack);
    *rel = 0.05f * powf(1200.0f, b.release);
}

static uint8_t NR[FRS_SCENE_BYTES], LN[FRS_SCENE_BYTES];

static void notes_test(void)
{
    char t[200];
    float o331, i331, att, rel;
    scene_t s;
    swap(NR, LN, 8u, 1);                 /* (3.3.1's: the control) */
    o331 = sw.half_out;
    swap(LN, NR, 8u, 1);
    i331 = sw.half_in;
    swap(NR, LN, 8u, 0);
    snprintf(t, sizeof t, "NORTH RIVER to LONG NIGHT over 2:00: its %d notes let go at half their level at %.1f s "
             "(3.3.1: %.1f s)", sw.n_out, sw.half_out, o331);
    check(sw.dur == 120000u && sw.n_out == 6 && sw.half_out >= 10.0f && o331 >= 0.0f && o331 < 2.0f, t);
    check(sw_at(sw.out, 30000u) > 0.05 * sw.out[0], "  at 30 s, still heard (over a twentieth of their level)");
    check(sw_at(sw.out, 120000u) < 0.01 * sw.out[0], "  at its end, gone (under a hundredth)");
    swap(LN, NR, 8u, 0);
    snprintf(t, sizeof t, "LONG NIGHT to NORTH RIVER over 2:00: its %d notes from nothing at half their level at %.1f s "
             "(3.3.1: %.1f s)", sw.n_in, sw.half_in, i331);
    check(sw.n_in == 6 && sw.half_in >= 20.0f && i331 >= 0.0f && i331 < 8.0f, t);
    check(sw_at(sw.in, 10000u) < 0.25 * sw_at(sw.in, 120000u), "  at 10 s, under a quarter of theirs");
    check(sw_at(sw.in, 120000u) > 0.95 * sw.in[(sw.dur + 2000u) / 50u - 1u], "  at its end, in whole");

    /* the ATTACK and RELEASE heard */
    frs_close();
    frs_init(44100.0f);
    frs_scene_take(NR);
    run_ms(500);
    frs_param_set(P_MORPH, 8u);
    frs_scene_go(LN);
    run_ms(20);
    env_times(&att, &rel);
    snprintf(t, sizeof t, "a 2:00 MORPH's start: ATTACK %.0f s, RELEASE %.0f s heard (NORTH RIVER's 1.9 s, 5.1 s)", att,
             rel);
    check(att > 115.0f && att < 125.0f && rel > 115.0f && rel < 125.0f, t);
    run_ms(60000);
    env_times(&att, &rel);
    snprintf(t, sizeof t, "  half-way, on their way back: ATTACK %.0f s, RELEASE %.0f s (LONG NIGHT's 9.4 s, 31 s)", att,
             rel);
    check(att > 10.0f && att < 110.0f && rel > 32.0f && rel < 110.0f, t);
    run_ms(60100);
    {
        frasil_params_t b;
        params_to_engine(st.v, 0, &b);
        check(!mo.on && b.attack == (float)st.v[P_ATTAQUE] / 127.0f && b.release == (float)st.v[P_RELACHE] / 127.0f &&
                  st.v[P_ATTAQUE] == 110u && st.v[P_RELACHE] == 115u,
              "  at its end, LONG NIGHT's own (110, 115)");
    }
    frs_param_set(P_MORPH, 1u);          /* (a second: LONG NIGHT's both longer, as before; each read as it is gone to) */
    frs_scene_go(NR);
    check((float)st.v[P_ATTAQUE] + morph_off[P_ATTAQUE] == 110.0f &&
              (float)st.v[P_RELACHE] + morph_off[P_RELACHE] == 115.0f,
          "a MORPH of a second: the ATTACK and RELEASE heard the scene left's (LONG NIGHT's, longer), as 3.3.1");
    run_ms(2000);
    frs_param_set(P_LOAD, 1u);           /* (SND ONLY: no note let go or played) */
    frs_param_set(P_MORPH, 8u);
    frs_scene_go(LN);
    check((float)st.v[P_ATTAQUE] + morph_off[P_ATTAQUE] == 87.0f &&
              (float)st.v[P_RELACHE] + morph_off[P_RELACHE] == 83.0f,
          "LOAD SND ONLY: the ATTACK and RELEASE heard the scene left's");
    frs_param_set(P_LOAD, 0u);
    run_ms(500);
    memcpy(&s, NR, sizeof s);
    s.dq_on = 1;                         /* (a drone's sequence: its steps on their own) */
    frs_scene_go((const uint8_t *)&s);
    check(st.v[P_DQ_ON] && (float)st.v[P_ATTAQUE] + morph_off[P_ATTAQUE] < 112.0f &&
              (float)st.v[P_RELACHE] + morph_off[P_RELACHE] < 117.0f,
          "a drone's sequence gone to: the ATTACK and RELEASE heard from where they were");
    frs_close();
}

/* a CUT: every sample as 3.3.1's */
static uint64_t cut_hash(int as_331)
{
    uint64_t h = 0xCBF29CE484222325u;
    int32_t lr[2 * 256];
    uint32_t k, j;
    frs_close();
    frs_init(44100.0f);
    frs_morph_331(as_331);
    frs_scene_take(NR);
    run_ms(3000);
    frs_param_set(P_MORPH, 0u);
    frs_scene_go(LN);
    for (k = 0; k < 600u; k++) {
        frs_render_i24(lr, 256);
        for (j = 0; j < 512u; j++) {
            h ^= (uint32_t)lr[j];
            h *= 0x100000001B3u;
        }
    }
    frs_close();
    return h;
}

/* MASTERING, CLOUD, MASTER EQ: two scenes of NEW SHARD's apart (a: its EQ OFF, eq_off) over 8 s */
static frasil_params_t pa, pb, p0, ph, p1;
static float h_thr, h_lowf;
static void master_go(int eq_off, int as_331)
{
    static const uint8_t P[11] = {P_MS_RATIO, P_MS_THRESH, P_MS_GAIN, P_MX_CLOUD, P_EQ_PRESET, P_EQ_LOW, P_EQ_MID,
                                  P_EQ_HIGH, P_EQ_LOWF, P_EQ_MIDF, P_EQ_Q},
                         X[2][11] = {{3, 30, 2, 100, 1, 34, 20, 30, 40, 50, 30}, {3, 10, 12, 40, 1, 10, 40, 20, 90, 100, 90}};
    scene_t a, b;
    uint32_t k;
    frs_scene_new((uint8_t *)&a);
    frs_scene_new((uint8_t *)&b);
    for (k = 0; k < 11u; k++) {
        a.v[P[k]] = X[0][k];
        b.v[P[k]] = X[1][k];
    }
    a.v[P_EQ_HIGHF] = 80;
    b.v[P_EQ_HIGHF] = 30;
    if (eq_off)
        a.v[P_EQ_PRESET] = 0;
    frs_close();
    frs_init(44100.0f);
    frs_morph_331(as_331);
    frs_scene_take((const uint8_t *)&b);
    params_to_engine(st.v, 0, &pb);
    frs_scene_take((const uint8_t *)&a);
    run_ms(300);
    params_to_engine(st.v, 0, &pa);
    frs_param_set(P_MORPH, 4u);
    frs_scene_go((const uint8_t *)&b);
    run_ms(20);
    params_to_engine(st.v, 0, &p0);
    run_ms(3980);
    params_to_engine(st.v, 0, &ph);
    h_thr = (float)st.v[P_MS_THRESH] + morph_off[P_MS_THRESH];
    h_lowf = (float)st.v[P_EQ_LOWF] + morph_off[P_EQ_LOWF];
    run_ms(4200);
    params_to_engine(st.v, 0, &p1);
    frs_close();
}
static int near(float x, float a, float b) { return fabsf(x - a) <= 0.01f * fabsf(b - a) + 1e-6f; }
static int between(float x, float a, float b) { return a < b ? x > a && x < b : x < a && x > b; }
#define MS_F(f) {#f, offsetof(frasil_params_t, f)}
static const struct {
    const char *name;
    size_t at;
} MF[10] = {MS_F(comp_thr), MS_F(comp_gain), MS_F(mx_cloud), MS_F(eq_low), MS_F(eq_mid), MS_F(eq_high), MS_F(eq_lowf),
            MS_F(eq_midf), MS_F(eq_q), MS_F(eq_highf)};
static float mf(const frasil_params_t *b, uint32_t i) { return *(const float *)((const char *)b + MF[i].at); }

static void master_test(void)
{
    uint32_t i, start = 0, half = 0, end = 0, once = 0;
    char t[200];
    master_go(0, 0);
    for (i = 0; i < 10u; i++) {
        start += near(mf(&p0, i), mf(&pa, i), mf(&pb, i));
        half += between(mf(&ph, i), mf(&pa, i), mf(&pb, i));
        end += mf(&p1, i) == mf(&pb, i);
        if (!near(mf(&p0, i), mf(&pa, i), mf(&pb, i)) || !between(mf(&ph, i), mf(&pa, i), mf(&pb, i)))
            printf("  (%s: %g, %g half-way, %g; from %g to %g)\n", MF[i].name, mf(&p0, i), mf(&ph, i), mf(&p1, i),
                   mf(&pa, i), mf(&pb, i));
    }
    snprintf(t, sizeof t, "MASTERING's THRESHOLD and GAIN, CLOUD's level, MASTER EQ's seven over 8 s: %u of 10 where they "
             "were at its start, %u on their way half-way, %u the scene's at its end", start, half, end);
    check(start == 10u && half == 10u && end == 10u && p0.eq_on && ph.eq_on && p1.eq_on, t);
    check(fabsf(h_thr - 20.0f) < 0.1f && fabsf(h_lowf - 65.0f) < 0.3f, "  half-way, THRESHOLD and LOW FREQ half-way");
    master_go(0, 1);
    for (i = 0; i < 10u; i++)
        once += mf(&p0, i) == mf(&pb, i);
    check(once == 10u, "  3.3.1's MORPH: each the scene's at once");
    master_go(1, 0);
    check(fabsf(p0.eq_low) < 0.01f && fabsf(p0.eq_mid) < 0.01f && fabsf(p0.eq_high) < 0.01f && ph.eq_on &&
              between(ph.eq_low, 0.0f, pb.eq_low) && between(ph.eq_mid, 0.0f, pb.eq_mid) &&
              between(ph.eq_high, 0.0f, pb.eq_high) && p1.eq_low == pb.eq_low,
          "MASTER EQ OFF to a scene's: its bands from flat, half-way on their way, then the scene's");
}

/* MORPH's mark in the state saved now: its value, -1 none */
static int mark(void)
{
    uint8_t s[FRS_STATE_BYTES];
    uint32_t n = frs_state_save(s), i;
    for (i = 5; i + 1u < n; i += 2u)
        if (s[i] == STATE_MORPH)
            return s[i + 1u];
    return -1;
}

static void state_test(void)
{
    static fst_start s;
    uint8_t old[FRS_STATE_BYTES];
    uint32_t n, m;
    frs_init(44100.0f);
    n = frs_state_save(old);
    check(mark() == 1 && n <= FRS_STATE_BYTES && old[n - 2u] == STATE_METRO, "a state: MORPH's mark, the metronome last");
    frs_morph_331(1);
    m = frs_state_save(old);
    check(mark() == -1 && m == n - 2u, "  none while frs_morph_331 (a stream of 3.3.1's played: its state's bytes)");
    frs_close();
    frs_init(44100.0f);
    check(frs_state_load(old, m) == 0 && !morph_331, "  one without it loaded on a device (the Vibe's): this engine's MORPH");
    frs_scene_take(NR);
    frs_morph_331(1);
    fst_capture(&s, 0);
    frs_morph_331(0);
    check(fst_fresh(&s, 44100.0f) == 0 && morph_331, "  opened as a stream's (fst_fresh), without it: 3.3.1's");
    fst_capture(&s, 0);
    check(fst_fresh(&s, 44100.0f) == 0 && morph_331 && mark() == -1, "  its state taken again: none still (a snap of it)");
    frs_morph_331(0);
    fst_capture(&s, 0);
    frs_morph_331(1);
    check(fst_fresh(&s, 44100.0f) == 0 && !morph_331, "  with it: this engine's");
    frs_close();
}

int main(void)
{
    frs_init(44100.0f);
    frs_scene_factory(0, NR);
    frs_scene_factory(3, LN);
    check(!strncmp((const char *)NR, "NORTH RIVER", 11) && !strncmp((const char *)LN, "LONG NIGHT", 10),
          "the factory scenes: NORTH RIVER, LONG NIGHT");
    frs_close();
    notes_test();
    check(cut_hash(0) == cut_hash(1), "a CUT: every sample as 3.3.1's");
    master_test();
    state_test();
    if (fails)
        return printf("morph: %d failed\n", fails), 1;
    printf("morph: ok (2:00 between NORTH RIVER and LONG NIGHT, the notes over it; MASTERING, CLOUD, MASTER EQ gliding; "
           "MORPH's mark)\n");
    return 0;
}
