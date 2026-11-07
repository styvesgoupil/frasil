/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The same sound at another rate, measured: at 44.1, 48 and 96 kHz, a bass's pitch (SINE, A2: 110 Hz), GLINT's echo's
 * time (a quarter at 90 BPM: 667 ms), CLOUD's RT60 (DECAY at its middle: 4.9 s); 2.5's LOFI TAPE's delay (its head
 * at rest: 9.5 ms) and flutter (5 Hz; 7.3 until TAPE SPEED), MASTER FILTER's 3 dB point, MASTER EQ's MID FREQ. Each the same at 48 kHz
 * and at 96 kHz (the lengths' upper sizes) as at 44.1 kHz.
 *   engine/tests/run.sh, or: rate */
#include <stdio.h>
#include <math.h>
#include "../frasil_engine.c"

#define SECS 8
static float L[96000 * SECS], R[96000 * SECS];
static int fails;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "  ok" : "FAIL", what);
    fails += !ok;
}

static uint32_t open_at(float fs)
{
    frs_close();
    frs_init(fs);
    return (uint32_t)fs;
}

/* the bass's pitch: SINE on A2, alone, its last second's rising zero crossings (interpolated) */
static double pitch(float fs)
{
    uint32_t r = open_at(fs), i, n = 0;
    double first = -1.0, last = 0.0;
    param_set(P_BS_MODEL, 0);
    param_set(P_BS_FILTER, 127);
    param_set(P_BS_DRIVE, 0);
    frs_latch(2, 45, FRS_CHORD_HARMONY);
    frs_render(L, R, 3 * r);
    for (i = 2 * r + 1; i < 3 * r; i++)
        if (L[i - 1] < 0.0f && L[i] >= 0.0f) {
            double t = (double)(i - 1) + (double)L[i - 1] / ((double)L[i - 1] - (double)L[i]);
            if (first < 0.0)
                first = t;
            last = t;
            n++;
        }
    return n > 1 ? (double)(n - 1) * (double)r / (last - first) : 0.0;
}

/* a 5 ms window's level at sample i */
static double level(const float *x, uint32_t i, uint32_t r)
{
    uint32_t k, w = r / 200u;
    double e = 0.0;
    for (k = 0; k < w; k++)
        e += (double)x[i + k] * x[i + k];
    return sqrt(e / w);
}

/* GLINT's echo: a glint struck, the time from its onset to its echo's (each where its level first passes half its
 * peak) */
static double echo(float fs)
{
    uint32_t r = open_at(fs), i, a = 0, b = 0, w = r / 200u;
    double pk = 0.0, pk2 = 0.0;
    param_set(P_SQ_TYPE, 0);             /* (NEW SHARD's GLINT on ARP: off, a glint struck as it is held) */
    param_set(P_GL_PRESET, 8);           /* (SINE) */
    param_set(P_GL_SPACE, 0);
    param_set(P_GD_TIME, 8);             /* (1/4, mono) */
    param_set(P_GD_FB, 0);
    param_set(P_GD_MIX, 127);
    param_set(P_GL_DECAY, 0);
    param_set(P_GL_RELEASE, 0);
    param_set(P_TEMPO, 60);              /* (90 BPM) */
    frs_render(L, R, r / 2u);
    frs_hold(1, 84, 1, FRS_CHORD_HARMONY);                  /* (L: from the strike on: the glint, then its echo) */
    frs_render(L, R, r / 50u);
    frs_hold(1, 84, 0, FRS_CHORD_HARMONY);
    frs_render(L + r / 50u, R + r / 50u, 2u * r);
    for (i = 0; i + w < r / 3u; i += w / 4u)
        pk = level(L, i, r) > pk ? level(L, i, r) : pk;
    for (i = 0; i + w < r / 3u && !a; i += w / 8u)
        a = level(L, i, r) >= 0.5 * pk ? i : 0;
    for (i = r / 2u; i + w < 3u * r / 2u; i += w / 4u)
        pk2 = level(L, i, r) > pk2 ? level(L, i, r) : pk2;
    for (i = r / 2u; i + w < 3u * r / 2u && !b; i += w / 8u)
        b = level(L, i, r) >= 0.5 * pk2 ? i : 0;
    return (double)(b - a) * 1000.0 / (double)r;
}

/* CLOUD's RT60: a chord into it, let go; the tail's fall, dB a second, from 0.5 s to 3.5 s after */
static double rt60(float fs)
{
    uint32_t r = open_at(fs), i;
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0, n = 0.0;
    param_set(P_ESPACE, 127);
    param_set(P_DUREE, 64);
    param_set(P_RELACHE, 0);
    param_set(P_ATTAQUE, 0);
    param_set(P_MODULATION, 0);
    param_set(P_SHIMMER, 0);
    param_set(P_TEINTE, 127);            /* (no damping: one RT60 for every band) */
    frs_latch(0, 48, FRS_CHORD_HARMONY);
    frs_render(L, R, 2u * r);
    frs_clear(0);
    frs_render(L, R, 4u * r);
    for (i = r / 2u; i + r / 200u < 7u * r / 2u; i += r / 100u) {
        double x = (double)i / r, y = 20.0 * log10(level(L, i, r) + 1e-12);
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
        n++;
    }
    return -60.0 / ((n * sxy - sx * sy) / (n * sxx - sx * sx));
}

/* LOFI TAPE's delay: a glint struck with the tape on every track (WOBBLE at 0: its head at rest, TP_D0), its onset
 * against the same without it (ms) */
static double onset(float fs, int tape)
{
    uint32_t r = open_at(fs), i, a = 0;
    double pk = 0.0;
    param_set(P_TP_ON, tape);
    param_set(P_TP_SAT, 0);
    param_set(P_TP_WOBBLE, 0);
    param_set(P_TP_TRACKS, FRS_TRACKS_ALL);
    param_set(P_GD_MIX, 0);
    param_set(P_GD_FB, 0);
    param_set(P_GL_SPACE, 0);
    param_set(P_ESPACE, 0);
    frs_render(L, R, r);                 /* (the tape in: its loop filled, its mix up) */
    frs_hold(1, 84, 1, FRS_CHORD_HARMONY);
    frs_render(L, R, r / 2u);
    frs_hold(1, 84, 0, FRS_CHORD_HARMONY);
    for (i = 0; i < r / 2u; i++)
        pk = fabs(L[i]) > pk ? fabs(L[i]) : pk;
    for (i = 0; i < r / 2u && !a; i++)
        if (fabs(L[i]) > 0.1 * pk)
            a = i;
    return (double)a * 1000.0 / (double)r;
}

/* LOFI TAPE's flutter: its phase's turns over 4 s (5 a second, at TAPE SPEED's pace: speed its setting) */
static double flutter(float fs, uint32_t speed)
{
    uint32_t r = open_at(fs), k, turns = 0;
    float fx[FRS_LIVE_FX], was = 0.0f;
    param_set(P_TP_ON, 1);
    param_set(P_TP_FLUTTER, speed);
    param_set(P_TP_WOBBLE, 127);
    frs_latch(0, 48, FRS_CHORD_HARMONY);
    frs_render(L, R, r);
    for (k = 0; k < 4u * r / FRS_BLOCK; k++) {
        frs_render(L, R, FRS_BLOCK);
        frs_live_fx(fx);
        turns += fx[4] < was;
        was = fx[4];
    }
    return (double)turns / 4.0;
}

/* MASTER FILTER (LOW PASS, RESONANCE 0) and MASTER EQ (MID at +12 dB): where the filter is 3 dB down and the bell's
 * top, as the engine has them now (frs_live_fx, frs_fx_response at the rate), Hz */
static void fx_points(float fs, double *f3, double *bell)
{
    float fx[FRS_LIVE_FX], best = -100.0f, hz;
    open_at(fs);
    param_set(P_FL_TYPE, 1);
    param_set(P_FL_CUT, 80);
    param_set(P_FL_RESO, 0);
    param_set(P_EQ_PRESET, 1);
    param_set(P_EQ_MID, 48);
    param_set(P_EQ_MIDF, 64);
    frs_latch(0, 48, FRS_CHORD_HARMONY);
    frs_render(L, R, (uint32_t)fs);      /* (in, and glided to) */
    frs_live_fx(fx);
    *f3 = 0.0;
    for (hz = 20.0f; hz < 0.45f * fs; hz *= 1.002f)
        if (*f3 == 0.0 && frs_fx_response(fx, 0, hz, fs) < -3.0103f)
            *f3 = hz;
    *bell = 0.0;
    for (hz = 20.0f; hz < 0.45f * fs; hz *= 1.002f) {
        float d = frs_fx_response(fx, 1, hz, fs);
        if (d > best) {
            best = d;
            *bell = hz;
        }
    }
}

int main(void)
{
    static const float AT[2] = {48000.0f, 96000.0f};
    double p1 = pitch(44100.0f), e1 = echo(44100.0f), t1 = rt60(44100.0f), d1 = onset(44100.0f, 1) - onset(44100.0f, 0);
    double w1 = flutter(44100.0f, 64), wh = flutter(44100.0f, 14), c1, b1;
    char s[200];
    int k;
    fx_points(44100.0f, &c1, &b1);
    for (k = 0; k < 2; k++) {
        float fs = AT[k];
        double p2 = pitch(fs), e2 = echo(fs), t2 = rt60(fs), d2 = onset(fs, 1) - onset(fs, 0), w2 = flutter(fs, 64), c2, b2;
        fx_points(fs, &c2, &b2);
        snprintf(s, sizeof s, "the bass's A2: %.3f Hz at 44.1 kHz, %.3f Hz at %.0f kHz", p1, p2, fs / 1000.0f);
        check(fabs(p1 - 110.0) < 0.05 && fabs(p2 - 110.0) < 0.05, s);
        snprintf(s, sizeof s, "GLINT's echo: %.1f ms at 44.1 kHz, %.1f ms at %.0f kHz (a quarter at 90 BPM: 666.7)", e1, e2,
                 fs / 1000.0f);
        check(fabs(e1 - 666.7) < 6.0 && fabs(e2 - 666.7) < 6.0 && fabs(e1 - e2) < 3.0, s);
        snprintf(s, sizeof s, "CLOUD's RT60: %.2f s at 44.1 kHz, %.2f s at %.0f kHz", t1, t2, fs / 1000.0f);
        check(t1 > 1.0 && fabs(t2 / t1 - 1.0) < 0.05, s);
        snprintf(s, sizeof s, "LOFI TAPE's delay: %.2f ms at 44.1 kHz, %.2f ms at %.0f kHz (TP_D0: 9.52)", d1, d2,
                 fs / 1000.0f);
        check(fabs(d1 - 9.52) < 0.6 && fabs(d2 - 9.52) < 0.6 && fabs(d1 - d2) < 0.3, s);
        snprintf(s, sizeof s, "LOFI TAPE's flutter: %.2f turns a second at 44.1 kHz, %.2f at %.0f kHz (5)", w1, w2,
                 fs / 1000.0f);
        check(fabs(w1 - 5.0) < 0.3 && fabs(w2 - 5.0) < 0.3, s);
        snprintf(s, sizeof s, "MASTER FILTER's 3 dB point: %.0f Hz at 44.1 kHz, %.0f Hz at %.0f kHz", c1, c2, fs / 1000.0f);
        check(c1 > 100.0 && fabs(c2 / c1 - 1.0) < 0.02, s);
        snprintf(s, sizeof s, "MASTER EQ's MID: its top at %.0f Hz at 44.1 kHz, %.0f Hz at %.0f kHz", b1, b2, fs / 1000.0f);
        check(b1 > 100.0 && fabs(b2 / b1 - 1.0) < 0.02, s);
    }
    snprintf(s, sizeof s, "  at TAPE SPEED X0.50: %.2f turns a second (2.5)", wh);
    check(fabs(wh - 2.5) < 0.4, s);
    frs_close();
    printf("rate: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
