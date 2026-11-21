/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* LOFI TAPE's MEDIA (card #99): P_TP_ON's values, OFF, TAPE, VINYL, AM, PHONE (card #218; TAPE the one there was: ON's
 * 1). TAPE as it was, bit for bit (renders hashed on the engine before the media, 70c3ad07: every setting of LOFI TAPE
 * and LOFI TAPE II moved, its TRACKS, ROUTING, off and on again, at 44.1 and 48 kHz), AM the same renders as on the
 * engine before PHONE (49bcaa81; VINYL its own since card #224); the words; VINYL, AM and PHONE on a factory scene at
 * their gentlest and their hardest, finite, bounded, as loud as the tape; AM and PHONE one side, PHONE's band (its lows
 * gone, its hiss in it); silent in, silent out at NOISE 0 (no crackle, no rumble, no static, no hiss, no hum), NOISE's
 * own noise on silence, DROPOUT's dips (PHONE's cuts); a medium changed while it plays, through its fade (no click);
 * the effects' view of the one sounding. `make DIR`: three takes of this engine (PHONE, the same on AM, on TAPE) that
 * engine/tests/media_old.c hears on the engines before PHONE (run.sh builds it on them from git).
 *   VINYL's record player (card #224): OFF and AM as on the engine before it (history/dev 49bcaa81), CUT and the iPad's
 * STOP on OFF, TAPE and AM too; VINYL silent on silence till music reaches it (the needle up), its needle's drop then (a
 * thump), none on a join (a skip); its noise on the silence after music (the needle down); its marks ticking again at
 * each turn; its lows one side; CUT: the record slowing to a stop (sounding on, its pitch down, silent within VN_STOP,
 * no click), the needle up after it, PLAY again sounding as soon as after the tape's CUT, a note in the stop never held
 * back; at 48 and 96 kHz too.
 *   engine/tests/run.sh, or: media [make DIR] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../frasil_engine.h"
#include "../frasil_stream.h"

#define NM 5u                             /* OFF .. PHONE */
static const char *const MEDIUM[NM] = {"OFF", "TAPE", "VINYL", "AM", "PHONE"};
static int fails;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "  ok" : "FAIL", what);
    fails += !ok;
}
static void check_m(int ok, uint32_t m, const char *what)   /* (a medium's) */
{
    printf("%s %s: %s\n", ok ? "  ok" : "FAIL", MEDIUM[m], what);
    fails += !ok;
}

static uint64_t fnv(const int32_t *lr, uint32_t n, uint64_t h)
{
    uint32_t i;
    for (i = 0; i < 2u * n; i++) {
        h ^= (uint32_t)lr[i];
        h *= 0x100000001B3u;
    }
    return h;
}

static int32_t lr[2 * 4096];
static uint64_t hash_run(uint64_t h, uint32_t frames)   /* frames rendered, into the hash */
{
    while (frames) {
        uint32_t n = frames < 1000u ? frames : 1000u;   /* (not a power of two: the blocks' edges move) */
        frs_render_i24(lr, n);
        h = fnv(lr, n, h);
        frames -= n;
    }
    return h;
}

/* a factory scene at fs, its notes, the medium on with its settings: (SATURATION, WOBBLE, TAPE SPEED, TONE, NOISE,
 * DROPOUT) */
static void open_on(float fs, uint32_t scene, uint32_t medium, const uint8_t *s)
{
    uint8_t sc[FRS_SCENE_BYTES];
    frs_close();
    frs_init(fs);
    frs_scene_factory(scene, sc);
    frs_scene_take(sc);
    frs_param_set(P_TP_ON, medium);
    frs_param_set(P_TP_SAT, s[0]);
    frs_param_set(P_TP_WOBBLE, s[1]);
    frs_param_set(P_TP_FLUTTER, s[2]);
    frs_param_set(P_TP_TONE, s[3]);
    frs_param_set(P_TP_NOISE, s[4]);
    frs_param_set(P_TP_DROPOUT, s[5]);
}

/* TAPE's renders (medium 1), as they were hashed on the engine before the media; VINYL's and AM's, as on the engine
 * before PHONE */
static uint64_t tape_a(uint32_t medium)  /* 44.1 kHz: LUMIERE NOCTURNE through a worn tape, every setting moved */
{
    static const uint8_t S[6] = {80, 90, 64, 30, 50, 70};
    uint64_t h = 0xCBF29CE484222325u;
    open_on(44100.0f, 1, medium, S);
    h = hash_run(h, 6u * 44100u);
    frs_param_set(P_TP_TRACKS, FRS_TRACKS_GLINT_BASS);
    frs_param_set(P_TP_PRE, 1);
    h = hash_run(h, 3u * 44100u);
    frs_param_set(P_TP_TRACKS, FRS_TRACKS_ALL);
    frs_param_set(P_TP_FLUTTER, 40);
    frs_param_set(P_TP_TONE, 110);
    frs_param_set(P_TP_SAT, 127);
    h = hash_run(h, 3u * 44100u);
    frs_param_set(P_TP_ON, 0);
    h = hash_run(h, 44100u);
    frs_param_set(P_TP_ON, medium);
    frs_param_set(P_TP_PRE, 0);
    frs_param_set(P_TP_WOBBLE, 127);
    frs_param_set(P_TP_NOISE, 0);
    return hash_run(h, 3u * 44100u);
}

static uint64_t tape_b(uint32_t medium)  /* 48 kHz: OPEN WATER on the tape at its defaults, a little hiss */
{
    static const uint8_t S[6] = {50, 40, 64, 74, 20, 0};
    open_on(48000.0f, 4, medium, S);
    return hash_run(0xCBF29CE484222325u, 6u * 48000u);
}

/* CUT and the iPad's STOP on a medium, PLAY after each (card #224: VINYL's stop its own; the others as they were) */
static uint64_t cut_hash(uint32_t medium)
{
    static const uint8_t S[6] = {60, 70, 64, 60, 40, 30};
    uint64_t h = 0xCBF29CE484222325u;
    open_on(44100.0f, 1, medium, S);
    h = hash_run(h, 2u * 44100u);
    frs_cut();
    h = hash_run(h, 44100u);
    frs_play();
    h = hash_run(h, 2u * 44100u);
    frs_cut_stop();
    h = hash_run(h, 22050u);
    frs_play();
    return hash_run(h, 3u * 22050u);
}

static float L[4096], R[4096];

/* VINYL's record player (card #224): renders kept whole (the noise alone: one minus another, the music's path the same in
 * both, DROPOUT 0: no draw of its own), their measures */
#define VN_N (20u * 44100u)
static float VL[VN_N], VR[VN_N], WL[6u * 96000u], WR[6u * 96000u];
static void vrender(float *l, float *r, uint32_t n)
{
    uint32_t d = 0;
    while (d < n) {
        uint32_t k = n - d < 256u ? n - d : 256u;   /* (the iPad's blocks) */
        frs_render(l + d, r + d, k);
        d += k;
    }
}
static void vinyl_new(float fs, uint32_t medium, uint32_t noise)   /* NEW SHARD on a medium, NOISE, nothing playing */
{
    uint8_t sc[FRS_SCENE_BYTES];
    frs_close();
    frs_init(fs);
    frs_scene_new(sc);
    frs_scene_take(sc);
    frs_param_set(P_TP_ON, medium);
    frs_param_set(P_TP_NOISE, noise);
}
static double lows(const float *a, const float *b, uint32_t i0, uint32_t n)   /* the power of a - b under some 70 Hz */
{
    double e = 0.0;
    float x = 0.0f;
    uint32_t i;
    for (i = i0; i < i0 + n; i++) {
        x += 0.01f * ((a[i] - b[i]) - x);
        e += (double)x * x;
    }
    return e / n;
}
static uint32_t crossings(const float *a, uint32_t i0, uint32_t n)   /* (its lows' zero crossings: its pitch) */
{
    float x = 0.0f, p = 0.0f;
    uint32_t i, c = 0;
    for (i = i0 - 400u; i < i0 + n; i++) {
        x += 0.05f * (a[i] - x);
        c += i >= i0 && (x > 0.0f) != (p > 0.0f);
        p = x;
    }
    return c;
}
static float vmost(const float *a, uint32_t i0, uint32_t n, int step)   /* the largest sample (step: its step) */
{
    float m = 0.0f;
    uint32_t i;
    for (i = i0 + 1u; i < i0 + n; i++) {
        float v = step ? fabsf(a[i] - a[i - 1u]) : fabsf(a[i]);
        m = v > m ? v : m;
    }
    return m;
}
static uint32_t onset(const float *a, uint32_t n)   /* the first sample over -60 dB */
{
    uint32_t i;
    for (i = 0; i < n && fabsf(a[i]) < 1e-3f; i++)
        ;
    return i;
}
/* frames rendered: their RMS (both sides), their peak, the largest step from a sample to the next, any not finite */
static double rms_of(uint32_t frames, float *peak, float *step, int *bad)
{
    double e = 0.0;
    float pk = 0.0f, st = 0.0f;
    static float ll, rr;
    uint32_t done = 0, i;
    while (done < frames) {
        uint32_t n = frames - done < 4096u ? frames - done : 4096u;
        frs_render(L, R, n);
        for (i = 0; i < n; i++) {
            float a = fabsf(L[i]) > fabsf(R[i]) ? fabsf(L[i]) : fabsf(R[i]);
            float d = fabsf(L[i] - ll) > fabsf(R[i] - rr) ? fabsf(L[i] - ll) : fabsf(R[i] - rr);
            if (!isfinite(L[i]) || !isfinite(R[i]))
                *bad = 1;
            e += (double)L[i] * L[i] + (double)R[i] * R[i];
            pk = a > pk ? a : pk;
            st = d > st ? d : st;
            ll = L[i];
            rr = R[i];
        }
        done += n;
    }
    if (peak)
        *peak = pk;
    if (step)
        *step = st;
    return sqrt(e / (2.0 * frames));
}

/* nothing playing (NEW SHARD, no note): the largest sample of frames (its RMS: rms) */
static float silent_peak(uint32_t frames, double *rms)
{
    float pk = 0.0f;
    int bad = 0;
    double r = rms_of(frames, &pk, 0, &bad);
    if (rms)
        *rms = r;
    return bad ? 1e9f : pk;
}

/* frames rendered: the RMS of the sides' difference against theirs (0: one side, AM's), the highs' (the left's fourth
 * difference: 24 dB an octave up to 11 kHz, the top's) against the whole's; lows (if asked): the lows' (the left
 * through six poles at 70 Hz: under 100 Hz, 200 Hz's 29 dB down, 300 Hz's 39) against the whole's */
static double mids_of;                   /* (the last call's: the mids' against the whole, the left through two poles'
                                          * high pass at 800 Hz and two poles at 2 kHz) */
static void sides_highs_lows(uint32_t frames, double *side, double *highs, double *lows)
{
    double e = 0.0, d = 0.0, h = 0.0, w = 0.0, md2 = 0.0;
    static float x1, x2, x3, x4, p1, p2, p3, p4, p5, p6, a1, a2, a3, a4;
    const float k = 1.0f - expf(-6.2831853f * 70.0f / 44100.0f), ka = 1.0f - expf(-6.2831853f * 800.0f / 44100.0f);
    const float kb = 1.0f - expf(-6.2831853f * 2000.0f / 44100.0f);
    uint32_t done = 0, i;
    while (done < frames) {
        uint32_t n = frames - done < 4096u ? frames - done : 4096u;
        frs_render(L, R, n);
        for (i = 0; i < n; i++) {
            double q = (double)L[i] - 4.0 * x1 + 6.0 * x2 - 4.0 * x3 + x4;
            e += (double)L[i] * L[i] + (double)R[i] * R[i];
            d += (double)(L[i] - R[i]) * (L[i] - R[i]);
            h += q * q;
            x4 = x3;
            x3 = x2;
            x2 = x1;
            x1 = L[i];
            p1 += (L[i] - p1) * k;
            p2 += (p1 - p2) * k;
            p3 += (p2 - p3) * k;
            p4 += (p3 - p4) * k;
            p5 += (p4 - p5) * k;
            p6 += (p5 - p6) * k;
            w += (double)p6 * p6;
            a1 += (L[i] - a1) * ka;      /* (the mids: L less its lows twice, then its highs out twice) */
            a2 += ((L[i] - a1) - a2) * ka;
            a3 += ((L[i] - a1 - a2) - a3) * kb;
            a4 += (a3 - a4) * kb;
            md2 += (double)a4 * a4;
        }
        done += n;
    }
    *side = sqrt(d / (e + 1e-30));
    *highs = sqrt(2.0 * h / (e + 1e-30));
    mids_of = sqrt(2.0 * md2 / (e + 1e-30));
    if (lows)
        *lows = sqrt(2.0 * w / (e + 1e-30));
}
static void sides_highs(uint32_t frames, double *side, double *highs) { sides_highs_lows(frames, side, highs, 0); }

/* ------------------------------------------------------------------ the takes for the engines before PHONE --- */
/* LUMIERE NOCTURNE on the medium (its start's MEDIA m; SATURATION, WOBBLE, NOISE, DROPOUT moved), a drone and a bass
 * note, MEDIA turned to TAPE at 7 s and back to m at 10 s (FST_PARAM, as a CC or the encoder: frs_param_set), 14 s */
#define TAKE_MS 14000u
static uint8_t *take_stream(uint32_t m, size_t *out)
{
    static const struct { uint32_t ms; uint8_t op, n, a[4]; } EV[] = {
        {500u, FST_KEY_DOWN, 4, {0, 48, 0, 100}},  {1500u, FST_KEY_DOWN, 4, {1, 31, 2, 100}},
        {7000u, FST_PARAM, 2, {P_TP_ON, FRS_MEDIA_TAPE, 0, 0}}, {9000u, FST_KEY_UP, 2, {1, 0, 0, 0}},
        {10000u, FST_PARAM, 2, {P_TP_ON, 0xFF, 0, 0}},   /* (0xFF: m) */
        {11000u, FST_PARAM, 2, {P_TP_NOISE, 90, 0, 0}}, {13000u, FST_KEY_UP, 2, {0, 0, 0, 0}},
    };
    static fst_start start;
    uint8_t sc[FRS_SCENE_BYTES], *b = malloc(256u * 1024u), *s;
    uint32_t last = 0, i;
    size_t j;
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_step_stays(1);
    frs_scene_factory(1, sc);
    frs_scene_take(sc);
    frs_param_set(P_TP_ON, m);
    frs_param_set(P_TP_SAT, 90);
    frs_param_set(P_TP_WOBBLE, 100);
    frs_param_set(P_TP_NOISE, 40);
    frs_param_set(P_TP_DROPOUT, 90);
    fst_capture(&start, 0);
    j = fst_jhead(&start, 44100, 1793000000, b);
    for (i = 0; i <= sizeof EV / sizeof EV[0]; i++) {
        fst_ev e;
        memset(&e, 0, sizeof e);
        if (i < sizeof EV / sizeof EV[0]) {
            e.ms = EV[i].ms;
            e.op = EV[i].op;
            e.n = EV[i].n;
            memcpy(e.a, EV[i].a, 4);
            if (e.op == FST_PARAM && e.a[1] == 0xFFu)
                e.a[1] = (uint8_t)m;
        } else {
            e.ms = TAKE_MS;
            e.op = FSTJ_END;
        }
        j += fst_jevent(&e, &last, 0, 0, b + j);
    }
    s = fst_finish(b, j, out);
    free(b);
    return s;
}

/* a take from its top to its end and 2 s more, as the radio plays it: its hash (0: not read) */
static uint64_t hear_take(uint32_t m)
{
    static fst_file f;
    static fst_air a;
    uint64_t h = 0xcbf29ce484222325ull;
    size_t n;
    uint8_t *b = take_stream(m, &n);
    uint32_t i, x;
    if (!b || fst_read(b, n, &f) || fst_air_open(&a, &f, 44100.0f)) {
        free(b);
        return 0;
    }
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
    free(b);
    return h;
}

static int make(const char *dir)
{
    static const uint32_t TK[3] = {FRS_MEDIA_PHONE, FRS_MEDIA_AM, FRS_MEDIA_TAPE};
    char path[1024];
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        size_t n;
        uint8_t *s = take_stream(TK[k], &n);
        FILE *f;
        snprintf(path, sizeof path, "%s/%s.frstream", dir, k == 0u ? "phone" : k == 1u ? "am" : "tape");
        f = s ? fopen(path, "wb") : 0;
        if (!f || fwrite(s, 1, n, f) != n)
            return printf("make: %s not written\n", path), 1;
        fclose(f);
        free(s);
    }
    frs_close();
    printf("make: 3 takes in %s (PHONE, AM, TAPE)\n", dir);
    return 0;
}

int main(int argc, char **argv)
{
    uint8_t sc[FRS_SCENE_BYTES];
    uint32_t m, k, on[2];               /* (on: PLAY's onsets after a CUT, the tape's, VINYL's) */
    char t[FRS_TEXT];

    if (argc >= 3 && !strcmp(argv[1], "make"))
        return make(argv[2]);

    /* TAPE bit for bit: the engine before the media's (70c3ad07); AM: the engine before PHONE's (49bcaa81); VINYL its own
     * since card #224 (its record player, below) */
    {
        static const uint64_t H[4][2] = {{0, 0}, {0x3cde3999de0a6691ull, 0xeddf95db4e52cde7ull}, {0, 0},
                                         {0x60ebd7759408fd4full, 0xc76049aa91d10e35ull}};
        for (m = 1; m < 4u; m++) {
            uint64_t a, b;
            if (m == FRS_MEDIA_VINYL)
                continue;
            a = tape_a(m);
            b = tape_b(m);
            printf("       %-5s: %016llx at 44.1 kHz, %016llx at 48 kHz\n", MEDIUM[m], (unsigned long long)a,
                   (unsigned long long)b);
            check_m(a == H[m][0] && b == H[m][1], m, m == FRS_MEDIA_TAPE
                    ? "at 44.1 kHz every setting moved, at 48 kHz at its defaults: as before the media, bit for bit"
                    : "the same two: as before PHONE, bit for bit");
        }
    }
    /* OFF bit for bit: the engine before VINYL's record player (history/dev 49bcaa81, card #224); CUT and STOP on OFF,
     * TAPE and AM too */
    {
        uint64_t a = tape_a(0), b = tape_b(0);
        uint64_t e = cut_hash(0), f = cut_hash(FRS_MEDIA_TAPE), g = cut_hash(FRS_MEDIA_AM);
        printf("       OFF %016llx %016llx; CUT, STOP: %016llx %016llx %016llx\n", (unsigned long long)a,
               (unsigned long long)b, (unsigned long long)e, (unsigned long long)f, (unsigned long long)g);
        check(a == 0xab3d50053bfeee2dull && b == 0x2a3ddd9a89cea2d1ull, "OFF: as before VINYL's record player, bit for bit");
        check(e == 0x4b364383874d5031ull && f == 0x63988f008b5ea7bdull && g == 0xec296a69b9d8acb2ull,
              "  CUT and STOP, PLAY after each, on OFF, TAPE and AM: as before, bit for bit");
    }

    /* the words */
    {
        const char *name;
        uint32_t max, def;
        int ok = !frs_param_info(P_TP_ON, &name, &max, &def) && !strcmp(name, "MEDIA") && max == NM - 1u && def == 0u;
        for (m = 0; m < NM; m++) {
            frs_param_text(P_TP_ON, m, t);
            ok &= !strcmp(t, MEDIUM[m]);
        }
        check(ok, "MEDIA: OFF, TAPE, VINYL, AM, PHONE (OFF its default)");
        frs_close();
        frs_init(44100.0f);
        frs_param_set(P_TP_ON, 9);
        check(frs_param_get(P_TP_ON) == FRS_MEDIA_PHONE, "  a value past PHONE: PHONE (as every setting past its range)");
    }

    /* the takes `make` writes (media_old.c hears them on the engines before PHONE): each read here and heard as made,
     * PHONE not AM, not TAPE */
    {
        uint64_t hp = hear_take(FRS_MEDIA_PHONE), ha = hear_take(FRS_MEDIA_AM), ht = hear_take(FRS_MEDIA_TAPE);
        printf("       the takes: PHONE %016llx, AM %016llx, TAPE %016llx\n", (unsigned long long)hp,
               (unsigned long long)ha, (unsigned long long)ht);
        check(hp && ha && ht && hp != ha && hp != ht && ha != ht, "a take on PHONE read and heard as PHONE (not AM, not "
              "TAPE)");
    }

    /* VINYL, AM and PHONE on a factory scene (LUMIERE NOCTURNE: slow, bright, wide), at their gentlest and their
     * hardest: finite, under full scale, as loud as the tape (its level kept) */
    {
        static const uint8_t SOFT[6] = {20, 20, 64, 74, 10, 0}, HARD[6] = {127, 127, 20, 0, 127, 127};
        static const uint8_t HARD2[6] = {127, 127, 127, 127, 127, 127};
        double dry, lv[NM][3];
        float pk[NM][3];
        int bad = 0;
        open_on(44100.0f, 1, 0, SOFT);
        rms_of(2u * 44100u, 0, 0, &bad);
        dry = rms_of(8u * 44100u, 0, 0, &bad);
        for (m = 1; m < NM; m++)
            for (k = 0; k < 3u; k++) {
                open_on(44100.0f, 1, m, k == 0u ? SOFT : k == 1u ? HARD : HARD2);
                rms_of(2u * 44100u, 0, 0, &bad);
                lv[m][k] = rms_of(8u * 44100u, &pk[m][k], 0, &bad);
            }
        for (m = 1; m < NM; m++)
            printf("       %-5s: RMS %.4f, %.4f, %.4f (dry %.4f); peaks %.3f, %.3f, %.3f\n", MEDIUM[m], lv[m][0], lv[m][1],
                   lv[m][2], dry, pk[m][0], pk[m][1], pk[m][2]);
        check(!bad, "every sample finite");
        for (m = 2; m < NM; m++) {
            int ok = 1;
            for (k = 0; k < 3u; k++)
                ok &= pk[m][k] < 1.0f && lv[m][k] > 0.5 * dry && lv[m][k] < 2.0 * dry;
            check_m(ok, m, "gentle and hard, under full scale, within 6 dB of the dry scene");
        }
    }

    /* each its own: AM and PHONE one side, VINYL as wide as the tape (the same scene, their gentlest); PHONE an old
     * handset's (Styves, card #218): its lows gone (under 100 Hz a tenth of the dry scene's and less), its mids pushed
     * (800 Hz .. 2 kHz twice the dry scene's share and more), its highs muffled (duller than AM's); on silence, NOISE at
     * the top (TONE at its default), the tape's hiss bright, AM's static and PHONE's hiss in their band (their tops some
     * 4 and 2.4 kHz: their fourth difference a tenth of the hiss's and less), higher at TONE's top */
    {
        static const uint8_t SOFT[6] = {20, 20, 64, 74, 0, 0}, NZ[6] = {50, 40, 64, 74, 127, 0};
        static const uint32_t TP[6] = {P_TP_SAT, P_TP_WOBBLE, P_TP_FLUTTER, P_TP_TONE, P_TP_NOISE, P_TP_DROPOUT};
        double side[NM], highs[NM], low[NM], hiss[NM], top[NM], mid[NM], dlow;
        open_on(44100.0f, 1, 0, SOFT);
        sides_highs_lows(2u * 44100u, &side[0], &highs[0], &dlow);
        sides_highs_lows(6u * 44100u, &side[0], &highs[0], &dlow);
        mid[0] = mids_of;
        for (m = 1; m < NM; m++) {
            open_on(44100.0f, 1, m, SOFT);
            sides_highs_lows(2u * 44100u, &side[m], &highs[m], &low[m]);
            sides_highs_lows(6u * 44100u, &side[m], &highs[m], &low[m]);
            mid[m] = mids_of;
            frs_close();
            frs_init(44100.0f);
            frs_scene_new(sc);
            frs_scene_take(sc);
            frs_param_set(P_TP_ON, m);
            for (k = 0; k < 6u; k++)
                frs_param_set(TP[k], NZ[k]);
            sides_highs(44100u, &top[m], &hiss[m]);
            sides_highs(3u * 44100u, &top[m], &hiss[m]);
            frs_param_set(P_TP_TONE, 127);
            sides_highs(44100u, &top[m], &top[m]);
            sides_highs(3u * 44100u, &side[0], &top[m]);
        }
        printf("       the sides' difference %.3g, %.3g, %.3g, %.3g (TAPE, VINYL, AM, PHONE); NOISE's top %.3g, %.3g, "
               "%.3g, %.3g, AM's and PHONE's at TONE's top %.3g, %.3g; the lows %.3g on PHONE, %.3g dry\n", side[1],
               side[2], side[3], side[4], hiss[1], hiss[2], hiss[3], hiss[4], top[3], top[4], low[4], dlow);
        for (m = 3; m < NM; m++) {
            check_m(side[m] < 1e-4, m, "one side");
            check_m(hiss[m] < 0.1 * hiss[1] && top[m] > 2.0 * hiss[m], m,
                    "  its noise in its band (its top far under the tape's hiss's), higher at TONE's top");
        }
        check_m(side[2] > 0.5 * side[1] && side[2] < 2.0 * side[1], FRS_MEDIA_VINYL, "as wide as the tape");
        check_m(low[4] < 0.1 * dlow, FRS_MEDIA_PHONE, "  its lows gone (under 100 Hz a tenth of the dry scene's)");
        printf("       the mids' share %.3g on PHONE, %.3g dry; the highs' %.3g on PHONE, %.3g on AM\n", mid[4], mid[0],
               highs[4], highs[3]);
        check_m(mid[4] > 2.0 * mid[0] && highs[4] < highs[3], FRS_MEDIA_PHONE,
                "  its mids pushed (twice the dry scene's share), its highs duller than AM's");
    }

    /* silent in, silent out: NEW SHARD, nothing playing, each medium at its hardest but NOISE and DROPOUT 0: nothing at
     * all; NOISE's own noise (the tape's hiss, AM's static, PHONE's hiss and hum) on that silence, gone again at 0.
     * VINYL's needle up on it (nothing has reached the record): nothing at any NOISE (its noise on silence below, after
     * music) */
    for (m = 1; m < NM; m++) {
        static const uint8_t Q[6] = {127, 127, 64, 127, 0, 0};
        float p0, p1, p2, p3;
        double r1;
        frs_close();
        frs_init(44100.0f);
        frs_scene_new(sc);
        frs_scene_take(sc);
        frs_param_set(P_TP_ON, m);
        frs_param_set(P_TP_SAT, Q[0]);
        frs_param_set(P_TP_WOBBLE, Q[1]);
        frs_param_set(P_TP_TONE, Q[3]);
        frs_param_set(P_TP_DROPOUT, 127);
        p0 = silent_peak(5u * 44100u, 0);
        frs_param_set(P_TP_NOISE, 127);
        silent_peak(44100u, 0);
        p1 = silent_peak(3u * 44100u, &r1);
        frs_param_set(P_TP_NOISE, 20);
        silent_peak(44100u, 0);
        p2 = silent_peak(3u * 44100u, 0);
        frs_param_set(P_TP_NOISE, 0);
        silent_peak(2u * 44100u, 0);
        p3 = silent_peak(3u * 44100u, 0);
        printf("       %-5s on silence: peak %.3g at NOISE 0, %.3g at 127 (%.1f its RMS), %.3g at 20, %.3g at 0 again\n",
               MEDIUM[m], p0, p1, p1 / (r1 + 1e-30), p2, p3);
        if (m == FRS_MEDIA_VINYL)
            check_m(p0 == 0.0f && p1 == 0.0f && p2 == 0.0f && p3 == 0.0f, m,
                    "silent on silence at every NOISE (DROPOUT, SATURATION, WOBBLE at the top): the needle waits for music");
        else
            check_m(p0 == 0.0f && p1 > 1e-3f && p1 < 0.25f && p2 > 0.0f && p2 < 0.25f * p1 && p3 < 1e-6f, m,
                    "silent at NOISE 0 (DROPOUT, SATURATION, WOBBLE at the top), its noise at NOISE, under it at 20, "
                    "silent again");
    }

    /* PHONE's hum: on silence, NOISE at the top, the hum's harmonics (120 Hz's, in the band: 240 .. 600 Hz) heard over
     * the hiss's floor there (each one's level by a correlation over 3 s against the hiss's at 3 neighbours between) */
    {
        static const uint8_t NZ[6] = {50, 0, 64, 74, 127, 0};
        static const uint32_t TP[6] = {P_TP_SAT, P_TP_WOBBLE, P_TP_FLUTTER, P_TP_TONE, P_TP_NOISE, P_TP_DROPOUT};
        double hc[5][2], fl[5][2], hum = 0.0, floor_ = 0.0;
        uint32_t done = 0, i, f;
        frs_close();
        frs_init(44100.0f);
        frs_scene_new(sc);
        frs_scene_take(sc);
        frs_param_set(P_TP_ON, FRS_MEDIA_PHONE);
        for (k = 0; k < 6u; k++)
            frs_param_set(TP[k], NZ[k]);
        silent_peak(44100u, 0);
        memset(hc, 0, sizeof hc);
        memset(fl, 0, sizeof fl);
        while (done < 3u * 44100u) {
            frs_render(L, R, 4096u);
            for (i = 0; i < 4096u; i++)
                for (f = 0; f < 5u; f++) {
                    double ph = 6.283185307 * (double)(done + i) / 44100.0, hz = 240.0 + 120.0 * f;
                    hc[f][0] += L[i] * cos(ph * hz);
                    hc[f][1] += L[i] * sin(ph * hz);
                    fl[f][0] += L[i] * cos(ph * (hz + 60.0 + 7.0));
                    fl[f][1] += L[i] * sin(ph * (hz + 60.0 + 7.0));
                }
            done += 4096u;
        }
        for (f = 0; f < 5u; f++) {
            hum += hc[f][0] * hc[f][0] + hc[f][1] * hc[f][1];
            floor_ += fl[f][0] * fl[f][0] + fl[f][1] * fl[f][1];
        }
        printf("       PHONE's hum: its harmonics %.3g against the hiss between them %.3g\n", sqrt(hum), sqrt(floor_));
        check_m(hum > 4.0 * floor_, FRS_MEDIA_PHONE, "  a mains hum under its hiss (60 Hz's harmonics in the band)");
    }

    /* DROPOUT: the level dips now and then, on each medium (the same drone twice from the start, the second with it at
     * the top); PHONE's a cut (its level down to a few percent a moment, whole 50 ms windows of it) */
    for (m = 1; m < NM; m++) {
        double lv2[2];
        int bad = 0;
        uint32_t cut = 0;
        for (k = 0; k < 2u; k++) {
            uint32_t w;
            frs_close();
            frs_init(48000.0f);
            frs_scene_new(sc);
            frs_scene_take(sc);
            frs_param_set(P_TP_ON, m);
            frs_param_set(P_TP_SAT, 0);
            frs_param_set(P_TP_WOBBLE, 0);
            frs_param_set(P_TP_DROPOUT, k ? 127 : 0);
            frs_latch(0, 48, FRS_CHORD_HARMONY);
            rms_of(96000, 0, 0, &bad);
            lv2[k] = 0.0;
            for (w = 0; w < 400u; w++) {   /* (20 s, in windows of 50 ms) */
                double e = rms_of(2400u, 0, 0, &bad);
                lv2[k] += e * e / 400.0;
                if (k)
                    cut += e < 0.06 * lv2[0];
            }
            lv2[k] = sqrt(lv2[k]);
        }
        printf("       %-5s: the drone %.4f, %.4f with DROPOUT at the top (%u windows of 50 ms near silent)\n",
               MEDIUM[m], lv2[0], lv2[1], (unsigned)cut);
        check_m(lv2[0] > 0.0 && lv2[1] < 0.97 * lv2[0], m, "DROPOUT dips the level now and then");
        if (m == FRS_MEDIA_PHONE)
            check_m(cut >= 5u, m, "  its line cut a moment now and then (near silent)");
    }

    /* a medium changed while a drone plays: out through the tape's fade, the new one in (no step), the view's the one
     * sounding; every sample's step from the last no larger than the drone's own steps on the media (the largest of
     * theirs, each settled), through the changes */
    {
        float fx[FRS_LIVE_FX], st0 = 0.0f, st1, pk, own[NM];
        int bad = 0, seen = 1;
        frs_close();
        frs_init(44100.0f);
        frs_scene_new(sc);
        frs_scene_take(sc);
        frs_latch(0, 48, FRS_CHORD_HARMONY);
        frs_latch(2, 60, FRS_CHORD_HARMONY);
        for (m = NM - 1u; m >= 1u; m--) {   /* (each settled: PHONE .. TAPE, the last TAPE) */
            frs_param_set(P_TP_ON, m);
            rms_of(3u * 44100u, 0, 0, &bad);
            rms_of(2u * 44100u, &pk, &own[m], &bad);
            st0 = own[m] > st0 ? own[m] : st0;
        }
        frs_live_fx(fx);
        seen &= fx[37] == (float)FRS_MEDIA_TAPE;
        st1 = 0.0f;
        for (m = 2; m < 8u; m++) {       /* (VINYL, AM, PHONE, TAPE, VINYL, AM) */
            float st;
            uint32_t to = (m - 1u) % 4u + 1u;
            frs_param_set(P_TP_ON, to);
            rms_of(44100u, 0, &st, &bad);
            st1 = st > st1 ? st : st1;
            frs_live_fx(fx);
            seen &= fx[37] == (float)to && fx[0] > 0.99f;
        }
        frs_param_set(P_TP_ON, FRS_MEDIA_PHONE);   /* (and PHONE off at once) */
        rms_of(44100u, 0, 0, &bad);
        frs_param_set(P_TP_ON, 0);
        rms_of(44100u, 0, 0, &bad);
        frs_live_fx(fx);
        seen &= fx[37] == 0.0f && fx[0] == 0.0f;
        printf("       a drone's largest step %.4f, %.4f, %.4f, %.4f settled (TAPE, VINYL, AM, PHONE); %.4f through VINYL, "
               "AM, PHONE, TAPE, VINYL, AM\n", own[1], own[2], own[3], own[4], st1);
        check(!bad && st1 < 1.5f * st0, "a medium changed while it plays: through the fade, no click");
        check(seen, "  the effects' view: the medium sounding (37), its mix in (0); none once off");
    }


    /* VINYL's needle (card #224): music reaching the record drops it, a thump under some 70 Hz (NOISE 64's render
     * minus NOISE 0's, its first 100 ms against a second on); none on a join (the brain skipped to 3 s, the needle
     * already on it); VINYL chosen while music plays (from the tape): its drop as its mix comes in */
    {
        double d0, d1, j0, j1, c0, c1;
        uint32_t k;
        for (k = 0; k < 2u; k++) {       /* (NOISE 64, then 0, the same drone) */
            vinyl_new(44100.0f, FRS_MEDIA_VINYL, k ? 0 : 64);
            vrender(k ? WL : VL, k ? WR : VR, 22050u);
            frs_latch(0, 48, FRS_CHORD_HARMONY);
            vrender((k ? WL : VL) + 22050u, (k ? WR : VR) + 22050u, 2u * 44100u);
        }
        d0 = lows(VL, WL, 22050u, 4410u);
        d1 = lows(VL, WL, 22050u + 44100u, 4410u);
        for (k = 0; k < 2u; k++) {
            vinyl_new(44100.0f, FRS_MEDIA_VINYL, k ? 0 : 64);
            frs_latch(0, 48, FRS_CHORD_HARMONY);
            frs_skip_to(3000);
            vrender(k ? WL : VL, k ? WR : VR, 2u * 44100u);
        }
        j0 = lows(VL, WL, 0, 6615u);
        j1 = lows(VL, WL, 44100u, 6615u);
        for (k = 0; k < 2u; k++) {
            vinyl_new(44100.0f, FRS_MEDIA_TAPE, k ? 0 : 64);
            frs_latch(0, 48, FRS_CHORD_HARMONY);
            vrender(k ? WL : VL, k ? WR : VR, 44100u);
            frs_param_set(P_TP_ON, FRS_MEDIA_VINYL);
            vrender((k ? WL : VL) + 44100u, (k ? WR : VR) + 44100u, 2u * 44100u);
        }
        c0 = lows(VL, WL, 44100u, 6615u);
        c1 = lows(VL, WL, 2u * 44100u, 6615u);
        printf("       VINYL's needle, its lows: %.3g then %.3g as music comes; %.3g then %.3g on a join; %.3g then %.3g "
               "chosen while it plays\n", d0, d1, j0, j1, c0, c1);
        check_m(d0 > 20.0 * d1 && d0 > 1e-6, FRS_MEDIA_VINYL, "the needle drops as music reaches the record: its thump");
        check_m(j0 < 4.0 * j1 + 1e-9, FRS_MEDIA_VINYL, "  none on a join (the needle on it already)");
        check_m(c0 > 20.0 * c1 && c0 > 1e-6, FRS_MEDIA_VINYL, "  chosen while music plays: as its mix comes in");
    }

    /* its noise, the needle down: NOISE 127's minus NOISE 0's over a drone, ticks and pops (its peak eight times its
     * RMS and more), a hush under; none at NOISE 0 again (the rumble gone too); its marks ticking again each turn (NOISE
     * 40: the noise's ticks over a level, their recurrence at the turn's 1.8 s against every lag's from 0.6 to 4.5 s) */
    {
        float x = 0.0f, nl[3];
        double e = 0.0, best = 0.0, mean = 0.0;
        static float env[VN_N / 132u];
        uint32_t i, k, lag, nb = VN_N / 132u;
        for (k = 0; k < 2u; k++) {
            vinyl_new(44100.0f, FRS_MEDIA_VINYL, 0);
            frs_latch(0, 48, FRS_CHORD_HARMONY);
            vrender(k ? WL : VL, k ? WR : VR, 44100u);
            frs_param_set(P_TP_NOISE, k ? 0 : 127);
            vrender(k ? WL : VL, k ? WR : VR, 3u * 44100u);
            frs_param_set(P_TP_NOISE, 0);
            vrender((k ? WL : VL) + 3u * 44100u, (k ? WR : VR) + 3u * 44100u, 2u * 44100u);
        }
        for (i = 44100u; i < 3u * 44100u; i++) {
            float d = fabsf(VL[i] - WL[i]) > fabsf(VR[i] - WR[i]) ? fabsf(VL[i] - WL[i]) : fabsf(VR[i] - WR[i]);
            x = d > x ? d : x;
            e += (double)(VL[i] - WL[i]) * (VL[i] - WL[i]);
        }
        nl[0] = x;
        nl[1] = (float)sqrt(e / (2.0 * 44100.0));
        nl[2] = 0.0f;
        for (i = 4u * 44100u; i < 5u * 44100u; i++)
            nl[2] = fabsf(VL[i] - WL[i]) > nl[2] ? fabsf(VL[i] - WL[i]) : nl[2];
        vinyl_new(44100.0f, FRS_MEDIA_VINYL, 0);
        frs_latch(0, 48, FRS_CHORD_HARMONY);
        vrender(VL, VR, VN_N);
        vinyl_new(44100.0f, FRS_MEDIA_VINYL, 40);
        frs_latch(0, 48, FRS_CHORD_HARMONY);
        memset(env, 0, sizeof env);
        for (i = 0; i < VN_N; i += 4096u) {
            uint32_t n = VN_N - i < 4096u ? VN_N - i : 4096u, j;
            frs_render(L, R, n);
            for (j = 0; j < n; j++) {
                float d = fabsf(L[j] - VL[i + j]) + fabsf(R[j] - VR[i + j]);
                if (i + j >= 44100u && d > 0.016f)   /* (ticks over a level, in 3 ms bins) */
                    env[(i + j) / 132u] = 1.0f;
            }
        }
        for (lag = 200; lag < 1500u; lag++) {
            double c = 0.0;
            for (i = 0; i + lag < nb; i++)
                c += env[i] * env[i + lag];
            c /= (double)(nb - lag);
            mean += c / 1300.0;
            if (lag >= 598u && lag <= 604u && c > best)
                best = c;
        }
        printf("       VINYL's noise over a drone: peak %.3g (%.1f its RMS), %.3g at NOISE 0 again; its marks: %.2f times "
               "the mean at the turn\n", nl[0], nl[0] / (nl[1] + 1e-30), nl[2], best / (mean + 1e-30));
        check_m(nl[0] > 1e-3f && nl[0] < 0.25f && nl[0] > 8.0f * nl[1] && nl[2] < 1e-6f, FRS_MEDIA_VINYL,
                "its noise, the needle down: ticks and pops over a hush; none at NOISE 0 again");
        check_m(best > 2.5 * mean, FRS_MEDIA_VINYL, "  its marks ticking again at each turn (33 1/3 a minute)");
    }

    /* its lows one side, as a record is cut: LUMIERE NOCTURNE's sides' difference under some 56 Hz over their sum's,
     * VINYL against the tape */
    {
        double lo[3];
        uint32_t i;
        for (m = 1; m < 3u; m++) {
            float s1 = 0.0f, s2 = 0.0f;
            double es = 0.0, em = 0.0;
            open_on(44100.0f, 1, m, (const uint8_t[6]){50, 40, 64, 74, 0, 0});
            vrender(VL, VR, 4u * 44100u);
            for (i = 44100u; i < 4u * 44100u; i++) {
                s1 += 0.008f * ((VL[i] - VR[i]) - s1);
                s2 += 0.008f * ((VL[i] + VR[i]) - s2);
                es += (double)s1 * s1;
                em += (double)s2 * s2;
            }
            lo[m] = es / (em + 1e-30);
        }
        printf("       the lows' sides over their sum: %.3g TAPE, %.3g VINYL\n", lo[1], lo[2]);
        check_m(lo[2] < 0.6 * lo[1], FRS_MEDIA_VINYL, "its lows one side (the cut's), the tape's as wide as they were");
    }

    /* CUT on VINYL, a drone (NOISE 0, its stop on every NOISE): the record slowing to a stop, still sounding 0.3 s on, its
     * pitch down (its lows' crossings), every sample's step no larger than the drone's own, silent (every sample 0) from
     * VN_STOP's 0.75 s and a block or two; after it the needle up (NOISE 127: still nothing); PLAY again as soon as after
     * the tape's CUT (its first sample over -60 dB within 30 ms of the tape's, the record reading behind as the tape
     * does) */
    {
        uint32_t c0, c1, k;
        float sound, after, quiet, st0, st1;
        for (k = 0; k < 2u; k++) {       /* (the tape's CUT first: PLAY's onset after it, for VINYL's) */
            vinyl_new(44100.0f, k ? FRS_MEDIA_VINYL : FRS_MEDIA_TAPE, 0);
            frs_latch(0, 48, FRS_CHORD_HARMONY);
            vrender(VL, VR, 44100u);
            frs_cut();
            vrender(VL + 44100u, VR + 44100u, 44100u);
            frs_param_set(P_TP_NOISE, 127);
            vrender(VL + 2u * 44100u, VR + 2u * 44100u, 44100u);
            frs_param_set(P_TP_NOISE, 0);   /* (the music's onset, not the tape's hiss) */
            vrender(VL + 3u * 44100u, VR + 3u * 44100u, 4410u);
            frs_play();
            vrender(VL + 3u * 44100u, VR + 3u * 44100u, 44100u);
            on[k] = onset(VL + 3u * 44100u, 44100u);
        }
        sound = vmost(VL, 44100u + 13230u, 2205u, 0);
        c0 = crossings(VL, 44100u + 2205u, 6615u);
        c1 = crossings(VL, 44100u + 19845u, 6615u);
        after = vmost(VL, 44100u + 35280u, 44100u - 35280u, 0);
        quiet = vmost(VL, 2u * 44100u, 44100u, 0);
        st0 = vmost(VL, 22050u, 22050u, 1);
        st1 = vmost(VL, 44100u, 44100u, 1);
        printf("       VINYL's stop: %.3g 0.3 s on, its lows' crossings %u then %u, %.3g from 0.8 s, %.3g after with NOISE "
               "127; steps %.4f (the drone's %.4f); PLAY again sounding at %u (the tape's %u)\n", sound, c0, c1, after, quiet,
               st1, st0, on[1], on[0]);
        check_m(sound > 0.01f && c1 < 0.6 * c0 && after == 0.0f && st1 < 1.5f * st0, FRS_MEDIA_VINYL,
                "CUT: the record slowing to a stop (its pitch down), silent within VN_STOP, no click");
        check_m(quiet == 0.0f, FRS_MEDIA_VINYL, "  the needle up after it (nothing at NOISE 127)");
        check_m(on[1] < on[0] + 1323u, FRS_MEDIA_VINYL, "  PLAY after it as soon as after the tape's CUT");
    }

    /* a note played in the stop (PLAY again, a quarter of a second in; NOISE 0: the music's onset): never held back (it
     * sounds as soon as PLAY after the tape's CUT, above: the loop filling from nothing, the drone's attack), the old
     * out within its block, no click */
    {
        uint32_t at;
        float st0, st1;
        vinyl_new(44100.0f, FRS_MEDIA_VINYL, 0);
        frs_latch(0, 48, FRS_CHORD_HARMONY);
        vrender(VL, VR, 44100u);
        frs_cut();
        vrender(VL + 44100u, VR + 44100u, 11025u);
        frs_latch(0, 55, FRS_CHORD_HARMONY);
        vrender(VL + 55125u, VR + 55125u, 44100u);
        at = onset(VL + 55125u + 512u, 44100u - 512u) + 512u;   /* (past the old's last block) */
        st0 = vmost(VL, 22050u, 22050u, 1);
        st1 = vmost(VL, 44100u, 44100u, 1);
        printf("       a note played in VINYL's stop: sounding at %u (PLAY after the tape's CUT: %u); steps %.4f (the "
               "drone's %.4f)\n", at, on[0], st1, st0);
        check_m(at < on[0] + 1323u && st1 < 1.5f * st0, FRS_MEDIA_VINYL,
                "  a note played in the stop: never held back, the old gone, no click");
    }

    /* at 48 and 96 kHz: the needle, the stop (still sounding 0.3 s on, silent from 0.8 s), the needle up after it */
    {
        static const float FS[2] = {48000.0f, 96000.0f};
        uint32_t k;
        for (k = 0; k < 2u; k++) {
            uint32_t s1 = (uint32_t)FS[k], i;
            float sound, after, quiet, most = 0.0f;
            int fin = 1;
            vinyl_new(FS[k], FRS_MEDIA_VINYL, 64);
            frs_latch(0, 48, FRS_CHORD_HARMONY);
            vrender(WL, WR, s1);
            frs_cut();
            vrender(WL + s1, WR + s1, s1);
            frs_param_set(P_TP_NOISE, 127);
            vrender(WL + 2u * s1, WR + 2u * s1, s1);
            for (i = 0; i < 3u * s1; i++) {
                fin &= isfinite(WL[i]) && isfinite(WR[i]);
                most = fabsf(WL[i]) > most ? fabsf(WL[i]) : most;
            }
            sound = vmost(WL, s1 + s1 * 3u / 10u, s1 / 20u, 0);
            after = vmost(WL, s1 + s1 * 8u / 10u, s1 * 2u / 10u, 0);
            quiet = vmost(WL, 2u * s1, s1, 0);
            printf("       VINYL at %.0f kHz: %.3g 0.3 s into the stop, %.3g from 0.8 s, %.3g after; its peak %.3g\n",
                   FS[k] / 1000.0f, sound, after, quiet, most);
            check_m(fin && most < 1.0f && sound > 0.01f && after == 0.0f && quiet == 0.0f, FRS_MEDIA_VINYL,
                    k ? "  at 96 kHz: the same" : "  at 48 kHz: the needle, the stop, the needle up after it");
        }
    }

    frs_close();
    printf("media: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
