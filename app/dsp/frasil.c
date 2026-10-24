/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's sound engine (frasil.h).
 *
 * A block (the platform's, 256 frames): the queued notes; the drift (an Ornstein-Uhlenbeck walk per thing that
 * drifts, a step a block) and the tide; each voice's frequencies, partial amplitudes and filter coefficients for
 * the block; then each voice in chunks of 32 frames (the envelope and pressure ramp across a chunk), its layers
 * summed into its stereo pair and placed in the field; then the mix's DC blocker and warmth, the tracks' levels
 * (MIXER), the reverb, PUMP's duck on the beat, the compressor (MASTERING) and the soft limit at the end.
 *
 * CHOEUR   three PolyBLEP saws, centre (at INTERVAL), -detune left, +detune right, each drifting a few cents, each
 *          minus SHAPE's share of itself a width on (a square at half a turn, then narrower pulses), the width
 *          swaying on its own (PWM); two one-poles per side, the cutoff in harmonics of the note (so a timbre is the
 *          same over the keyboard); with RESONANCE or TYPE a state-variable filter (TPT): low, band or high
 * PARTIELS 8 partials as rotating phasors (x' = cx - sy, y' = sx + cy: no table, no phase), the fundamental in the
 *          centre, the even ones to one side and the odd ones to the other as SPREAD asks; each one's amplitude and
 *          tuning drift on their own (EVOLVE deeper): the spectrum evolves without anything being played; BEAT
 *          pulses each at its own pace (its level ramped across the block); a formant (FORMANT, PEAK) lifts the ones
 *          near it, and at the top leaves that harmonic alone
 * SOUFFLE  noise (a hiss, or GRAIN's sparse grains; TONE darker or brighter, the same for every voice) through three
 *          resonators (two-pole band-passes) at the note times ratios (COLOR) times PITCH, Q 2 (air) .. 600 (glass),
 *          levelled so the loudness does not follow Q or pitch; AIR, the noise itself beside them */
#include <stddef.h>
#include "dsp.h"
#include "frasil.h"

#define NV FR_NV
#define NP 8
#define NR 3
#define CH 64u
#define MAXN 256u
#define VOICE_G 0.2f

/* ---------------------------------------------------------------- events --- */
enum { EV_ON = 1, EV_OFF, EV_ALLOFF, EV_PRS, EV_SPON, EV_SPOFF, EV_SPPRS, EV_BON, EV_BOFF, EV_BPRS, EV_BPUL, EV_CLICK };
typedef struct {
    uint8_t type;
    int8_t note;
    float v;
} ev_t;
#define NEV 64u
static ev_t evq[NEV];
static float ck_ph, ck_inc, ck_env;      /* the metronome's tick: a sine blip, its phase (turns), step, level */
static volatile uint32_t ev_w, ev_r;

static void ev_push(uint8_t type, int note, float v)
{
    uint32_t w = ev_w;
    if (w - ev_r >= NEV)
        return;                          /* full: dropped (the render takes them every block) */
    evq[w & (NEV - 1u)].type = type;
    evq[w & (NEV - 1u)].note = (int8_t)note;
    evq[w & (NEV - 1u)].v = v;
    __asm__ volatile("" ::: "memory");   /* the slot before the index */
    ev_w = w + 1u;
}

void frasil_note_on(int note, float vel) { ev_push(EV_ON, note, vel); }
void frasil_note_off(int note) { ev_push(EV_OFF, note, 0.0f); }
void frasil_all_off(void) { ev_push(EV_ALLOFF, 0, 0.0f); }
void frasil_pressure(int note, float p) { ev_push(EV_PRS, note, p); }
void frasil_glint_on(int note, float vel) { ev_push(EV_SPON, note, vel); }
void frasil_glint_off(int note) { ev_push(EV_SPOFF, note, 0.0f); }
void frasil_glint_pressure(int note, float p) { ev_push(EV_SPPRS, note, p); }
void frasil_bass_on(int note, float vel) { ev_push(EV_BON, note, vel); }
void frasil_bass_off(void) { ev_push(EV_BOFF, 0, 0.0f); }
void frasil_bass_pressure(float p) { ev_push(EV_BPRS, 0, p); }
void frasil_bass_pulse(float gap) { ev_push(EV_BPUL, 0, gap); }
void frasil_click(int accent) { ev_push(EV_CLICK, 0, accent ? 1.0f : 0.0f); }

/* ---------------------------------------------------------------- voices --- */
typedef struct {
    int8_t note;                         /* -1: free */
    uint8_t gate;
    uint32_t age;                        /* note-on order (stealing) */
    float vel, prs, prs_t;
    float env;                           /* 0..1; the amplitude is its square */
    float lvl;                           /* the gain at the end of the last chunk (the next ramps from it) */
    float pan;                           /* -1..1 */
    /* drift, each about -1..1 */
    float ou_c[3], ou_cut, ou_pa[NP], ou_pf[NP], ou_r[NR], ou_pan;
    /* CHOEUR */
    float cph[3], cdt[3];                /* phases, increments */
    float cw[3];                         /* SHAPE: how far on the saw taken off it is (the pulse's width) */
    float cpw[3];                        /* the widths' sway, a turn each */
    float csh;                           /* SHAPE: how much of the saw is taken off, 0..1 */
    float sc0, sc1, sc2;                 /* the state-variable's output: its input, band and low parts */
    float clp[4];                        /* left two stages, right two (the state-variable's: band, low) */
    float ca;                            /* one-pole coefficient */
    float sa1, sa2, sa3;                 /* the state-variable's (RESONANCE) */
    float cgain;                         /* SHAPE's and RESONANCE's level made up */
    uint8_t csvf;                        /* the filter running: the state-variable (1) or the one-poles */
    /* PARTIELS */
    float px[NP], py[NP], pc[NP], ps[NP], pa[NP];
    float pac[NP], pda[NP];              /* BEAT: the level now and its step a sample (ramped to pa over the block) */
    float pbt[NP];                       /* BEAT: each one's pulse, a turn */
    int np;                              /* how many are under the limit */
    /* SOUFFLE */
    float r1[NR], r2[NR];                /* two-pole resonators: states, coefficients, gains */
    float rb1[NR], rb2[NR], rg[NR];
    float gl, gr;                        /* its place: left and right gains */
} voice_t;

static voice_t vo[NV];
static uint32_t ages;
static uint32_t rng = 0x2545F491u;

static frasil_params_t P, T, pend;       /* the render's parameters (gliding to T), the target, and what the main
                                         * loop last set */
#define NGLIDE (offsetof(frasil_params_t, rev) / sizeof(float))   /* the floats before rev glide: no steps, no clicks */
static volatile uint32_t pend_seq;
static uint32_t pend_seen;

static volatile int shed;                /* frasil_shed */
static volatile float swell;             /* frasil_swell */
static const uint8_t SHED_NP[4] = {NP, 6, 5, 4}, SHED_NV[4] = {NV, NV, 7, 6};
static float tide_ph, tide_v;
static float nlp;                        /* TONE's filter on the noise */
static float dcl, dcr;
static float thl, thr;                   /* FULLNESS's high-pass */
static float gain_now;
static float peak;
static float noise[MAXN];
static float mixl[MAXN], mixr[MAXN], wetl[MAXN], wetr[MAXN];
static float spl[MAXN], spr[MAXN], rinl[MAXN], rinr[MAXN];   /* the glints dry, and the reverb's input */

static const float LOG2K[NP] = {0.0f, 1.0f, 1.5849625f, 2.0f, 2.3219281f, 2.5849625f, 2.8073549f, 3.0f};
/* SOUFFLE's ratios, COLOR 0 .. 1 across them: harmonic, odd, bell (a free bar's modes) */
static const float RATIOS[3][NR] = {{1.0f, 2.0f, 3.0f}, {1.0f, 3.0f, 5.0f}, {1.0f, 2.756f, 5.404f}};
static float RATIO_L2[3][NR];            /* their log2 (frasil_init) */

static void voice_start(voice_t *v, int note, float vel, int fresh)
{
    uint32_t k;
    v->note = (int8_t)note;
    v->gate = 1;
    v->vel = vel;
    v->age = ++ages;
    if (!fresh)
        return;
    v->env = 0.0f;
    v->lvl = 0.0f;
    v->prs = v->prs_t = 0.0f;
    /* a place in the field from the note, so a chord opens out (the drift moves it) */
    v->pan = (float)(((note * 7) % 12) - 6) * (1.0f / 6.0f);
    for (k = 0; k < 3; k++) {
        v->cph[k] = (float)(br_rand(&rng) >> 8) * (1.0f / 16777216.0f);
        v->cpw[k] = (float)k * 0.33f;
        v->ou_c[k] = br_randf(&rng) * 0.5f;
    }
    for (k = 0; k < 4; k++)
        v->clp[k] = 0.0f;
    v->csvf = 0;
    for (k = 0; k < NP; k++) {           /* random phases: no peak where they all start together */
        float ph = (float)(br_rand(&rng) >> 8) * (6.2831853f / 16777216.0f);
        v->px[k] = fm_cosf(ph);
        v->py[k] = fm_sinf(ph);
        v->ou_pa[k] = br_randf(&rng) * 0.5f;
        v->ou_pf[k] = br_randf(&rng) * 0.5f;
        v->pac[k] = v->pda[k] = 0.0f;
        v->pbt[k] = (float)(br_rand(&rng) >> 8) * (1.0f / 16777216.0f);
    }
    for (k = 0; k < NR; k++) {
        v->r1[k] = v->r2[k] = 0.0f;
        v->ou_r[k] = br_randf(&rng) * 0.5f;
    }
    v->ou_cut = br_randf(&rng) * 0.5f;
    v->ou_pan = 0.0f;
}

static void note_on(int note, float vel)
{
    voice_t *v = 0;
    uint32_t i, oldest = 0xFFFFFFFFu;
    float quiet = 2.0f;
    for (i = 0; i < NV; i++)             /* the same note again: that voice, from where it is */
        if (vo[i].note == note) {
            voice_start(&vo[i], note, vel, 0);
            return;
        }
    for (i = 0; i < NV && !v; i++)
        if (vo[i].note < 0)
            v = &vo[i];
    if (v) {
        voice_start(v, note, vel, 1);
        return;
    }
    for (i = 0; i < NV; i++)             /* none free: the quietest released one, else the oldest */
        if (!vo[i].gate && vo[i].env < quiet) {
            quiet = vo[i].env;
            v = &vo[i];
        }
    if (!v)
        for (i = 0; i < NV; i++)
            if (vo[i].age < oldest) {
                oldest = vo[i].age;
                v = &vo[i];
            }
    voice_start(v, note, vel, 0);        /* its phases go on: no click, the new pitch from its level */
}

static void events(void)
{
    while (ev_r != ev_w) {
        ev_t e = evq[ev_r & (NEV - 1u)];
        uint32_t i;
        __asm__ volatile("" ::: "memory");
        ev_r = ev_r + 1u;
        switch (e.type) {
        case EV_ON:
            note_on(e.note, e.v);
            break;
        case EV_OFF:
            for (i = 0; i < NV; i++)
                if (vo[i].note == e.note)
                    vo[i].gate = 0;
            break;
        case EV_ALLOFF:
            for (i = 0; i < NV; i++)
                vo[i].gate = 0;
            break;
        case EV_SPON:
            glint_on(e.note, e.v, &T.glint);
            break;
        case EV_SPOFF:
            glint_off(e.note, &T.glint);
            break;
        case EV_SPPRS:
            glint_press(e.note, e.v);
            break;
        case EV_BON:
            bass_on(e.note, e.v);
            break;
        case EV_BOFF:
            bass_off();
            break;
        case EV_BPRS:
            bass_press(e.v);
            break;
        case EV_BPUL:
            bass_pulse(e.v);
            break;
        case EV_CLICK:                   /* a tick: 1.7 kHz, the bar's first 2.6 kHz and louder */
            ck_ph = 0.0f;
            ck_inc = (e.v > 0.5f ? 2600.0f : 1700.0f) * BR_INV_FS;
            ck_env = e.v > 0.5f ? 0.2f : 0.12f;
            break;
        case EV_PRS:
            for (i = 0; i < NV; i++)
                if (vo[i].note >= 0 && (e.note < 0 ? vo[i].gate : vo[i].note == e.note))
                    vo[i].prs_t = br_clampf(e.v, 0.0f, 1.0f);
            break;
        }
    }
}

/* ----------------------------------------------------------------- drift --- */
/* one Ornstein-Uhlenbeck step: back towards 0 by th, a kick of s (unit variance: three uniforms) */
static inline float ou(float x, float th, float s)
{
    float r = br_randf(&rng) + br_randf(&rng) + br_randf(&rng);
    return br_clampf(x - th * x + s * r, -1.5f, 1.5f);
}

static void drift(uint32_t n)
{
    float tau = 20.0f * fm_powf(0.025f, P.motion_rate);   /* 20 s .. 0.5 s */
    float th = (float)n * BR_INV_FS / tau, s = 0.6f * fm_sqrtf(2.0f * th);
    float te = th * (1.0f + 3.0f * P.pa_evolve), se = 0.6f * fm_sqrtf(2.0f * te);
    uint32_t i, k;
    for (i = 0; i < NV; i++) {
        voice_t *v = &vo[i];
        if (v->note < 0)
            continue;
        for (k = 0; k < 3; k++)
            v->ou_c[k] = ou(v->ou_c[k], th, s);
        for (k = 0; k < NP; k++) {
            v->ou_pa[k] = ou(v->ou_pa[k], te, se);   /* (EVOLVE: deeper, and up to four times quicker) */
            v->ou_pf[k] = ou(v->ou_pf[k], th, s);
        }
        for (k = 0; k < NR; k++)
            v->ou_r[k] = ou(v->ou_r[k], th, s);
        v->ou_cut = ou(v->ou_cut, th, s);
        v->ou_pan = ou(v->ou_pan, th * 0.5f, s * 0.7f);
    }
    /* the tide: one turn in 5 min .. 10 s */
    tide_ph += (float)n * BR_INV_FS / (300.0f * fm_powf(1.0f / 30.0f, P.tide_rate));
    if (tide_ph >= 1.0f)
        tide_ph -= 1.0f;
    tide_v = fm_sinf(6.2831853f * tide_ph);
}

/* -------------------------------------------------- a voice's block setup --- */
static void voice_block(voice_t *v)
{
    float f = 440.0f * fm_exp2f(((float)v->note - 69.0f + P.tune) * (1.0f / 12.0f));
    float m = P.motion, prs = br_clampf(v->prs + swell, 0.0f, 1.0f) * P.pressure;
    float bright = (P.timbre - 0.5f) + tide_v * P.tide * 0.5f;
    float sum2 = 0.0f, norm, pe, pw, sx, ce, hc, fc, pan, col, q, sb;
    const float *ra, *rb;            /* log2 of the two ratio sets COLOR is between */
    uint32_t j;
    int np;

    /* CHOEUR: the saws' tunings (the centre's at INTERVAL), SHAPE's widths, the low-pass (BLOOM closing it while
     * the voice is quiet), RESONANCE's state-variable */
    if (P.choir > 0.001f) {
        static const float SIDE[3] = {0.0f, -1.0f, 1.0f}, PWR[3] = {0.21f, 0.29f, 0.37f};   /* the sways, Hz */
        float dc = P.detune * P.detune * 40.0f * (0.2f + 0.8f * P.fullness), sh = P.ch_shape;   /* (FULLNESS: closer) */
        float a = sh < 0.5f ? 2.0f * sh : 1.0f, w = sh < 0.5f ? 0.5f : 0.5f - 0.8f * (sh - 0.5f);   /* 0.5 .. 0.1 */
        float sw = sh > 0.5f ? 0.03f + 0.18f * (sh - 0.5f) : 0.03f * a, pv;   /* the sway: slight at the square (it
                                                                          * stays hollow), deeper toward the pulse */
        for (j = 0; j < 3; j++) {
            v->cdt[j] = f * (j ? 1.0f : P.ch_interval) *
                        fm_exp2f((SIDE[j] * dc + m * 7.0f * v->ou_c[j]) * (1.0f / 1200.0f)) * BR_INV_FS;
            v->cpw[j] += PWR[j] * (float)MAXN * BR_INV_FS;
            v->cpw[j] -= (float)(int32_t)v->cpw[j];
            v->cw[j] = br_clampf(w + sw * fm_sinf(6.2831853f * v->cpw[j]) + 0.1f * m * v->ou_c[j] * a, 0.04f, 0.96f);
        }
        v->csh = a;
        ce = br_clampf(P.cutoff + bright * 0.6f + prs * 0.5f + m * 0.2f * v->ou_cut, 0.0f, 1.0f);
        hc = 1.0f + 39.0f * ce * ce;
        fc = br_clampf(f * hc, 20.0f, 16000.0f);
        v->ca = 1.0f - fm_expf(-6.2831853f * fc * BR_INV_FS);
        pv = (1.0f + a * a) * (1.0f / 3.0f) - 2.0f * a * (1.0f / 3.0f - 2.0f * w * (1.0f - w));   /* a saw minus a of it
                                                                                             * w on: its power */
        v->cgain = fm_sqrtf((1.0f / 3.0f) / (pv > 0.05f ? pv : 0.05f));
        if (P.ch_reso > 0.002f || T.ch_type) {   /* the state-variable: Q 0.5 .. 20, the type's output, levelled
                                                  * (TYPE, a choice, from T: P glides only the floats) */
            float g = fm_tanf(3.14159265f * fc * BR_INV_FS), q = 0.5f * fm_powf(40.0f, P.ch_reso), k = 1.0f / q;
            v->sa1 = 1.0f / (1.0f + g * (g + k));
            v->sa2 = g * v->sa1;
            v->sa3 = g * v->sa2;
            if (T.ch_type == 1) {        /* band: its peak at 1, made up for being narrow */
                v->sc0 = 0.0f;
                v->sc1 = k * (1.4f + 0.12f * fm_sqrtf(hc)) * fm_sqrtf(q);
                v->sc2 = 0.0f;
            } else if (T.ch_type == 2) { /* high: x - k band - low, made up for the low harmonics gone */
                float mk = br_clampf(fm_sqrtf(0.75f * hc), 1.0f, 4.0f);
                v->sc0 = mk;
                v->sc1 = -k * mk;
                v->sc2 = -mk;
            } else {
                v->sc0 = v->sc1 = 0.0f;
                v->sc2 = 1.0f;
            }
            v->cgain *= 1.0f / fm_sqrtf(1.0f + 0.22f * (q - 0.5f));   /* its peak, kept under the limit */
        }
    }

    /* PARTIELS: frequencies (stretched, drifting), amplitudes (tilt, odd, drift), levelled */
    if (P.partials > 0.001f) {
        pe = br_clampf(P.tilt - bright * 0.7f - prs * 0.45f, 0.0f, 1.0f);
        pw = 0.3f + 2.7f * pe;
        sx = 1.0f + 0.12f * P.stretch;
        np = 0;
        float fnp = 2.0f + 6.0f * P.fullness, top;   /* (FULLNESS: down to two partials, the top one fading in and
                                                      * out as it turns: no step) */
        uint32_t nmax = (uint32_t)fnp;
        top = fnp - (float)nmax;
        if (top > 0.001f)
            nmax++;
        else
            top = 1.0f;
        if (nmax > SHED_NP[shed & 3]) {
            nmax = SHED_NP[shed & 3];
            top = 1.0f;
        }
        for (j = 0; j < nmax; j++) {
            float fk = f * fm_exp2f(sx * LOG2K[j] + m * 5.0f * v->ou_pf[j] * (1.0f / 1200.0f)), w, a;
            if (fk > 15000.0f)
                break;
            w = 6.2831853f * fk * BR_INV_FS;
            v->pc[j] = fm_cosf(w);
            v->ps[j] = fm_sinf(w);
            a = fm_exp2f(-pw * LOG2K[j] + 1.6f * (m + 2.5f * P.pa_evolve) * v->ou_pa[j]);
            if (j & 1u)                  /* the even harmonics (2, 4 ...) */
                a *= 1.0f - P.odd;
            if (P.pa_peak > 0.001f) {    /* the formant: the ones near it up to 8 times, narrower and the rest down to
                                          * -30 dB as PEAK rises (at the top, that harmonic alone) */
                float pk = P.pa_peak, d = (sx * LOG2K[j] - P.pa_formant) * (1.5f + 4.0f * pk), fl = 1.0f - 0.97f * pk;
                a *= fl + (1.0f + 7.0f * pk - fl) / (1.0f + d * d);
            }
            if (j + 1u == nmax)
                a *= top;
            v->pa[j] = a;
            sum2 += a * a;
            np++;
        }
        v->np = np;
        norm = 0.6f / fm_sqrtf(sum2 + 1e-9f);
        for (j = 0; j < (uint32_t)np; j++) {
            float r2 = v->px[j] * v->px[j] + v->py[j] * v->py[j], g = 1.5f - 0.5f * r2;   /* back on the circle */
            v->pa[j] *= norm;
            v->px[j] *= g;
            v->py[j] *= g;
        }
        if (P.pa_beat > 0.001f) {        /* BEAT: each one pulsing at its own pace (0.15 .. 7 Hz), the deeper the quicker;
                                          * reached by the block's end, a sample at a time */
            static const float SPREAD_[NP] = {0.55f, 1.25f, 0.8f, 1.45f, 0.65f, 1.1f, 0.95f, 1.35f};
            float bt = P.pa_beat, dep = bt * 1.4f > 1.0f ? 1.0f : bt * 1.4f, hz = 0.15f + 5.0f * bt * bt;
            for (j = 0; j < (uint32_t)np; j++) {
                float gb;
                v->pbt[j] += hz * SPREAD_[j] * (float)MAXN * BR_INV_FS;
                v->pbt[j] -= (float)(int32_t)v->pbt[j];
                gb = (1.0f - dep * (0.5f + 0.5f * fm_sinf(6.2831853f * v->pbt[j]))) / (1.0f - 0.5f * dep);   /* (as loud
                                                                                                   * on average) */
                v->pa[j] *= gb;
                v->pda[j] = (v->pa[j] - v->pac[j]) * (1.0f / (float)MAXN);
            }
        } else {
            for (j = 0; j < (uint32_t)np; j++) {
                v->pac[j] = v->pa[j];
                v->pda[j] = 0.0f;
            }
        }
    }

    /* SOUFFLE: the resonators at the note x ratios (COLOR between the sets), their bandwidth from Q, and each one's
     * gain from its noise gain (so the loudness follows neither Q nor pitch): y = g x + b1 y1 - b2 y2 */
    if (P.souffle > 0.001f) {
        col = br_clampf(P.color, 0.0f, 1.0f) * 2.0f;
        ra = RATIO_L2[col < 1.0f ? 0 : 1];
        rb = RATIO_L2[col < 1.0f ? 1 : 2];
        col -= col < 1.0f ? 0.0f : 1.0f;
        q = 2.0f * fm_powf(300.0f, br_clampf(P.resonance + m * 0.1f * v->ou_r[0], 0.0f, 1.0f));
        sb = br_clampf(0.55f + bright * 0.8f + prs * 0.5f, 0.05f, 1.0f);
        for (j = 0; j < NR; j++) {
            float fr = f * P.br_pitch * fm_exp2f(br_lerp(ra[j], rb[j], col) + m * 12.0f * v->ou_r[j] * (1.0f / 1200.0f));
            float r, b1, b2, ng;
            fr = br_clampf(fr, 20.0f, 16000.0f);
            r = fm_expf(-3.14159265f * fr / q * BR_INV_FS);
            b1 = 2.0f * r * fm_cosf(6.2831853f * fr * BR_INV_FS);
            b2 = r * r;
            ng = (1.0f + b2) / ((1.0f - b2) * ((1.0f + b2) * (1.0f + b2) - b1 * b1));   /* the sum of h^2 */
            v->rb1[j] = b1;
            v->rb2[j] = b2;
            v->rg[j] = 0.4f * fm_sqrtf(3.0f / ng) * (j ? sb * (j == 2u ? sb : 1.0f) : 1.0f);   /* noise: var 1/3 */
        }
    }

    /* its place in the field */
    pan = br_clampf((v->pan + m * 0.4f * v->ou_pan) * P.spread * (0.3f + 0.7f * P.fullness), -1.0f, 1.0f);
    v->gl = fm_sqrtf(0.5f * (1.0f - pan));
    v->gr = fm_sqrtf(0.5f * (1.0f + pan));
}

/* ---------------------------------------------------- a voice's chunk --- */
static inline float blep(float t, float dt)
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

/* the layers write three buses: the centre (c), one side (a) and the other (b); SPREAD places the sides.
 * CHOEUR's saws (with SHAPE: each minus its share of itself cw on), then the two one-poles, or the state-variable
 * (RESONANCE; its states the band's and the low's): four loops, only what is used costs */
static inline __attribute__((always_inline)) void choir_run(voice_t *v, float *ba, float *bb, float g, const int sq,
                                                            const int svf)
{
    float p0 = v->cph[0], p1 = v->cph[1], p2 = v->cph[2], d0 = v->cdt[0], d1 = v->cdt[1], d2 = v->cdt[2];
    float a = v->ca, l1 = v->clp[0], l2 = v->clp[1], r1 = v->clp[2], r2 = v->clp[3];
    float a1 = v->sa1, a2 = v->sa2, a3 = v->sa3, sh = v->csh, w0 = v->cw[0], w1 = v->cw[1], w2 = v->cw[2];
    float o0 = v->sc0, o1 = v->sc1, o2 = v->sc2;
    uint32_t i;
    for (i = 0; i < CH; i++) {
        float s0, s1, s2;
        p0 += d0;
        p1 += d1;
        p2 += d2;
        if (p0 >= 1.0f)
            p0 -= 1.0f;
        if (p1 >= 1.0f)
            p1 -= 1.0f;
        if (p2 >= 1.0f)
            p2 -= 1.0f;
        s0 = 2.0f * p0 - 1.0f - blep(p0, d0);
        s1 = 2.0f * p1 - 1.0f - blep(p1, d1);
        s2 = 2.0f * p2 - 1.0f - blep(p2, d2);
        if (sq) {
            float q0 = p0 + w0, q1 = p1 + w1, q2 = p2 + w2;
            if (q0 >= 1.0f)
                q0 -= 1.0f;
            if (q1 >= 1.0f)
                q1 -= 1.0f;
            if (q2 >= 1.0f)
                q2 -= 1.0f;
            s0 -= sh * (2.0f * q0 - 1.0f - blep(q0, d0));
            s1 -= sh * (2.0f * q1 - 1.0f - blep(q1, d1));
            s2 -= sh * (2.0f * q2 - 1.0f - blep(q2, d2));
        }
        if (svf) {                       /* (o0, o1, o2: the type's mix of the input, the band and the low) */
            float x0 = s0 + s1, x = x0 - l2, t1 = a1 * l1 + a2 * x, t2 = l2 + a2 * l1 + a3 * x;
            l1 = 2.0f * t1 - l1;
            l2 = 2.0f * t2 - l2;
            ba[i] += (o0 * x0 + o1 * t1 + o2 * t2) * g;
            x0 = s0 + s2;
            x = x0 - r2;
            t1 = a1 * r1 + a2 * x;
            t2 = r2 + a2 * r1 + a3 * x;
            r1 = 2.0f * t1 - r1;
            r2 = 2.0f * t2 - r2;
            bb[i] += (o0 * x0 + o1 * t1 + o2 * t2) * g;
        } else {
            l1 += a * (s0 + s1 - l1);
            l2 += a * (l1 - l2);
            r1 += a * (s0 + s2 - r1);
            r2 += a * (r1 - r2);
            ba[i] += l2 * g;
            bb[i] += r2 * g;
        }
    }
    v->cph[0] = p0;
    v->cph[1] = p1;
    v->cph[2] = p2;
    v->clp[0] = l1;
    v->clp[1] = l2;
    v->clp[2] = r1;
    v->clp[3] = r2;
}

static void choir_chunk(voice_t *v, float *ba, float *bb, float g)
{
    int svf = P.ch_reso > 0.002f || T.ch_type, sq = P.ch_shape > 0.002f;
    if (svf != v->csvf) {                /* the other filter: the low-pass's state carried over */
        v->clp[0] = svf ? 0.0f : v->clp[1];
        v->clp[2] = svf ? 0.0f : v->clp[3];
        v->csvf = (uint8_t)svf;
    }
    g *= v->cgain;
    if (sq && svf)
        choir_run(v, ba, bb, g, 1, 1);
    else if (sq)
        choir_run(v, ba, bb, g, 1, 0);
    else if (svf)
        choir_run(v, ba, bb, g, 0, 1);
    else
        choir_run(v, ba, bb, g, 0, 0);
}

/* partials j and k (k < 0: j alone) into one bus, two at a time: their states stay in registers. With BEAT their
 * levels ramp (pac, pda) */
static __attribute__((noinline)) void partial_ramp(voice_t *v, uint32_t j, int k, float *b, float g)
{
    float x1 = v->px[j], y1 = v->py[j], c1 = v->pc[j], s1 = v->ps[j], a1 = v->pac[j] * g, d1 = v->pda[j] * g;
    float x2 = 0.0f, y2 = 0.0f, c2 = 0.0f, s2 = 0.0f, a2 = 0.0f, d2 = 0.0f;
    uint32_t i;
    if (k >= 0) {
        x2 = v->px[k];
        y2 = v->py[k];
        c2 = v->pc[k];
        s2 = v->ps[k];
        a2 = v->pac[k] * g;
        d2 = v->pda[k] * g;
    }
    for (i = 0; i < CH; i++) {
        float n1 = c1 * x1 - s1 * y1, n2 = c2 * x2 - s2 * y2;
        y1 = s1 * x1 + c1 * y1;
        y2 = s2 * x2 + c2 * y2;
        x1 = n1;
        x2 = n2;
        b[i] += a1 * x1 + a2 * x2;
        a1 += d1;
        a2 += d2;
    }
    v->px[j] = x1;
    v->py[j] = y1;
    v->pac[j] += v->pda[j] * (float)CH;
    if (k >= 0) {
        v->px[k] = x2;
        v->py[k] = y2;
        v->pac[k] += v->pda[k] * (float)CH;
    }
}

static void partial_pair(voice_t *v, uint32_t j, int k, float *b, float g)
{
    float x1 = v->px[j], y1 = v->py[j], c1 = v->pc[j], s1 = v->ps[j], a1 = v->pa[j] * g;
    uint32_t i;
    if (k < 0) {
        for (i = 0; i < CH; i++) {
            float n1 = c1 * x1 - s1 * y1;
            y1 = s1 * x1 + c1 * y1;
            x1 = n1;
            b[i] += a1 * x1;
        }
    } else {
        float x2 = v->px[k], y2 = v->py[k], c2 = v->pc[k], s2 = v->ps[k], a2 = v->pa[k] * g;
        for (i = 0; i < CH; i++) {
            float n1 = c1 * x1 - s1 * y1, n2 = c2 * x2 - s2 * y2;
            y1 = s1 * x1 + c1 * y1;
            y2 = s2 * x2 + c2 * y2;
            x1 = n1;
            x2 = n2;
            b[i] += a1 * x1 + a2 * x2;
        }
        v->px[k] = x2;
        v->py[k] = y2;
    }
    v->px[j] = x1;
    v->py[j] = y1;
}

/* the fundamental in the centre; harmonics 2, 4, 6, 8 to one side, 3, 5, 7 to the other */
static void partials_chunk(voice_t *v, float *bc, float *ba, float *bb, float g)
{
    int np = v->np;
    if (P.pa_beat > 0.001f) {            /* BEAT: the same pairs, their levels ramped */
        if (np > 0)
            partial_ramp(v, 0, -1, bc, g);
        if (np > 1)
            partial_ramp(v, 1, np > 3 ? 3 : -1, ba, g);
        if (np > 2)
            partial_ramp(v, 2, np > 4 ? 4 : -1, bb, g);
        if (np > 5)
            partial_ramp(v, 5, np > 7 ? 7 : -1, ba, g);
        if (np > 6)
            partial_ramp(v, 6, -1, bb, g);
        return;
    }
    if (np > 0)
        partial_pair(v, 0, -1, bc, g);
    if (np > 1)
        partial_pair(v, 1, np > 3 ? 3 : -1, ba, g);
    if (np > 2)
        partial_pair(v, 2, np > 4 ? 4 : -1, bb, g);
    if (np > 5)
        partial_pair(v, 5, np > 7 ? 7 : -1, ba, g);
    if (np > 6)
        partial_pair(v, 6, -1, bb, g);
}

static void souffle_chunk(voice_t *v, const float *nz, float *bc, float *ba, float *bb, float g)
{
    float *bus[NR] = {bc, ba, bb};
    uint32_t i, j = 0;
    if (P.br_air > 0.001f) {             /* AIR: the noise itself, with the first resonator, in the centre (as loud as
                                          * one at the top) */
        float y1 = v->r1[0], y2 = v->r2[0], b1 = v->rb1[0], b2 = v->rb2[0], gg = v->rg[0] * g, ga = P.br_air * 0.7f * g;
        for (i = 0; i < CH; i++) {
            float y = gg * nz[i] + b1 * y1 - b2 * y2;
            y2 = y1;
            y1 = y;
            bc[i] += y + ga * nz[i];
        }
        if (y1 > -1e-20f && y1 < 1e-20f && y2 > -1e-20f && y2 < 1e-20f)
            y1 = y2 = 0.0f;
        v->r1[0] = y1;
        v->r2[0] = y2;
        j = 1;
    }
    for (; j < NR; j++) {
        float y1 = v->r1[j], y2 = v->r2[j], b1 = v->rb1[j], b2 = v->rb2[j], gg = v->rg[j] * g, *b = bus[j];
        for (i = 0; i < CH; i++) {
            float y = gg * nz[i] + b1 * y1 - b2 * y2;
            y2 = y1;
            y1 = y;
            b[i] += y;
        }
        if (y1 > -1e-20f && y1 < 1e-20f && y2 > -1e-20f && y2 < 1e-20f)
            y1 = y2 = 0.0f;
        v->r1[j] = y1;
        v->r2[j] = y2;
    }
}

/* SOUFFLE's source for the block, the same for every voice: a hiss (white, variance 1/3), or GRAIN's grains, sparser
 * as it rises (from every sample to ~5 a second), as loud on average; TONE darker (a low-pass, 16 kHz .. 300 Hz) or
 * brighter (a high-pass, 20 Hz .. 3 kHz), its power kept */
static void breath_noise(uint32_t n)
{
    uint32_t i;
    if (P.br_grain < 0.002f) {
        for (i = 0; i < n; i++)
            noise[i] = br_randf(&rng);
    } else {
        float p = fm_exp2f(-13.0f * P.br_grain), amp = fm_sqrtf(1.0f / (3.0f * 0.5833f * p));   /* (x a grain's
                                                                                                 * mean square) */
        uint32_t th = (uint32_t)(p * 16777216.0f);
        if (amp > 16.0f)                 /* the sparsest a little quieter: no grain over the limit */
            amp = 16.0f;
        for (i = 0; i < n; i++) {
            uint32_t r = br_rand(&rng);
            float a = amp * (0.5f + (float)(r & 255u) * (0.5f / 255.0f));
            noise[i] = (r >> 8) < th ? (r & 256u ? a : -a) : 0.0f;
        }
    }
    if (P.br_tone < -0.01f || P.br_tone > 0.01f) {
        float t = P.br_tone, fc, a, gn;
        if (t < 0.0f) {
            fc = 16000.0f * fm_powf(300.0f / 16000.0f, -t);
            a = 1.0f - fm_expf(-6.2831853f * fc * BR_INV_FS);
            gn = fm_sqrtf((2.0f - a) / a);
            for (i = 0; i < n; i++) {
                nlp += a * (noise[i] - nlp);
                noise[i] = nlp * gn;
            }
        } else {
            fc = 20.0f * fm_powf(150.0f, t);
            a = 1.0f - fm_expf(-6.2831853f * fc * BR_INV_FS);
            gn = 1.0f / fm_sqrtf(1.0f + a / (2.0f - a) - 2.0f * a);
            for (i = 0; i < n; i++) {
                nlp += a * (noise[i] - nlp);
                noise[i] = (noise[i] - nlp) * gn;
            }
        }
        nlp = fm_flush(nlp);
    }
}

/* ---------------------------------------------------------------- render --- */
static void render_part(float *ol, float *or_, uint32_t n)
{
    float ka, kr, sl, sr;
    uint32_t i, c, j;
    if (pend_seen != pend_seq) {
        pend_seen = pend_seq;
        T = pend;
    }
    {
        float *pf = (float *)&P;
        const float *tf = (const float *)&T;
        for (i = 0; i < NGLIDE; i++)     /* ~25 ms */
            pf[i] += (tf[i] - pf[i]) * 0.2f;
    }
    events();
    drift(n);
    breath_noise(n);
    for (i = 0; i < n; i++)
        ol[i] = or_[i] = 0.0f;
    /* envelope coefficients a chunk: one-pole to the target, a third of the time a time constant */
    ka = 1.0f - fm_expf(-(float)CH * BR_INV_FS * 3.0f / (0.005f * fm_powf(6000.0f, P.attack)));
    kr = 1.0f - fm_expf(-(float)CH * BR_INV_FS * 3.0f / (0.05f * fm_powf(1200.0f, P.release)));
    {                                    /* shedding: no more voices sounding than the level allows */
        uint32_t on = 0, cap = SHED_NV[shed & 3];
        for (j = 0; j < NV; j++)
            on += vo[j].note >= 0;
        while (on > cap) {
            voice_t *x = 0;
            for (j = 0; j < NV; j++)     /* the quietest released one, else the oldest held one */
                if (vo[j].note >= 0 && !vo[j].gate && (!x || vo[j].env < x->env))
                    x = &vo[j];
            if (!x)
                for (j = 0; j < NV; j++)
                    if (vo[j].note >= 0 && (!x || vo[j].age < x->age))
                        x = &vo[j];
            x->note = -1;
            x->env = x->lvl = 0.0f;
            on--;
        }
    }
    sl = 0.5f + 0.4f * P.spread * (0.3f + 0.7f * P.fullness);   /* a side's share on its own side */
    sr = 1.0f - sl;
    for (j = 0; j < NV; j++) {
        voice_t *v = &vo[j];
        if (v->note < 0)
            continue;
        voice_block(v);
        for (c = 0; c < n; c += CH) {
            float bc[CH], ba[CH], bb[CH], g0 = v->lvl, g1, dg, lvl, al, ar, bl, br;
            v->env += ((v->gate ? 1.0f : 0.0f) - v->env) * (v->gate ? ka : kr);
            v->prs += (v->prs_t - v->prs) * 0.15f;
            lvl = (1.0f - P.velocity + P.velocity * v->vel) * (1.0f + 1.2f * P.pressure * br_clampf(v->prs + swell, 0.0f, 1.0f)) * VOICE_G;
            g1 = lvl * v->env * v->env;
            v->lvl = g1;
            dg = (g1 - g0) * (1.0f / (float)CH);
            for (i = 0; i < CH; i++)
                bc[i] = ba[i] = bb[i] = 0.0f;
            if (P.choir > 0.001f)        /* (FULLNESS: the choir and the breath down, the partials less so) */
                choir_chunk(v, ba, bb, 0.35f * P.choir * (0.3f + 0.7f * P.fullness));
            if (P.partials > 0.001f)
                partials_chunk(v, bc, ba, bb, P.partials * (0.7f + 0.3f * P.fullness));
            if (P.souffle > 0.001f)
                souffle_chunk(v, noise + c, bc, ba, bb, P.souffle * P.fullness * P.fullness);
            al = v->gl * sl;                 /* the sides into the field, then the voice's place */
            ar = v->gr * sr;
            bl = v->gl * sr;
            br = v->gr * sl;
            for (i = 0; i < CH; i++) {
                g0 += dg;
                ol[c + i] += (bc[i] * v->gl + ba[i] * al + bb[i] * bl) * g0;
                or_[c + i] += (bc[i] * v->gr + ba[i] * ar + bb[i] * br) * g0;
            }
        }
        if (!v->gate && v->env < 0.001f)
            v->note = -1;                /* faded out: free */
    }
}

/* ------------------------------------------------- PUMP, the mix, MASTERING --- */
static float bsl[MAXN], bsr[MAXN];       /* the bass, apart: its own level and duck */
static volatile float pump_sync_ph, pump_sync_inc;   /* frasil_pump: the beat as the main loop has it */
static volatile uint32_t pump_seq;
static uint32_t pump_seen;
static float pump_ph, pump_inc, pump_d;  /* the render's phase, its step a sample, the duck now (0..1) */
static float comp_env, comp_g = 1.0f;    /* the compressor: its detector, its gain now */
static float sk_l1, sk_l2, sk_r1, sk_r2; /* SINK's low-pass (state-variable, trapezoidal), each side */
static volatile float comp_gr;
static float meter[4];

void frasil_pump(float phase, float inc)
{
    pump_sync_ph = phase;
    pump_sync_inc = inc;
    __asm__ volatile("" ::: "memory");
    pump_seq++;
}

/* the duck at phase ph of the beat's period, as a sidechain's: down over ATTACK at OFFSET into the period, back up
 * over RELEASE's share of it */
static float pump_duck(float ph, float inc)
{
    float t, r;
    if (inc <= 0.0f)
        return 0.0f;
    ph -= P.pump_off;
    ph -= fm_floorf(ph);
    t = ph / (inc * BR_FS);
    r = 1.0f - ph / (P.pump_rel > 0.1f ? P.pump_rel : 0.1f);
    if (r <= 0.0f)
        return 0.0f;
    return (t < P.pump_att ? t / (P.pump_att + 1e-6f) : 1.0f) * r * r * fm_sqrtf(r);
}

void frasil_render(int32_t *lr, uint32_t n, float gain)
{
    uint32_t off = 0;
    while (off < n) {
        uint32_t m = n - off > MAXN ? MAXN : n - off, i, c;
        float wd, ww, dk, dmake, dg, ka, kr, pk[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        int comp = P.comp_ratio > 1.001f || P.comp_gain > 1.001f;   /* (both glide: on and off without a step) */
        int thin = P.fullness < 0.995f;
        float ha = 1.0f - fm_expf(-6.2831853f * 20.0f * fm_powf(20.0f, 1.0f - P.fullness) * BR_INV_FS);
        int sink = P.sink > 0.0005f;
        float sa1 = 0.0f, sa2 = 0.0f, sa3 = 0.0f, sw = 0.0f;
        if (sink) {                      /* SINK: 12 kHz down to some 120 Hz (the octaves quicker at first: 2.4 kHz at
                                          * a quarter, 800 Hz halfway), a little peak as it goes under; in over its first
                                          * 4 % (no step as it starts) */
            float g = fm_tanf(3.14159265f * 12000.0f * fm_exp2f(-6.6f * fm_powf(P.sink, 0.75f)) * BR_INV_FS);
            float k = 1.0f / (0.75f + 0.5f * P.sink);
            sa1 = 1.0f / (1.0f + g * (g + k));
            sa2 = g * sa1;
            sa3 = g * sa2;
            sw = P.sink * 25.0f > 1.0f ? 1.0f : P.sink * 25.0f;
        } else {
            sk_l1 = sk_l2 = sk_r1 = sk_r2 = 0.0f;
        }
        render_part(mixl, mixr, m);
        /* the mix: DC out, warmth, the drones' level (before their share into the reverb) */
        dk = 1.0f + 5.0f * P.drive * P.fullness;
        dmake = 1.0f / fm_tanhf(dk * 0.35f) * 0.35f;
        for (i = 0; i < m; i++) {
            float xl = mixl[i], xr = mixr[i];
            dcl += 0.0014f * (xl - dcl);
            dcr += 0.0014f * (xr - dcr);
            xl -= dcl;
            xr -= dcr;
            if (P.drive * P.fullness > 0.001f) {
                xl = fm_tanhf(xl * dk) * dmake;
                xr = fm_tanhf(xr * dk) * dmake;
            }
            if (thin) {                  /* FULLNESS down: the body high-passed, 20 Hz .. 400 Hz */
                thl += ha * (xl - thl);
                thr += ha * (xr - thr);
                xl -= thl;
                xr -= thr;
            } else {
                thl = thr = 0.0f;        /* (from nothing the next time) */
            }
            mixl[i] = rinl[i] = xl * P.mx_drone;
            mixr[i] = rinr[i] = xr * P.mx_drone;
            spl[i] = spr[i] = bsl[i] = bsr[i] = 0.0f;
        }
        {                                /* the glints at their level: dry beside the drones, their share into the
                                          * reverb, their echoes on them alone */
            glint_params_t gp = T.glint;
            gp.level *= P.mx_glint;
            glint_render(spl, spr, rinl, rinr, m, &gp);
            glint_delay(spl, spr, rinl, rinr, m, &gp);
        }
        bass_render(bsl, bsr, m, &T.bass);   /* the bass: none of it in the reverb */
        /* the reverb, and the mix of dry and wet (equal power: SPACE), the cloud's return at its level */
        nuage_process(rinl, rinr, wetl, wetr, m);
        wd = fm_cosf(P.mix * 1.5707963f);
        ww = fm_sinf(P.mix * 1.5707963f) * 1.2f * P.mx_cloud;
        dg = (gain - gain_now) * (1.0f / (float)m);
        /* PUMP: the beat's phase, following the main loop's (a nudge, or a jump when far) */
        if (pump_seen != pump_seq) {
            float e;
            pump_seen = pump_seq;
            pump_inc = pump_sync_inc;
            e = pump_sync_ph - pump_ph;
            e -= fm_floorf(e + 0.5f);
            pump_ph = fm_fabsf(e) > 0.2f ? pump_sync_ph : pump_ph + e * 0.05f;
            pump_ph -= fm_floorf(pump_ph);
        }
        /* the compressor's detector: 10 ms up, RELEASE down; its gain from it a chunk at a time */
        ka = 1.0f - fm_expf(-1.0f / (0.01f * BR_FS));
        kr = 1.0f - fm_expf(-1.0f / (P.comp_rel * BR_FS + 1.0f));
        for (c = 0; c < m; c += 32u) {
            float d1, dd, d = pump_d, g1 = 1.0f, cg, dcg, slope = 1.0f - 1.0f / (comp ? P.comp_ratio : 1.0f);
            uint32_t e = c + 32u > m ? m : c + 32u;
            pump_ph += pump_inc * (float)(e - c);
            pump_ph -= fm_floorf(pump_ph);
            d1 = pump_duck(pump_ph, pump_inc);
            {                            /* down no faster than ATTACK (a jump of the phase, RATE turned on) */
                float most = (float)(e - c) * BR_INV_FS / (P.pump_att > 0.001f ? P.pump_att : 0.001f);
                if (d1 > d + most)
                    d1 = d + most;
            }
            dd = (d1 - d) / (float)(e - c);
            pump_d = d1;
            if (comp) {
                float over = 6.0206f * fm_log2f(comp_env + 1e-9f) - P.comp_thr;
                g1 = over > 0.0f ? fm_exp2f(-over * slope * 0.166096405f) : 1.0f;
                comp_gr = over > 0.0f ? -over * slope : 0.0f;
            } else {
                comp_gr = 0.0f;
            }
            cg = comp_g;
            dcg = (g1 - cg) / (float)(e - c);
            comp_g = g1;
            for (i = c; i < e; i++) {
                float gdr = 1.0f - 0.95f * P.pump_drone * d, gb = (1.0f - 0.95f * P.pump_bass * d) * P.mx_bass;
                float gg = 1.0f - 0.95f * P.pump_glint * d, dl, dr, wl, wr, l, r, a;
                d += dd;
                cg += dcg;
                dl = mixl[i] * wd * gdr;
                dr = mixr[i] * wd * gdr;
                wl = wetl[i] * ww * gdr;
                wr = wetr[i] * ww * gdr;
                l = dl + wl + spl[i] * gg + bsl[i] * gb;
                r = dr + wr + spr[i] * gg + bsr[i] * gb;
                if (!(i & 3u)) {         /* the meters: one sample in four is enough for the screen */
                    a = fm_fabsf(dl) + fm_fabsf(dr);
                    pk[0] = a > pk[0] ? a : pk[0];
                    a = fm_fabsf(bsl[i] * gb);
                    pk[1] = a > pk[1] ? a : pk[1];
                    a = (fm_fabsf(spl[i]) + fm_fabsf(spr[i])) * gg;
                    pk[2] = a > pk[2] ? a : pk[2];
                    a = fm_fabsf(wl) + fm_fabsf(wr);
                    pk[3] = a > pk[3] ? a : pk[3];
                }
                if (sink) {
                    float v1, v2, v3 = l - sk_l2;
                    v1 = sa1 * sk_l1 + sa2 * v3;
                    v2 = sk_l2 + sa2 * sk_l1 + sa3 * v3;
                    sk_l1 = 2.0f * v1 - sk_l1;
                    sk_l2 = 2.0f * v2 - sk_l2;
                    l += (v2 - l) * sw;
                    v3 = r - sk_r2;
                    v1 = sa1 * sk_r1 + sa2 * v3;
                    v2 = sk_r2 + sa2 * sk_r1 + sa3 * v3;
                    sk_r1 = 2.0f * v1 - sk_r1;
                    sk_r2 = 2.0f * v2 - sk_r2;
                    r += (v2 - r) * sw;
                }
                if (comp) {              /* MASTERING: the detector, then the gain and the make-up */
                    a = fm_fabsf(l) > fm_fabsf(r) ? fm_fabsf(l) : fm_fabsf(r);
                    comp_env += (a - comp_env) * (a > comp_env ? ka : kr);
                    l *= cg * P.comp_gain;
                    r *= cg * P.comp_gain;
                }
                if (ck_env > 0.0005f) {  /* the metronome's tick, over everything (no cloud, no SINK, no meter), some
                                          * 15 ms: a 3 ms fall */
                    a = fm_sin_turns(ck_ph) * ck_env;
                    ck_ph += ck_inc;
                    ck_ph -= ck_ph >= 1.0f ? 1.0f : 0.0f;
                    ck_env *= 0.9925f;
                    l += a;
                    r += a;
                }
                l = br_soft(l);
                r = br_soft(r);
                a = fm_fabsf(l) > fm_fabsf(r) ? fm_fabsf(l) : fm_fabsf(r);
                peak = a > peak ? a : peak;
                gain_now += dg;
                lr[2u * (off + i)] = (int32_t)(l * gain_now * 8300000.0f);
                lr[2u * (off + i) + 1u] = (int32_t)(r * gain_now * 8300000.0f);
            }
        }
        if (!comp)
            comp_env = 0.0f;
        comp_env = fm_flush(comp_env);
        thl = fm_flush(thl);
        thr = fm_flush(thr);
        sk_l1 = fm_flush(sk_l1);
        sk_l2 = fm_flush(sk_l2);
        sk_r1 = fm_flush(sk_r1);
        sk_r2 = fm_flush(sk_r2);
        pk[0] *= 0.5f;                   /* (the stereo ones: the sum of the sides, halved) */
        pk[2] *= 0.5f;
        pk[3] *= 0.5f;
        for (i = 0; i < 4u; i++)
            meter[i] = pk[i] > meter[i] * 0.94f ? pk[i] : meter[i] * 0.94f;
        peak *= 0.97f;
        off += m;
    }
}

/* ------------------------------------------------------------- the rest --- */
void frasil_init(void)
{
    uint32_t i, j;
    for (i = 0; i < NV; i++)
        vo[i].note = -1;
    for (i = 0; i < 3; i++)
        for (j = 0; j < NR; j++)
            RATIO_L2[i][j] = fm_log2f(RATIOS[i][j]);
    nuage_init();
    glint_init();
    bass_init();
}

void frasil_set(const frasil_params_t *p)
{
    pend = *p;
    __asm__ volatile("" ::: "memory");
    pend_seq++;
    nuage_set(&p->rev);
}

void frasil_panic(void)
{
    uint32_t i;
    for (i = 0; i < NV; i++) {
        vo[i].note = -1;
        vo[i].env = vo[i].lvl = 0.0f;
    }
    bass_init();
    ev_r = ev_w;
}

void frasil_view(frasil_view_t v[FR_NV])
{
    uint32_t i;
    for (i = 0; i < NV; i++) {
        v[i].note = vo[i].note;
        v[i].gate = vo[i].gate;
        v[i].level = vo[i].env;
        v[i].pressure = vo[i].prs;
    }
}

void frasil_swell(float s) { swell = br_clampf(s, 0.0f, 1.0f); }
void frasil_shed(int level) { shed = level < 0 ? 0 : level > 3 ? 3 : level; }
int frasil_shed_level(void) { return shed; }
float frasil_tide(void) { return tide_v; }
void frasil_glint_view(glint_view_t v[GLINT_NV]) { glint_view(v); }
void frasil_bass_view(bass_view_t *v) { bass_view(v); }
void frasil_meters(float m[4])
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        m[i] = meter[i] > 1.0f ? 1.0f : meter[i];
}
float frasil_pump_gain(void) { return 1.0f - 0.95f * P.pump_drone * pump_d; }
float frasil_comp_gr(void) { return comp_gr; }
float frasil_out_level(void) { return peak; }
