/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's DSP bench (host only): renders the DSP units on test material into WAV files, and runs them silently
 * for an instruction count (macOS: /usr/bin/time -l prints "instructions retired").
 *
 *   tools/bench/build.sh                  build/bench/bench, scalar like the device (no vectorising, no FMA)
 *   build/bench/bench render OUTDIR       every scenario into OUTDIR (WAV files)
 *   build/bench/bench cost WHAT SECONDS   WHAT: reverb, shimmer, voices1, voices4, voices8 (the whole engine, every
 *                                         layer on; voices8b: and the bass, voices8b4 its MODEL 4; voices8x: the
 *                                         layers' second pages' costly settings on); rust8 ...: the same with the
 *                                         drone preset RUST's settings (the heaviest); render SECONDS
 *                                         without writing anything
 *   build/bench/bench fx WHICH OUT.wav    8 s through the master effects (base, eq, lp, hp, tape ...), RUST's
 *                                         (rust, rusttape: eight voices, glints, the bass), a release (rel, 16 s) */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "dsp.h"
#include "nuage.h"
#include "frasil.h"

#define BLK 256u
#define MAXSEC 60u

/* ------------------------------------------------------------ WAV out --- */
static float *wl, *wr;
static uint32_t wn;

static void wav_write(const char *path, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, v, bytes = n * 6u;
    uint16_t h;
    if (!f) {
        perror(path);
        exit(1);
    }
    fwrite("RIFF", 1, 4, f);
    v = 36u + bytes;
    fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16;
    fwrite(&v, 4, 1, f);
    h = 1;
    fwrite(&h, 2, 1, f);
    h = 2;
    fwrite(&h, 2, 1, f);
    v = 44100;
    fwrite(&v, 4, 1, f);
    v = 44100u * 6u;
    fwrite(&v, 4, 1, f);
    h = 6;
    fwrite(&h, 2, 1, f);
    h = 24;
    fwrite(&h, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&bytes, 4, 1, f);
    for (i = 0; i < n; i++) {
        float s[2] = {wl[i], wr[i]};
        int c;
        for (c = 0; c < 2; c++) {
            int32_t q = (int32_t)(br_clampf(s[c], -1.0f, 1.0f) * 8388607.0f);
            uint8_t b[3] = {(uint8_t)q, (uint8_t)(q >> 8), (uint8_t)(q >> 16)};
            fwrite(b, 1, 3, f);
        }
    }
    fclose(f);
}

static void peak_report(const char *name, uint32_t n)
{
    float pk = 0.0f, rms = 0.0f;
    uint32_t i;
    for (i = 0; i < n; i++) {
        float a = fm_fabsf(wl[i]) > fm_fabsf(wr[i]) ? fm_fabsf(wl[i]) : fm_fabsf(wr[i]);
        pk = a > pk ? a : pk;
        rms += wl[i] * wl[i] + wr[i] * wr[i];
    }
    rms = fm_sqrtf(rms / (2.0f * (float)n));
    printf("%-28s peak %6.3f  rms %6.4f  (%u s)\n", name, pk, rms, n / 44100u);
}

/* ------------------------------------------------------ test material --- */
static uint32_t seed = 0x1234567u;

/* a band-limited saw (PolyBLEP) */
static float blep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

typedef struct {
    float ph[3], lp[2], env;
} padv_t;

/* a pad of notes (MIDI), three detuned saws each, a gentle low-pass; env rises over att seconds while t < hold */
static void pad(float *l, float *r, uint32_t n, const int *notes, int nn, float att, float hold, float rel, float bright)
{
    static padv_t v[8];
    uint32_t i;
    int k, o;
    memset(v, 0, sizeof v);
    for (k = 0; k < nn; k++)
        for (o = 0; o < 3; o++)
            v[k].ph[o] = (float)(br_rand(&seed) & 0xFFFF) / 65536.0f;
    for (i = 0; i < n; i++) {
        float t = (float)i / 44100.0f, sl = 0.0f, sr = 0.0f;
        for (k = 0; k < nn; k++) {
            float f = 440.0f * fm_exp2f(((float)notes[k] - 69.0f) / 12.0f), s[3];
            float target = t < hold ? 1.0f : 0.0f, a = bright;
            v[k].env += (target - v[k].env) * (target > v[k].env ? 1.0f / (att * 44100.0f) : 1.0f / (rel * 44100.0f));
            for (o = 0; o < 3; o++) {
                float dt = f * (1.0f + 0.004f * (float)(o - 1)) / 44100.0f;
                v[k].ph[o] += dt;
                if (v[k].ph[o] >= 1.0f)
                    v[k].ph[o] -= 1.0f;
                s[o] = 2.0f * v[k].ph[o] - 1.0f - blep(v[k].ph[o], dt);
            }
            v[k].lp[0] += a * (s[0] + s[1] - v[k].lp[0]);
            v[k].lp[1] += a * (s[0] + s[2] - v[k].lp[1]);
            sl += v[k].lp[0] * v[k].env;
            sr += v[k].lp[1] * v[k].env;
        }
        l[i] = sl * 0.08f;
        r[i] = sr * 0.08f;
    }
}

/* plucks: decaying sines with a click, one every gap seconds, pentatonic */
static void plucks(float *l, float *r, uint32_t n, float gap, float until)
{
    static const int SC[] = {62, 65, 69, 72, 74, 77, 81, 84};
    uint32_t i, every = (uint32_t)(gap * 44100.0f), stop = (uint32_t)(until * 44100.0f);
    float ph = 0.0f, f = 440.0f, env = 0.0f, pan = 0.5f;
    for (i = 0; i < n; i++) {
        float s;
        if (i < stop && i % every == 0) {
            f = 440.0f * fm_exp2f(((float)SC[br_rand(&seed) % 8u] - 69.0f) / 12.0f);
            env = 0.5f;
            pan = 0.2f + 0.6f * (float)(br_rand(&seed) & 255u) / 255.0f;
        }
        ph += f / 44100.0f;
        if (ph >= 1.0f)
            ph -= 1.0f;
        s = fm_sinf(6.2831853f * ph) * env;
        env *= 0.99985f;
        l[i] = s * (1.0f - pan);
        r[i] = s * pan;
    }
}

static void impulse(float *l, float *r, uint32_t n)
{
    memset(l, 0, n * sizeof *l);
    memset(r, 0, n * sizeof *r);
    l[100] = r[100] = 0.8f;
}

/* -------------------------------------------------------- scenarios --- */
static float *il, *ir;

static nuage_params_t rv_default(void)
{
    nuage_params_t p = {0};
    p.size = 0.75f;
    p.decay = 0.55f;
    p.damp = 0.5f;
    p.diffusion = 0.7f;
    p.mod = 0.5f;
    p.shimmer = 0.0f;
    p.interval = 12.0f;
    p.predelay = 0.1f;
    p.lowcut = 0.2f;
    p.width = 1.0f;
    return p;
}

/* the reverb over the input, mixed: wet at mix (equal power), with a freeze from t_frz to t_unfrz */
static void run_reverb(const nuage_params_t *pp, uint32_t n, float mix, float t_frz, float t_unfrz)
{
    nuage_params_t p = *pp;
    float ol[BLK], or_[BLK], gd = fm_cosf(mix * 1.5707963f), gw = fm_sinf(mix * 1.5707963f);
    uint32_t i0, i;
    nuage_init();
    nuage_set(&p);
    for (i0 = 0; i0 + BLK <= n; i0 += BLK) {
        float t = (float)i0 / 44100.0f;
        int fz = t >= t_frz && t < t_unfrz;
        if (fz != p.freeze) {
            p.freeze = fz;
            nuage_set(&p);
        }
        nuage_process(il + i0, ir + i0, ol, or_, BLK);
        for (i = 0; i < BLK; i++) {
            wl[i0 + i] = il[i0 + i] * gd + ol[i] * gw;
            wr[i0 + i] = ir[i0 + i] * gd + or_[i] * gw;
        }
    }
    wn = n - n % BLK;
}

static void scenario(const char *dir, const char *name, uint32_t sec)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    wav_write(path, wn);
    peak_report(name, wn);
    (void)sec;
}

static void render_all(const char *dir)
{
    static const int CHORD[] = {50, 57, 62, 64, 69};   /* D2 A2 D3 E3 A3 */
    static const int LOW[] = {38, 45};                  /* D1 A1 */
    nuage_params_t p;
    uint32_t n;

    /* 1. impulse: the raw tail, 100 % wet */
    n = 12u * 44100u;
    impulse(il, ir, n);
    p = rv_default();
    run_reverb(&p, n, 1.0f, 99.0f, 99.0f);
    scenario(dir, "01-impulse-hall", 12);

    /* 2. the dry pad, then the same with the reverb */
    n = 20u * 44100u;
    pad(il, ir, n, CHORD, 5, 2.0f, 8.0f, 1.5f, 0.12f);
    memcpy(wl, il, n * sizeof *il);
    memcpy(wr, ir, n * sizeof *ir);
    wn = n;
    scenario(dir, "02-pad-dry", 20);
    p = rv_default();
    run_reverb(&p, n, 0.5f, 99.0f, 99.0f);
    scenario(dir, "03-pad-hall", 20);

    /* 3. huge: size 1, decay long, dark, much modulation */
    p = rv_default();
    p.size = 1.0f;
    p.decay = 0.85f;
    p.damp = 0.35f;
    p.mod = 0.8f;
    p.predelay = 0.25f;
    run_reverb(&p, n, 0.6f, 99.0f, 99.0f);
    scenario(dir, "04-pad-cloud-huge", 20);

    /* 4. shimmer, an octave */
    p.shimmer = 0.6f;
    run_reverb(&p, n, 0.6f, 99.0f, 99.0f);
    scenario(dir, "05-pad-shimmer-octave", 20);

    /* 5. shimmer, a fifth, brighter */
    p.interval = 7.0f;
    p.damp = 0.6f;
    run_reverb(&p, n, 0.6f, 99.0f, 99.0f);
    scenario(dir, "06-pad-shimmer-fifth", 20);

    /* 6. plucks into a long cloud: the density of the echoes */
    n = 20u * 44100u;
    plucks(il, ir, n, 0.7f, 8.0f);
    p = rv_default();
    p.decay = 0.75f;
    p.diffusion = 0.9f;
    run_reverb(&p, n, 0.5f, 99.0f, 99.0f);
    scenario(dir, "07-plucks-cloud", 20);

    /* 7. freeze: the pad held at 6 s for 10 s, the pad released meanwhile */
    n = 24u * 44100u;
    pad(il, ir, n, CHORD + 1, 3, 1.5f, 5.0f, 1.0f, 0.15f);
    p = rv_default();
    p.decay = 0.7f;
    run_reverb(&p, n, 0.7f, 6.0f, 16.0f);
    scenario(dir, "08-freeze", 24);

    /* 8. low drone, small room vs the cloud: the low end */
    n = 16u * 44100u;
    pad(il, ir, n, LOW, 2, 1.0f, 8.0f, 2.0f, 0.06f);
    p = rv_default();
    p.size = 1.0f;
    p.decay = 0.9f;
    p.lowcut = 0.35f;
    run_reverb(&p, n, 0.55f, 99.0f, 99.0f);
    scenario(dir, "09-low-drone-cloud", 16);
}


/* ------------------------------------------------------------ the engine --- */
static frasil_params_t eng_default(void)
{
    frasil_params_t p;
    memset(&p, 0, sizeof p);
    p.mx_drone = p.mx_bass = p.mx_glint = p.mx_cloud = 1.0f;   /* the MIXER as it is, the compressor off */
    p.comp_ratio = p.comp_gain = 1.0f;
    p.comp_rel = 0.2f;
    p.pump_rel = 1.0f;
    p.pump_cloud = 1;
    p.pump_att = 0.005f;
    p.ch_interval = p.br_pitch = p.fullness = 1.0f;   /* the layers' second pages as they were, FULLNESS full */
    p.choir = 0.6f;
    p.partials = 0.7f;
    p.souffle = 0.25f;
    p.timbre = 0.5f;
    p.detune = 0.4f;
    p.cutoff = 0.35f;
    p.tilt = 0.45f;
    p.odd = 0.2f;
    p.stretch = 0.0f;
    p.resonance = 0.55f;
    p.color = 0.0f;
    p.attack = 0.7f;
    p.release = 0.65f;
    p.pressure = 0.6f;
    p.velocity = 0.3f;
    p.spread = 0.7f;
    p.motion = 0.35f;
    p.motion_rate = 0.4f;
    p.tide = 0.2f;
    p.tide_rate = 0.3f;
    p.drive = 0.15f;
    p.mix = 0.45f;
    p.rev = rv_default();
    p.rev.decay = 0.62f;
    p.rev.diffusion = 0.75f;
    return p;
}

/* the drone preset RUST (app/params.c DRONE_PRESET, its settings / 127 as params_to_engine takes them): the heaviest,
 * CHOIR's SHAPE and RESONANCE (band), BEAT, GRAIN, AIR, driven */
static frasil_params_t eng_rust(void)
{
    frasil_params_t p = eng_default();
    static const struct {
        size_t off;
        float v;
    } R[] = {
#define S(x, v) {offsetof(frasil_params_t, x), (float)(v) / 127.0f}
        S(choir, 80), S(partials, 50), S(souffle, 40), S(drive, 100), S(detune, 50), S(cutoff, 45), S(tilt, 65),
        S(odd, 60), S(stretch, 20), S(resonance, 80), S(color, 40), S(spread, 80), S(attack, 60), S(release, 80),
        S(motion, 40), S(motion_rate, 55), S(tide, 25), S(tide_rate, 40), S(timbre, 55), S(mix, 55), S(ch_shape, 70),
        S(ch_reso, 80), S(pa_evolve, 20), S(pa_beat, 15), S(pa_peak, 0), S(br_grain, 60), S(br_air, 20),
#undef S
    };
    uint32_t k;
    for (k = 0; k < sizeof R / sizeof R[0]; k++)
        *(float *)((char *)&p + R[k].off) = R[k].v;
    p.ch_interval = 1.0f;
    p.ch_type = 1;
    p.pa_formant = 3.0f * 75.0f / 127.0f;
    p.br_pitch = 1.0f;
    p.br_tone = (45.0f - 64.0f) / 63.0f;
    return p;
}

/* a score: at time t (s), note on (vel), off, or pressure */
typedef struct {
    float t;
    char what;                           /* 'n' on, 'f' off, 'p' pressure, 'a' all off, 'z' freeze */
    int note;
    float v;
} cue_t;

static void run_engine(const frasil_params_t *pp, const cue_t *cue, uint32_t n)
{
    frasil_params_t p = *pp;
    int32_t lr[2 * BLK];
    uint32_t i0, i;
    frasil_init();
    frasil_set(&p);
    for (i0 = 0; i0 + BLK <= n; i0 += BLK) {
        float t = (float)i0 / 44100.0f;
        while (cue && cue->what && cue->t <= t) {
            if (cue->what == 'n')
                frasil_note_on(cue->note, cue->v);
            else if (cue->what == 'f')
                frasil_note_off(cue->note);
            else if (cue->what == 'p')
                frasil_pressure(cue->note, cue->v);
            else if (cue->what == 'a')
                frasil_all_off();
            else if (cue->what == 's')
                frasil_glint_on(cue->note, cue->v);
            else if (cue->what == 'o')
                frasil_glint_off(cue->note);
            else if (cue->what == 'z') {
                p.rev.freeze = cue->v > 0.5f;
                frasil_set(&p);
            }
            cue++;
        }
        frasil_render(lr, BLK, 1.0f);
        for (i = 0; i < BLK; i++) {
            wl[i0 + i] = (float)lr[2 * i] / 8388607.0f;
            wr[i0 + i] = (float)lr[2 * i + 1] / 8388607.0f;
        }
    }
    wn = n - n % BLK;
}

/* a pressure gesture on a note: up to peak over rise s from t0, back down over fall s */
static cue_t *swell(cue_t *c, float t0, int note, float rise, float peak, float fall)
{
    int k;
    for (k = 1; k <= 20; k++)
        *c++ = (cue_t){t0 + rise * (float)k / 20.0f, 'p', note, peak * (float)k / 20.0f};
    for (k = 1; k <= 20; k++)
        *c++ = (cue_t){t0 + rise + 1.0f + fall * (float)k / 20.0f, 'p', note, peak * (1.0f - (float)k / 20.0f)};
    return c;
}

static void render_engine(const char *dir)
{
    frasil_params_t p;
    cue_t sc[256], *c;
    uint32_t n;

    /* 1. one low note, everything on, held 16 s */
    n = 26u * 44100u;
    p = eng_default();
    c = sc;
    *c++ = (cue_t){0.2f, 'n', 38, 0.8f};
    *c++ = (cue_t){16.0f, 'f', 38, 0};
    *c = (cue_t){0};
    run_engine(&p, sc, n);
    scenario(dir, "e1-drone-d1", 26);

    /* 2. each layer alone: a fifth, D2 A2 */
    n = 14u * 44100u;
    c = sc;
    *c++ = (cue_t){0.2f, 'n', 50, 0.8f};
    *c++ = (cue_t){0.2f, 'n', 57, 0.8f};
    *c++ = (cue_t){9.0f, 'a', 0, 0};
    *c = (cue_t){0};
    p = eng_default();
    p.partials = p.souffle = 0.0f;
    p.choir = 1.0f;
    run_engine(&p, sc, n);
    scenario(dir, "e2-choeur", 14);
    p = eng_default();
    p.choir = p.souffle = 0.0f;
    p.partials = 1.0f;
    run_engine(&p, sc, n);
    scenario(dir, "e3-partiels", 14);
    p = eng_default();
    p.choir = p.partials = 0.0f;
    p.souffle = 1.0f;
    run_engine(&p, sc, n);
    scenario(dir, "e4-souffle-air", 14);
    p.resonance = 0.9f;
    p.color = 1.0f;
    run_engine(&p, sc, n);
    scenario(dir, "e5-souffle-verre", 14);

    /* 3. a chord, a pressure swell on the top note from 7 s */
    n = 24u * 44100u;
    p = eng_default();
    c = sc;
    *c++ = (cue_t){0.2f, 'n', 50, 0.7f};
    *c++ = (cue_t){1.5f, 'n', 57, 0.7f};
    *c++ = (cue_t){3.0f, 'n', 62, 0.7f};
    *c++ = (cue_t){4.5f, 'n', 64, 0.6f};
    c = swell(c, 7.0f, 64, 3.0f, 1.0f, 3.0f);
    *c++ = (cue_t){17.0f, 'a', 0, 0};
    *c = (cue_t){0};
    run_engine(&p, sc, n);
    scenario(dir, "e6-accord-pression", 24);

    /* 4. glass and shimmer: SOUFFLE bells into the shimmering cloud */
    n = 30u * 44100u;
    p = eng_default();
    p.choir = 0.15f;
    p.partials = 0.3f;
    p.souffle = 0.9f;
    p.resonance = 0.85f;
    p.color = 1.0f;
    p.attack = 0.55f;
    p.mix = 0.65f;
    p.rev.size = 1.0f;
    p.rev.decay = 0.85f;
    p.rev.shimmer = 0.55f;
    p.rev.damp = 0.55f;
    c = sc;
    *c++ = (cue_t){0.2f, 'n', 69, 0.8f};
    *c++ = (cue_t){2.0f, 'n', 76, 0.7f};
    *c++ = (cue_t){4.0f, 'n', 81, 0.6f};
    *c++ = (cue_t){12.0f, 'a', 0, 0};
    *c = (cue_t){0};
    run_engine(&p, sc, n);
    scenario(dir, "e7-verre-shimmer", 30);

    /* 5. a long evolving texture: much motion, a fast tide, a freeze at 20 s */
    n = 40u * 44100u;
    p = eng_default();
    p.motion = 0.8f;
    p.motion_rate = 0.6f;
    p.tide = 0.7f;
    p.tide_rate = 0.75f;
    p.stretch = 0.25f;
    c = sc;
    *c++ = (cue_t){0.2f, 'n', 45, 0.8f};
    *c++ = (cue_t){0.2f, 'n', 52, 0.8f};
    *c++ = (cue_t){8.0f, 'n', 59, 0.6f};
    *c++ = (cue_t){20.0f, 'z', 0, 1.0f};
    *c++ = (cue_t){20.5f, 'a', 0, 0};
    *c++ = (cue_t){32.0f, 'z', 0, 0.0f};
    *c = (cue_t){0};
    run_engine(&p, sc, n);
    scenario(dir, "e8-texture-gel", 40);

    /* 6. glints over a drone: four presets, a phrase each (A minor pentatonic, two octaves up) */
    {
        static const struct {
            const char *name;
            glint_params_t sp;
        } PRE[4] = {
            {"e9-glint-ice", {3.5f, 0.55f, 0.55f, 0.7f, 0.7f, 0.6f}},
            {"e10-glint-bell", {1.414f, 0.63f, 0.75f, 0.67f, 0.63f, 0.4f}},
            {"e11-glint-glass", {7.0f, 0.39f, 0.67f, 0.63f, 0.8f, 0.55f}},
            {"e12-glint-frost", {11.0f, 0.35f, 0.43f, 0.6f, 0.87f, 0.95f}},
        };
        static const int PH[] = {81, 84, 88, 86, 93, 91, 88, 84, 86, 81};
        int k, j;
        n = 16u * 44100u;
        for (k = 0; k < 4; k++) {
            p = eng_default();
            p.glint = PRE[k].sp;
            c = sc;
            *c++ = (cue_t){0.1f, 'n', 45, 0.7f};
            *c++ = (cue_t){0.1f, 'n', 52, 0.7f};
            for (j = 0; j < 10; j++) {
                *c++ = (cue_t){3.0f + 0.45f * (float)j, 's', PH[j], 0.6f + 0.04f * (float)(j % 5)};
                *c++ = (cue_t){3.0f + 0.45f * (float)j + 0.2f, 'o', PH[j], 0};
            }
            *c++ = (cue_t){11.0f, 'a', 0, 0};
            *c = (cue_t){0};
            run_engine(&p, sc, n);
            scenario(dir, PRE[k].name, 16);
        }
    }
}

/* probe: a D minor chord (D2 A2 F3) held 10 s with the defaults and some fields set, into a WAV, to measure what a
 * setting changes: bench probe OUT.wav [FIELD=VALUE ...] */
static const struct {
    const char *name;
    size_t off;
} PROBE[] = {
#define F(x) {#x, offsetof(frasil_params_t, x)}
    F(choir), F(partials), F(souffle), F(timbre), F(detune), F(cutoff), F(tilt), F(odd), F(stretch), F(resonance),
    F(color), F(spread), F(motion), F(ch_shape), F(ch_interval), F(ch_reso), F(pa_evolve), F(pa_beat),
    F(pa_formant), F(pa_peak), F(br_grain), F(br_pitch), F(br_tone), F(br_air), F(mix), F(sink),
#undef F
};

static int probe(int argc, char **argv)
{
    frasil_params_t p = eng_default();
    cue_t sc[8], *c = sc;
    int k;
    uint32_t j;
    p.mix = 0.0f;                        /* dry: the layer, not the cloud */
    for (k = 3; k < argc; k++) {
        const char *eq = strchr(argv[k], '=');
        if (!strncmp(argv[k], "ch_type=", 8))   /* (an int) */
            p.ch_type = atoi(argv[k] + 8);
        for (j = 0; eq && j < sizeof PROBE / sizeof PROBE[0]; j++)
            if (strlen(PROBE[j].name) == (size_t)(eq - argv[k]) && !strncmp(argv[k], PROBE[j].name, (size_t)(eq - argv[k])))
                *(float *)((char *)&p + PROBE[j].off) = (float)atof(eq + 1);
    }
    p.attack = 0.2f;
    *c++ = (cue_t){0.05f, 'n', 38, 0.8f};
    for (k = 3; k < argc && strcmp(argv[k], "solo"); k++)   /* solo: D2 alone */
        ;
    if (k >= argc) {
        *c++ = (cue_t){0.05f, 'n', 45, 0.8f};
        *c++ = (cue_t){0.05f, 'n', 53, 0.8f};
    }
    *c = (cue_t){0};
    run_engine(&p, sc, 10u * 44100u);
    wav_write(argv[2], wn);
    return 0;
}

int main(int argc, char **argv)
{
    uint32_t maxn = MAXSEC * 44100u;
    il = calloc(maxn, sizeof *il);
    ir = calloc(maxn, sizeof *ir);
    wl = calloc(maxn, sizeof *wl);
    wr = calloc(maxn, sizeof *wr);
    if (argc >= 3 && !strcmp(argv[1], "probe"))
        return probe(argc, argv);
    if (argc >= 3 && !strcmp(argv[1], "render")) {
        render_all(argv[2]);
        render_engine(argv[2]);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "cost") && (!strncmp(argv[2], "voices", 6) || !strncmp(argv[2], "rust", 4))) {
        int rust = argv[2][0] == 'r';    /* rustN...: RUST's settings (eng_rust), else every layer at 0.8 */
        const char *a = argv[2] + (rust ? 4 : 6);
        uint32_t n = (uint32_t)atoi(argv[3]) * 44100u, i0, nv = (uint32_t)atoi(a), k;
        frasil_params_t p = rust ? eng_rust() : eng_default();
        int32_t lr[2 * BLK];
        const char *only = strchr(argv[2], ':');
        if (!rust) {
            p.choir = p.partials = p.souffle = 0.8f;
            p.rev.shimmer = 0.4f;
        }
        if (only) {                     /* voicesN:cps, the layers to keep */
            p.choir = strchr(only, 'c') ? 0.8f : 0.0f;
            p.partials = strchr(only, 'p') ? 0.8f : 0.0f;
            p.souffle = strchr(only, 's') ? 0.8f : 0.0f;
        }
        if (strchr(a, 'x')) {            /* voicesNx: and what costs on the layers' second pages, all, or those named
                                         * after it: s SHAPE, r RESONANCE, g GRAIN, t TONE, a AIR, p PEAK,
                                         * q BEAT, y TYPE */
            const char *o = strchr(a, 'x') + 1;
            int all = !*o || *o == ':';
            p.ch_shape = all || strchr(o, 's') ? 1.0f : 0.0f;
            p.ch_reso = all || strchr(o, 'r') ? 0.6f : 0.0f;
            p.br_grain = all || strchr(o, 'g') ? 0.5f : 0.0f;
            p.br_tone = all || strchr(o, 't') ? 0.5f : 0.0f;
            p.br_air = all || strchr(o, 'a') ? 0.5f : 0.0f;
            p.pa_peak = all || strchr(o, 'p') ? 1.0f : 0.0f;
            p.pa_beat = all || strchr(o, 'q') ? 1.0f : 0.0f;   /* q BEAT, y TYPE (high) */
            p.ch_type = all || strchr(o, 'y') ? 2 : 0;
        }
        frasil_init();
        frasil_set(&p);
        for (k = 0; k < nv; k++)
            frasil_note_on(40 + 5 * (int)k, 0.8f);
        if (strchr(a, 'f')) {            /* voicesNf: and the master effects, all on: MASTER EQ (its three bands
                                         * moved), MASTER FILTER (a low pass on the drones and GLINT), LOFI TAPE (on
                                         * everything): the most they cost together; voicesNfe, fl, ft: one alone */
            const char *o = strchr(a, 'f') + 1;
            p.eq_on = 1;
            p.eq_low = 3.0f;
            p.eq_mid = -4.0f;
            p.eq_high = 2.0f;
            p.eq_lowf = 120.0f;
            p.eq_midf = 400.0f;
            p.eq_q = 1.0f;
            p.eq_highf = 8000.0f;
            p.fl_type = 1;
            p.fl_cut = 1500.0f;
            p.fl_q = 2.0f;
            p.fl_tracks = 3;
            p.tp_on = 1;
            p.tp_sat = 0.6f;
            p.tp_wobble = 0.6f;
            p.tp_tracks = 7;
            if (*o) {
                p.eq_on = *o == 'e';
                p.fl_type = *o == 'l';
                p.tp_on = *o == 't';
            }
            frasil_set(&p);
        }
        if (strchr(a, 'b')) {            /* voicesNbM: and the bass, MODEL M (0 the sine) */
            p.bass = (bass_params_t){.model = atoi(strchr(a, 'b') + 1) % BASS_NMODEL, .drive = 0.2f,
                                     .filter = 0.6f, .resonance = 0.3f, .glide = 0.15f, .level = 0.7f,
                                     .attack = 0.2f, .release = 0.4f};
            frasil_set(&p);
            frasil_bass_on(33, 0.8f);
        }
        for (i0 = 0; i0 + BLK <= n; i0 += BLK)
            frasil_render(lr, BLK, 1.0f);
        printf("%d\n", lr[0]);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "fx")) {   /* fx WHICH OUT.wav: 8 s of a chord (every layer), the same notes and
                                                  * noise each run, through nothing (base), MASTER EQ (eq: MID -12 DB
                                                  * at 1 KHZ, Q 1), the filter on everything (lp, hp: 500 HZ, Q 0.71)
                                                  * or the tape (tape: SATURATION and WOBBLE at half, or as
                                                  * TP_SAT and TP_WOB have them, 0..1); eqknob: LOW turned as a
                                                  * knob is, half a dB a step every 12 ms, 0 to +12 to -12 */
        const char *w = argv[2];
        int mix = !strncmp(w, "mix", 3);   /* mixall: RUST's (below) with PUMP, MASTERING, the pans, SINK, the
                                            * metronome and the EQ; mixfx: and the filter and the tape on some tracks */
        int rust = !strncmp(w, "rust", 4) || mix, rel = !strcmp(w, "rel");   /* rust, rusttape: RUST on eight voices,
                                                                               * glints and the bass; rel: the chord
                                                                               * let go at 2 s, its tails (16 s) */
        uint32_t n = (rel ? 16u : 8u) * 44100u, i0, i;
        frasil_params_t p = rust ? eng_rust() : eng_default();
        int32_t lr[2 * BLK];
        p.eq_lowf = 120.0f;
        p.eq_midf = 1000.0f;
        p.eq_q = 1.0f;
        p.eq_highf = 8000.0f;
        p.fl_cut = 500.0f;
        p.fl_q = 0.7071f;
        p.fl_tracks = p.tp_tracks = 7;
        p.tp_sat = p.tp_wobble = 0.5f;
        p.eq_on = !strcmp(w, "eq");
        p.eq_mid = p.eq_on ? -12.0f : 0.0f;
        p.fl_type = !strcmp(w, "lp") ? 1 : !strcmp(w, "hp") ? 2 : 0;
        p.tp_on = !strcmp(w, "tape") || !strcmp(w, "rusttape");
        if (rust) {
            p.glint = (glint_params_t){.ratio = 3.5f, .bright = 0.55f, .decay = 0.55f, .level = 0.7f, .space = 0.7f,
                                       .release = 0.6f, .delay_s = 0.3f, .delay_fb = 0.4f, .delay_mix = 0.3f,
                                       .delay_pp = 1};
            p.bass = (bass_params_t){.model = 1, .drive = 0.2f, .filter = 0.6f, .resonance = 0.3f, .glide = 0.15f,
                                     .level = 0.7f, .attack = 0.2f, .release = 0.4f, .sustain = 1.0f};
        }
        if (mix) {
            p.pump_drone = 0.6f;
            p.pump_bass = 0.5f;
            p.pump_glint = 0.4f;
            p.pump_att = 0.01f;
            p.pump_rel = 0.6f;
            p.pump_off = 0.1f;
            p.comp_ratio = 4.0f;
            p.comp_thr = -18.0f;
            p.comp_gain = 1.5f;
            p.comp_rel = 0.2f;
            p.pan[0] = 0.3f;
            p.pan[1] = -0.5f;
            p.pan[2] = 0.7f;
            p.pan[3] = -0.2f;
            p.sink = 0.3f;
            p.eq_on = 1;
            p.eq_low = 3.0f;
            p.eq_mid = -4.0f;
            p.eq_high = 2.0f;
            if (!strcmp(w, "mixfx")) {
                p.fl_type = 1;
                p.fl_cut = 1200.0f;
                p.fl_q = 1.5f;
                p.fl_tracks = 3;
                p.tp_on = 1;
                p.tp_tracks = 5;
            }
        }
        if (getenv("DRY"))               /* (DRY=1: no cloud in what comes out: its int16 lines round a change upstream
                                          * into a noise of their LSB, some -80 dB, which hides the change's own) */
            p.mix = 0.0f;
        if (getenv("TP_SAT"))
            p.tp_sat = (float)atof(getenv("TP_SAT"));
        if (getenv("TP_WOB"))
            p.tp_wobble = (float)atof(getenv("TP_WOB"));
        if (!strcmp(w, "eqknob"))
            p.eq_on = 1;
        if (!strncmp(w, "tp", 2u))     /* (tptracks, tpon, tpsat, tpwob: the tape on, its settings stepped) */
            p.tp_on = 1;
        if (!strcmp(w, "chtype"))      /* (CHOIR's TYPE stepped, some RESONANCE) */
            p.ch_reso = 0.3f;
        if (!strcmp(w, "fltracks") || !strcmp(w, "fltype")) {   /* the filter's TRACKS or TYPE stepped every 200 ms */
            p.fl_type = 1;
            p.fl_cut = 800.0f;
            p.fl_q = 2.0f;
        }
        frasil_init();
        frasil_set(&p);
        frasil_note_on(38, 0.8f);
        frasil_note_on(45, 0.8f);
        frasil_note_on(52, 0.8f);
        frasil_note_on(57, 0.8f);
        if (rust) {
            static const int MORE[4] = {62, 64, 69, 74};
            for (i = 0; i < 4u; i++)
                frasil_note_on(MORE[i], 0.7f);
            frasil_bass_on(26, 0.8f);
        }
        for (i0 = 0; i0 + BLK <= n; i0 += BLK) {
            if (rel && i0 >= 2u * 44100u && i0 < 2u * 44100u + BLK)
                frasil_all_off();
            if (mix) {                   /* the beat at 120 BPM, a tick on each */
                frasil_pump((float)i0 / 22050.0f - (float)(i0 / 22050u), 2.0f / 44100.0f);
                if (i0 % 22050u < BLK)
                    frasil_click(i0 % 88200u < BLK);
            }
            if (rust) {                  /* a glint every half second, held 0.3 s */
                static const int PH[5] = {81, 84, 88, 86, 93};
                if (i0 % 22050u < BLK)
                    frasil_glint_on(PH[i0 / 22050u % 5u], 0.7f);
                if (i0 >= 13230u && (i0 - 13230u) % 22050u < BLK)
                    frasil_glint_off(PH[(i0 - 13230u) / 22050u % 5u]);
            }
            if (i0 >= 44100u && (i0 - 44100u) % 8820u < BLK) {   /* from 1 s, every 200 ms: the setting changed */
                static const int TRK[7] = {7, 1, 2, 4, 3, 5, 6};
                uint32_t k = (i0 - 44100u) / 8820u;
                int hit = 1;
                if (!strcmp(w, "fltracks"))
                    p.fl_tracks = TRK[k % 7u];
                else if (!strcmp(w, "fltype"))
                    p.fl_type = (int)(k % 3u);
                else if (!strcmp(w, "tptracks"))
                    p.tp_tracks = TRK[k % 7u];
                else if (!strcmp(w, "tpon"))
                    p.tp_on = (int)(k & 1u);
                else if (!strcmp(w, "tpsat"))
                    p.tp_sat = (float)(k % 8u) / 7.0f;
                else if (!strcmp(w, "tpwob"))
                    p.tp_wobble = (float)(k % 8u) / 7.0f;
                else if (!strcmp(w, "chtype"))
                    p.ch_type = (int)(k % 3u);
                else if (!strcmp(w, "shed")) {   /* the engine lightened, a step every 200 ms, up then down */
                    static const int LV[8] = {1, 2, 3, 2, 1, 0, 2, 0};
                    frasil_shed(LV[k % 8u]);
                    hit = 0;
                }
                else
                    hit = 0;
                if (hit)
                    frasil_set(&p);
            }
            if (!strcmp(w, "eqknob") && i0 >= 44100u) {   /* from 1 s: a step every 12 ms (two blocks) */
                uint32_t k = (i0 - 44100u) / (2u * BLK);
                p.eq_low = k <= 24u ? 0.5f * (float)k : k <= 72u ? 12.0f - 0.5f * (float)(k - 24u) : -12.0f;
                frasil_set(&p);
            }
            frasil_render(lr, BLK, 1.0f);
            for (i = 0; i < BLK; i++) {
                wl[i0 + i] = (float)lr[2 * i] / 8388607.0f;
                wr[i0 + i] = (float)lr[2 * i + 1] / 8388607.0f;
            }
        }
        wav_write(argv[3], n);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "cost")) {
        uint32_t n = (uint32_t)atoi(argv[3]) * 44100u, i0;
        nuage_params_t p = rv_default();
        float ol[BLK], or_[BLK];
        if (!strcmp(argv[2], "shimmer"))
            p.shimmer = 0.5f;
        for (i0 = 0; i0 < maxn; i0++)   /* noise in: every path busy */
            il[i0] = ir[i0] = br_randf(&seed) * 0.3f;
        nuage_init();
        nuage_set(&p);
        for (i0 = 0; i0 + BLK <= n; i0 += BLK)
            nuage_process(il + (i0 % (maxn - BLK)), ir + (i0 % (maxn - BLK)), ol, or_, BLK);
        printf("%g\n", (double)(ol[0] + or_[0]));
        return 0;
    }
    fprintf(stderr, "usage: bench render OUTDIR | bench cost reverb|shimmer SECONDS\n");
    return 2;
}
