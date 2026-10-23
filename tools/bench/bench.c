/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's DSP bench (host only): renders the DSP units on test material into WAV files, and runs them silently
 * for an instruction count (macOS: /usr/bin/time -l prints "instructions retired").
 *
 *   tools/bench/build.sh                  build/bench/bench, scalar like the device (no vectorising, no FMA)
 *   build/bench/bench render OUTDIR       every scenario into OUTDIR (WAV files)
 *   build/bench/bench cost WHAT SECONDS   WHAT: reverb, shimmer, voices1, voices4, voices8 (the whole engine, every
 *                                         layer on; voices8b: and the bass, voices8b4 its MODEL 4; voices8x: the
 *                                         layers' second pages' costly settings on); render SECONDS
 *                                         without writing anything */
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
    if (argc >= 4 && !strcmp(argv[1], "cost") && !strncmp(argv[2], "voices", 6)) {
        uint32_t n = (uint32_t)atoi(argv[3]) * 44100u, i0, nv = (uint32_t)atoi(argv[2] + 6), k;
        frasil_params_t p = eng_default();
        int32_t lr[2 * BLK];
        const char *only = strchr(argv[2], ':');
        p.choir = p.partials = p.souffle = 0.8f;
        p.rev.shimmer = 0.4f;
        if (only) {                     /* voicesN:cps, the layers to keep */
            p.choir = strchr(only, 'c') ? 0.8f : 0.0f;
            p.partials = strchr(only, 'p') ? 0.8f : 0.0f;
            p.souffle = strchr(only, 's') ? 0.8f : 0.0f;
        }
        if (strchr(argv[2] + 6, 'x')) {  /* voicesNx: and what costs on the layers' second pages, all, or those named
                                         * after it: s SHAPE, r RESONANCE, g GRAIN, t TONE, a AIR, p PEAK,
                                         * q BEAT, y TYPE */
            const char *o = strchr(argv[2] + 6, 'x') + 1;
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
        if (strchr(argv[2] + 6, 'b')) {  /* voicesNbM: and the bass, MODEL M (0 the sine) */
            p.bass = (bass_params_t){.model = atoi(strchr(argv[2] + 6, 'b') + 1) % BASS_NMODEL, .drive = 0.2f,
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
