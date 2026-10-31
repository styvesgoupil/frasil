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
 * PARTIELS 8 partials as rotating phasors (no table, no phase: a "magic circle", x -= e y, y += e x, the phasor's
 *          angle the same a block at a time), the fundamental in the
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
#define TAIL_END 0.01f                   /* a voice let go ends here: its gain (env squared) 80 dB down (2.5: was
                                          * 0.001, 120 dB; the long releases kept voices sounding nobody hears) */

/* ---------------------------------------------------------------- events --- */
enum { EV_ON = 1, EV_OFF, EV_ALLOFF, EV_PRS, EV_SPON, EV_SPOFF, EV_SPPRS, EV_BON, EV_BOFF, EV_BPRS, EV_BPUL, EV_CLICK,
       EV_BRET };
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
void frasil_bass_retune(int note, float vel) { ev_push(EV_BRET, note, vel); }
void frasil_click(int accent) { ev_push(EV_CLICK, 0, accent ? 1.0f : 0.0f); }

/* ---------------------------------------------------------------- voices --- */
typedef struct {
    int8_t note;                         /* -1: free */
    uint8_t gate;
    uint8_t shed;                        /* let go by the shedding: out in some 15 ms, not cut (no click) */
    uint8_t wait;                        /* chunks before it sounds: room being made for it (make_room) */
    uint32_t age;                        /* note-on order (stealing) */
    float vel, prs, prs_t;
    float env;                           /* 0..1; the amplitude is its square */
    float lvl;                           /* the gain at the end of the last chunk (the next ramps from it) */
    float pan;                           /* -1..1 */
    /* drift, each about -1..1 */
    float ou_c[3], ou_cut, ou_pa[NP], ou_pf[NP], ou_r[NR], ou_pan;
    /* CHOEUR */
    float cph[3], cdt[3], cid[3];        /* phases, increments, the increments' inverses */
    float cw[3];                         /* SHAPE: how far on the saw taken off it is (the pulse's width) */
    float cpw[3];                        /* the widths' sway, a turn each */
    float csh;                           /* SHAPE: how much of the saw is taken off, 0..1 */
    float sc0, sc1, sc2;                 /* the state-variable's output: its input, band and low parts */
    float sp0, sp1, sp2;                 /* the ones before (a chunk's glide to sc: TYPE turned, no step) */
    float clp[4];                        /* left two stages, right two (the state-variable's: band, low) */
    float ca;                            /* one-pole coefficient */
    float sa1, sa2, sa3;                 /* the state-variable's (RESONANCE) */
    float cgain;                         /* SHAPE's and RESONANCE's level made up */
    float cint[6];                       /* SOFT: the two integrators, the centre saw's (at its own pitch), then the
                                          * left's, then the right's */
    uint8_t csvf;                        /* the filter running: the state-variable (1) or the one-poles */
    /* PARTIELS */
    float px[NP], py[NP], pa[NP];        /* each partial: its oscillator (x the cosine, y the sine half a step on) */
    float ps[NP], pc[NP];                /* and its half step's sine, its cosine's inverse (1 / cos: from y to the sine) */
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
static uint32_t (*prof_clk)(void);       /* frasil_profile: the clock, each part's count, the samples, the part now */
static volatile uint32_t prof_acc[FR_PROF_N], prof_n;
static uint32_t prof_t;
static int prof_at;
static float np_cap = (float)NP;        /* the partials the shedding allows, gliding */
/* foresight (frasil_budget: the FM-1 only): a render's cost as it goes, its ticks a sample without the drone voices and
 * a voice's (each render's, smoothed), so a voice to come is weighed before it sounds: past ROOM of the time, room is
 * made first (make_room). 2.5.1, the user: a chord taken while the last one rang out glitched a quarter second, the
 * shedding stepping in only after renders had run over */
#define ROOM 0.80f
#define ROOM_WAIT 11u                    /* chunks: a voice let go by the shedding is out (env 0.35 a chunk, TAIL_END) */
static float bud_tps, cost_base, cost_voice;
static uint32_t rv_ticks, rv_vs;         /* this render's: the voices' ticks, and voices times samples */
static volatile float swell;             /* frasil_swell */
static const uint8_t SHED_NP[6] = {NP, 6, 5, 4, 3, 3}, SHED_NV[6] = {NV, NV, 7, 6, 5, 4};   /* (4 and 5: a heavy
                                                                                         * preset with the effects) */
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
    v->shed = 0;
    v->vel = vel;
    v->age = ++ages;
    if (!fresh)
        return;
    v->env = 0.0f;
    v->lvl = 0.0f;
    v->wait = 0;
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
    for (k = 0; k < 6; k++)
        v->cint[k] = 0.0f;
    v->csvf = 0;
    for (k = 0; k < NP; k++) {           /* random phases: no peak where they all start together */
        float ph = (float)(br_rand(&rng) >> 8) * (6.2831853f / 16777216.0f);
        v->px[k] = fm_cosf(ph);
        v->py[k] = fm_sinf(ph);          /* (the half step 0: the phasor itself) */
        v->ps[k] = 0.0f;
        v->pc[k] = 1.0f;
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

/* a fresh voice to come (nv): if the render would then run past ROOM, the quietest released voices are let go as the
 * shedding lets them (some 15 ms), and nv waits as long, silent: the old chord's tail shorter, never a glitch. With
 * room, nothing changes */
static void make_room(voice_t *nv)
{
    uint32_t j, s = 0, fading = 0;
    if (bud_tps <= 0.0f || cost_voice <= 0.0f)
        return;
    for (j = 0; j < NV; j++)
        if (vo[j].note >= 0 && &vo[j] != nv) {
            if (vo[j].shed)
                fading = 1;
            else
                s++;
        }
    while (s && cost_base + (float)(s + 1u) * cost_voice > ROOM * bud_tps) {
        voice_t *x = 0;
        for (j = 0; j < NV; j++)
            if (&vo[j] != nv && vo[j].note >= 0 && !vo[j].shed && !vo[j].gate && (!x || vo[j].env < x->env))
                x = &vo[j];
        if (!x)
            break;
        x->shed = 1;
        s--;
        fading = 1;
    }
    if (fading)
        nv->wait = (uint8_t)ROOM_WAIT;
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
        make_room(v);
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
            bass_on(e.note, e.v, &T.bass);
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
        case EV_BRET:
            bass_retune(e.note, e.v);
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

/* the time since the last mark to the part before it; from here, part's (no clock: nothing) */
static inline void prof(int part)
{
    if (prof_clk) {
        uint32_t t = prof_clk();
        prof_acc[prof_at] += t - prof_t;
        prof_t = t;
        prof_at = part;
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
            v->cid[j] = 1.0f / v->cdt[j];
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
        uint32_t nmax;
        if (fnp > np_cap)                /* (the shedding's cap, gliding: the top one fading the same way) */
            fnp = np_cap;
        nmax = (uint32_t)fnp;
        top = fnp - (float)nmax;
        if (top > 0.001f)
            nmax++;
        else
            top = 1.0f;
        for (j = 0; j < nmax; j++) {
            float fk = f * fm_exp2f(sx * LOG2K[j] + m * 5.0f * v->ou_pf[j] * (1.0f / 1200.0f)), w, a;
            if (fk > 15000.0f)
                break;
            w = 6.2831853f * fk * BR_INV_FS;
            {                            /* the oscillator: back to its phasor (the sine from y, the last step's half),
                                          * on the circle, then on with this block's step, the angle of the rotation
                                          * (c, s) it was (fastmath's: its pitch the same), halved */
                float c = fm_cosf(w), sn = fm_sinf(w), r = fm_sqrtf(c * c + sn * sn), q = fm_sqrtf(2.0f * r * (r + c));
                float x = v->px[j], y = (v->py[j] - x * v->ps[j]) * v->pc[j], gn = 1.5f - 0.5f * (x * x + y * y);
                float sh = sn / q, ch = (r + c) / q;
                x *= gn;
                y *= gn;
                v->ps[j] = sh;
                v->pc[j] = q / (r + c);
                v->px[j] = x;
                v->py[j] = y * ch + x * sh;
            }
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
        for (j = 0; j < (uint32_t)np; j++)
            v->pa[j] *= norm;
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
/* how many samples on (m at most) a saw at p, stepping d (id its inverse), goes without nearing its wrap: no blep
 * there (CH_EPS: room for the float's rounding over the steps) */
#define CH_EPS 1e-5f
static inline uint32_t ch_run(float p, float d, float id, uint32_t m)
{
    float r = (1.0f - CH_EPS - d - p) * id;
    return r <= 0.0f ? 0u : r < (float)m ? (uint32_t)r : m;
}

static float ch_x0[CH], ch_x1[CH];       /* a chunk's inputs, the left's and the right's (choir_run) */
static float ch_xc[CH];                  /* with SOFT: the centre saw's alone (the sides then without it) */

/* one side's state-variable (RESONANCE) over a chunk of its input x (its saws' sum): the filter (c its a1 a2 a3, s its
 * band's and low's states), out at the type's mix o (of the input, the band and the low, the gain in it) gliding e a
 * sample (TYPE turned), into b. Band or low alone (TYPE as it stays), the terms at 0 left out: their products were
 * zeros, the sums the same. A side at a time, a function of its own: on the FM-1 the FPU's registers are the CPU's
 * sixteen, and both sides at once spilled some twenty a sample */
static __attribute__((noinline)) void ch_svf(float *b, const float *x, const float *c, float *s, const float *o,
                                             const float *e)
{
    float a1 = c[0], a2 = c[1], a3 = c[2], s1 = s[0], s2 = s[1], o0 = o[0], o1 = o[1], o2 = o[2];
    uint32_t i;
    if (e[0] != 0.0f || e[1] != 0.0f || e[2] != 0.0f) {
        float e0 = e[0], e1 = e[1], e2 = e[2];
        for (i = 0; i < CH; i++) {
            float v = x[i] - s2, t1 = a1 * s1 + a2 * v, t2 = s2 + a2 * s1 + a3 * v;
            s1 = 2.0f * t1 - s1;
            s2 = 2.0f * t2 - s2;
            b[i] += o0 * x[i] + o1 * t1 + o2 * t2;
            o0 += e0;
            o1 += e1;
            o2 += e2;
        }
    } else if (o0 == 0.0f && o2 == 0.0f) {   /* (band) */
        for (i = 0; i < CH; i++) {
            float v = x[i] - s2, t1 = a1 * s1 + a2 * v, t2 = s2 + a2 * s1 + a3 * v;
            s1 = 2.0f * t1 - s1;
            s2 = 2.0f * t2 - s2;
            b[i] += o1 * t1;
        }
    } else if (o0 == 0.0f && o1 == 0.0f) {   /* (low) */
        for (i = 0; i < CH; i++) {
            float v = x[i] - s2, t1 = a1 * s1 + a2 * v, t2 = s2 + a2 * s1 + a3 * v;
            s1 = 2.0f * t1 - s1;
            s2 = 2.0f * t2 - s2;
            b[i] += o2 * t2;
        }
    } else {
        for (i = 0; i < CH; i++) {
            float v = x[i] - s2, t1 = a1 * s1 + a2 * v, t2 = s2 + a2 * s1 + a3 * v;
            s1 = 2.0f * t1 - s1;
            s2 = 2.0f * t2 - s2;
            b[i] += o0 * x[i] + o1 * t1 + o2 * t2;
        }
    }
    s[0] = s1;
    s[1] = s2;
}

/* the band or the low alone (TYPE as it stays), both sides at once: their chains side by side (on the FM-1 an FPU op
 * waits for the one it needs, some 17 ns, where two independent ones take 8.5: two filters at once hide it); ch_svf's
 * sums, bit for bit */
static __attribute__((noinline)) void ch_svf2(float *ba, float *bb, const float *c, float *s, float o, int low)
{
    float a1 = c[0], a2 = c[1], a3 = c[2], l1 = s[0], l2 = s[1], r1 = s[2], r2 = s[3];
    uint32_t i;
    if (low)
        for (i = 0; i < CH; i++) {
            float vl = ch_x0[i] - l2, vr = ch_x1[i] - r2;
            float tl1 = a1 * l1 + a2 * vl, tr1 = a1 * r1 + a2 * vr;
            float tl2 = l2 + a2 * l1 + a3 * vl, tr2 = r2 + a2 * r1 + a3 * vr;
            l1 = 2.0f * tl1 - l1;
            r1 = 2.0f * tr1 - r1;
            l2 = 2.0f * tl2 - l2;
            r2 = 2.0f * tr2 - r2;
            ba[i] += o * tl2;
            bb[i] += o * tr2;
        }
    else
        for (i = 0; i < CH; i++) {
            float vl = ch_x0[i] - l2, vr = ch_x1[i] - r2;
            float tl1 = a1 * l1 + a2 * vl, tr1 = a1 * r1 + a2 * vr;
            float tl2 = l2 + a2 * l1 + a3 * vl, tr2 = r2 + a2 * r1 + a3 * vr;
            l1 = 2.0f * tl1 - l1;
            r1 = 2.0f * tr1 - r1;
            l2 = 2.0f * tl2 - l2;
            r2 = 2.0f * tr2 - r2;
            ba[i] += o * tl1;
            bb[i] += o * tr1;
        }
    s[0] = l1;
    s[1] = l2;
    s[2] = r1;
    s[3] = r2;
}

/* the low-passes over a chunk, two one-poles a side (s: the left's two states, the right's), both sides at once: the
 * inputs x0 and x1 into ba and bb at g */
static __attribute__((noinline)) void ch_lp(float *ba, float *bb, const float *x0, const float *x1, float a, float g,
                                            float *s)
{
    float l1 = s[0], l2 = s[1], r1 = s[2], r2 = s[3];
    uint32_t i;
    for (i = 0; i < CH; i++) {
        l1 += a * (x0[i] - l1);
        l2 += a * (l1 - l2);
        r1 += a * (x1[i] - r1);
        r2 += a * (r1 - r2);
        ba[i] += l2 * g;
        bb[i] += r2 * g;
    }
    s[0] = l1;
    s[1] = l2;
    s[2] = r1;
    s[3] = r2;
}

/* a blep with the step's inverse (no division: the FM-1's is slow) */
static inline float blepi(float t, float dt, float idt)
{
    if (t < dt) {
        t *= idt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) * idt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/* the layers write three buses: the centre (c), one side (a) and the other (b); SPREAD places the sides.
 * CHOEUR's saws (with SHAPE: each minus its share of itself cw on), the left the centre's and the -detune's, the right
 * the centre's and the +detune's, then the two one-poles or the state-variable, a chunk at a time: the sides' inputs
 * first, then their filters. Between the saws' wraps (most samples) there is no blep to test, and a saw less its share
 * of itself cw on is the saw scaled plus a constant: each side's sum is A (p + p') + K. So the inputs go in runs: how
 * far each saw (and SHAPE's) is from its wrap (ch_run), that many samples so, then the sample at a wrap as it was (its
 * bleps), the next run's constants from there (2.5) */
static inline __attribute__((always_inline)) void choir_run(voice_t *v, float *ba, float *bb, float g, const int sq,
                                                            const int svf)
{
    float p0 = v->cph[0], p1 = v->cph[1], p2 = v->cph[2], d0 = v->cdt[0], d1 = v->cdt[1], d2 = v->cdt[2];
    float id0 = v->cid[0], id1 = v->cid[1], id2 = v->cid[2];
    float sh = v->csh, w0 = v->cw[0], w1 = v->cw[1], w2 = v->cw[2];
    const int soft = P.ch_soft > 0.002f;  /* (SOFT: the centre saw apart, integrated at its own pitch: INTERVAL's) */
    uint32_t i = 0;
    while (i < CH) {
        uint32_t m = ch_run(p0, d0, id0, CH - i), e;
        float A = 2.0f, k0 = -2.0f, k1 = -2.0f, b0 = -1.0f, b1 = -1.0f, b2 = -1.0f;   /* (2p - 1 each) */
        m = ch_run(p1, d1, id1, m);
        m = ch_run(p2, d2, id2, m);
        if (sq) {                        /* the saws taken off: where they are, and from there the constants */
            float q0 = p0 + w0, q1 = p1 + w1, q2 = p2 + w2, c0, c1, c2;
            c0 = q0 >= 1.0f ? 1.0f : 0.0f;
            c1 = q1 >= 1.0f ? 1.0f : 0.0f;
            c2 = q2 >= 1.0f ? 1.0f : 0.0f;
            q0 -= c0;
            q1 -= c1;
            q2 -= c2;
            if (q0 < CH_EPS || q1 < CH_EPS || q2 < CH_EPS)
                m = 0;                   /* (one just wrapped: that sample as it was) */
            m = ch_run(q0, d0, id0, m);
            m = ch_run(q1, d1, id1, m);
            m = ch_run(q2, d2, id2, m);
            A = 2.0f - 2.0f * sh;
            b0 = sh - 1.0f - 2.0f * sh * (w0 - c0);
            b1 = sh - 1.0f - 2.0f * sh * (w1 - c1);
            b2 = sh - 1.0f - 2.0f * sh * (w2 - c2);
            k0 = b0 + b1;
            k1 = b0 + b2;
        }
        if (soft)
            for (e = i + m; i < e; i++) {
                p0 += d0;
                p1 += d1;
                p2 += d2;
                ch_xc[i] = A * p0 + b0;
                ch_x0[i] = A * p1 + b1;
                ch_x1[i] = A * p2 + b2;
            }
        else
            for (e = i + m; i < e; i++) {
                p0 += d0;
                p1 += d1;
                p2 += d2;
                ch_x0[i] = A * (p0 + p1) + k0;
                ch_x1[i] = A * (p0 + p2) + k1;
            }
        if (i < CH) {                    /* a saw at its wrap: the sample as it was, its bleps */
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
            s0 = 2.0f * p0 - 1.0f - blepi(p0, d0, id0);
            s1 = 2.0f * p1 - 1.0f - blepi(p1, d1, id1);
            s2 = 2.0f * p2 - 1.0f - blepi(p2, d2, id2);
            if (sq) {
                float q0 = p0 + w0, q1 = p1 + w1, q2 = p2 + w2;
                if (q0 >= 1.0f)
                    q0 -= 1.0f;
                if (q1 >= 1.0f)
                    q1 -= 1.0f;
                if (q2 >= 1.0f)
                    q2 -= 1.0f;
                s0 -= sh * (2.0f * q0 - 1.0f - blepi(q0, d0, id0));
                s1 -= sh * (2.0f * q1 - 1.0f - blepi(q1, d1, id1));
                s2 -= sh * (2.0f * q2 - 1.0f - blepi(q2, d2, id2));
            }
            if (soft) {
                ch_xc[i] = s0;
                ch_x0[i] = s1;
                ch_x1[i] = s2;
            } else {
                ch_x0[i] = s0 + s1;
                ch_x1[i] = s0 + s2;
            }
            i++;
        }
    }
    v->cph[0] = p0;
    v->cph[1] = p1;
    v->cph[2] = p2;
    if (soft) {                          /* SOFT: the wave integrated once (the square a triangle), then twice (near a
                                          * sine), each saw at its own gain (its fundamental kept: 2 pi f a sample;
                                          * the centre's at INTERVAL's pitch, the sides' at the note's), a leak at a
                                          * sixth of it (no DC creeping); crossfaded by quarter turns, the three a
                                          * quarter turn apart at the fundamental: it keeps its level. The states held
                                          * without their gains (k, k^2 in the mix). The centre then into both sides */
        float kc = 6.2831853f * d0, ks = 6.2831853f * d1, lc = 1.0f - 0.15f * kc, ls = 1.0f - 0.15f * ks;
        float u = P.ch_soft, a0, a1, a2, c1, c2, s1, s2;
        float cA = v->cint[0], cB = v->cint[1], l1 = v->cint[2], l2 = v->cint[3], r1 = v->cint[4], r2 = v->cint[5];
        uint32_t j;
        if (u < 0.5f) {
            a0 = fm_cosf(3.14159265f * u);
            a1 = fm_sinf(3.14159265f * u);
            a2 = 0.0f;
        } else {
            a0 = 0.0f;
            a1 = fm_cosf(3.14159265f * (u - 0.5f));
            a2 = fm_sinf(3.14159265f * (u - 0.5f));
        }
        c1 = a1 * kc;
        c2 = a2 * kc * kc;
        s1 = a1 * ks;
        s2 = a2 * ks * ks;
        for (j = 0; j < CH; j++) {
            float xc = ch_xc[j], xl = ch_x0[j], xr = ch_x1[j], c;
            cA = lc * cA + xc;
            cB = lc * cB + cA;
            l1 = ls * l1 + xl;
            l2 = ls * l2 + l1;
            r1 = ls * r1 + xr;
            r2 = ls * r2 + r1;
            c = a0 * xc + c1 * cA + c2 * cB;
            ch_x0[j] = c + a0 * xl + s1 * l1 + s2 * l2;
            ch_x1[j] = c + a0 * xr + s1 * r1 + s2 * r2;
        }
        if (!(cB > -1e9f && cB < 1e9f && l2 > -1e9f && l2 < 1e9f && r2 > -1e9f && r2 < 1e9f))
            cA = cB = l1 = l2 = r1 = r2 = 0.0f;   /* (never wild: begun again from nothing) */
        v->cint[0] = cA;
        v->cint[1] = cB;
        v->cint[2] = l1;
        v->cint[3] = l2;
        v->cint[4] = r1;
        v->cint[5] = r2;
    } else {                             /* off: its states emptied, SOFT turned up again (an LFO crossing 0, a CC, a
                                          * scene) starts from nothing, never from what they held when it went off */
        v->cint[0] = v->cint[1] = v->cint[2] = v->cint[3] = v->cint[4] = v->cint[5] = 0.0f;
    }
    if (svf) {                           /* the type's mix with the gain in it, gliding to sc over the chunk */
        float cf[3], o[3], e[3];
        cf[0] = v->sa1;
        cf[1] = v->sa2;
        cf[2] = v->sa3;
        o[0] = v->sp0 * g;
        o[1] = v->sp1 * g;
        o[2] = v->sp2 * g;
        e[0] = (v->sc0 - v->sp0) * g * (1.0f / (float)CH);
        e[1] = (v->sc1 - v->sp1) * g * (1.0f / (float)CH);
        e[2] = (v->sc2 - v->sp2) * g * (1.0f / (float)CH);
        if (e[0] == 0.0f && e[1] == 0.0f && e[2] == 0.0f && o[0] == 0.0f && (o[1] == 0.0f || o[2] == 0.0f)) {
            ch_svf2(ba, bb, cf, v->clp, o[1] == 0.0f ? o[2] : o[1], o[1] == 0.0f);
        } else {
            ch_svf(ba, ch_x0, cf, v->clp, o, e);
            ch_svf(bb, ch_x1, cf, v->clp + 2, o, e);
        }
    } else {
        ch_lp(ba, bb, ch_x0, ch_x1, v->ca, g, v->clp);
    }
    v->sp0 = v->sc0;
    v->sp1 = v->sc1;
    v->sp2 = v->sc2;
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

/* partials j and k (k < 0: j alone) into one bus, two at a time: their states stay in registers (the FPU's are the
 * CPU's sixteen: the magic circle's four a partial, two pairs no spill). With BEAT their levels ramp (pac, pda) */
static __attribute__((noinline)) void partial_ramp(voice_t *v, uint32_t j, int k, float *b, float g)
{
    float x1 = v->px[j], y1 = v->py[j], e1 = 2.0f * v->ps[j], a1 = v->pac[j] * g, d1 = v->pda[j] * g;
    uint32_t i;
    if (k < 0) {
        for (i = 0; i < CH; i++) {
            x1 -= e1 * y1;
            y1 += e1 * x1;
            b[i] += a1 * x1;
            a1 += d1;
        }
    } else {
        float x2 = v->px[k], y2 = v->py[k], e2 = 2.0f * v->ps[k], a2 = v->pac[k] * g, d2 = v->pda[k] * g;
        for (i = 0; i < CH; i++) {
            x1 -= e1 * y1;
            x2 -= e2 * y2;
            y1 += e1 * x1;
            y2 += e2 * x2;
            b[i] += a1 * x1 + a2 * x2;
            a1 += d1;
            a2 += d2;
        }
        v->px[k] = x2;
        v->py[k] = y2;
        v->pac[k] += v->pda[k] * (float)CH;
    }
    v->px[j] = x1;
    v->py[j] = y1;
    v->pac[j] += v->pda[j] * (float)CH;
}

static void partial_pair(voice_t *v, uint32_t j, int k, float *b, float g)
{
    float x1 = v->px[j], y1 = v->py[j], e1 = 2.0f * v->ps[j], a1 = v->pa[j] * g;
    uint32_t i;
    if (k < 0) {
        for (i = 0; i < CH; i++) {
            x1 -= e1 * y1;
            y1 += e1 * x1;
            b[i] += a1 * x1;
        }
    } else {
        float x2 = v->px[k], y2 = v->py[k], e2 = 2.0f * v->ps[k], a2 = v->pa[k] * g;
        for (i = 0; i < CH; i++) {
            x1 -= e1 * y1;
            x2 -= e2 * y2;
            y1 += e1 * x1;
            y2 += e2 * x2;
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
    if (j == 0) {                        /* (the first alone, without AIR) */
        float y1 = v->r1[0], y2 = v->r2[0], b1 = v->rb1[0], b2 = v->rb2[0], gg = v->rg[0] * g;
        for (i = 0; i < CH; i++) {
            float y = gg * nz[i] + b1 * y1 - b2 * y2;
            y2 = y1;
            y1 = y;
            bc[i] += y;
        }
        if (y1 > -1e-20f && y1 < 1e-20f && y2 > -1e-20f && y2 < 1e-20f)
            y1 = y2 = 0.0f;
        v->r1[0] = y1;
        v->r2[0] = y2;
    }
    {                                    /* the other two together: their chains side by side (the FPU's latency) */
        float y1 = v->r1[1], y2 = v->r2[1], b1 = v->rb1[1], b2 = v->rb2[1], gg = v->rg[1] * g;
        float z1 = v->r1[2], z2 = v->r2[2], c1 = v->rb1[2], c2 = v->rb2[2], gh = v->rg[2] * g;
        for (i = 0; i < CH; i++) {
            float y = gg * nz[i] + b1 * y1 - b2 * y2, z = gh * nz[i] + c1 * z1 - c2 * z2;
            y2 = y1;
            y1 = y;
            z2 = z1;
            z1 = z;
            ba[i] += y;
            bb[i] += z;
        }
        if (y1 > -1e-20f && y1 < 1e-20f && y2 > -1e-20f && y2 < 1e-20f)
            y1 = y2 = 0.0f;
        if (z1 > -1e-20f && z1 < 1e-20f && z2 > -1e-20f && z2 < 1e-20f)
            z1 = z2 = 0.0f;
        v->r1[1] = y1;
        v->r2[1] = y2;
        v->r1[2] = z1;
        v->r2[2] = z2;
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
    {                                    /* shedding: no more voices sounding than the level allows, the ones over it
                                          * let go quickly (some 15 ms: cut, they clicked); the partials' cap gliding
                                          * to the level's, a partial in some 50 ms (voice_block: the top one fading) */
        uint32_t on = 0, cap = SHED_NV[shed];
        float t = (float)SHED_NP[shed] - np_cap;
        np_cap += t > 0.12f ? 0.12f : t < -0.12f ? -0.12f : t;
        for (j = 0; j < NV; j++)
            on += vo[j].note >= 0 && !vo[j].shed;
        while (on > cap) {
            voice_t *x = 0;
            for (j = 0; j < NV; j++)     /* the quietest released one, else the oldest held one */
                if (vo[j].note >= 0 && !vo[j].shed && !vo[j].gate && (!x || vo[j].env < x->env))
                    x = &vo[j];
            if (!x)
                for (j = 0; j < NV; j++)
                    if (vo[j].note >= 0 && !vo[j].shed && (!x || vo[j].age < x->age))
                        x = &vo[j];
            x->gate = 0;
            x->shed = 1;
            on--;
        }
    }
    sl = 0.5f + 0.4f * P.spread * (0.3f + 0.7f * P.fullness);   /* a side's share on its own side */
    sr = 1.0f - sl;
    uint32_t tv0 = bud_tps > 0.0f ? prof_clk() : 0u, nr = 0;
    for (j = 0; j < NV; j++) {
        voice_t *v = &vo[j];
        if (v->note < 0)
            continue;
        if (v->wait) {                   /* room being made for it: silent until the voices let go are out */
            uint32_t k = n / CH;
            v->wait = v->wait > k ? (uint8_t)(v->wait - k) : 0u;
            continue;
        }
        nr++;
        prof(FR_PROF_VBLOCK);
        voice_block(v);
        for (c = 0; c < n; c += CH) {
            float bc[CH], ba[CH], bb[CH], g0 = v->lvl, g1, dg, lvl, al, ar, bl, br;
            v->env += ((v->gate ? 1.0f : 0.0f) - v->env) * (v->gate ? ka : v->shed ? 0.35f : kr);
            v->prs += (v->prs_t - v->prs) * 0.15f;
            lvl = (1.0f - P.velocity + P.velocity * v->vel) * (1.0f + 1.2f * P.pressure * br_clampf(v->prs + swell, 0.0f, 1.0f)) * VOICE_G;
            g1 = !v->gate && v->env < TAIL_END ? 0.0f : lvl * v->env * v->env;   /* (its tail's end: down to nothing
                                                                                 * over this chunk, then free) */
            v->lvl = g1;
            dg = (g1 - g0) * (1.0f / (float)CH);
            for (i = 0; i < CH; i++)
                bc[i] = ba[i] = bb[i] = 0.0f;
            prof(FR_PROF_CHOIR);
            if (P.choir > 0.001f)        /* (FULLNESS: the choir and the breath down, the partials less so) */
                choir_chunk(v, ba, bb, 0.35f * P.choir * (0.3f + 0.7f * P.fullness));
            prof(FR_PROF_PARTIALS);
            if (P.partials > 0.001f)
                partials_chunk(v, bc, ba, bb, P.partials * (0.7f + 0.3f * P.fullness));
            prof(FR_PROF_BREATH);
            if (P.souffle > 0.001f)
                souffle_chunk(v, noise + c, bc, ba, bb, P.souffle * P.fullness * P.fullness);
            prof(FR_PROF_VMIX);
            {                            /* a guard: a voice's buses gone wild or not numbers never reach the mix (in the
                                          * cloud or the master one would hold for good: every scene broken till a
                                          * restart); the voice starts again from nothing (states as a NaN carries
                                          * them on, the last sample and the middle tell) */
                float z = bc[CH - 1] + ba[CH - 1] + bb[CH - 1] + bc[CH / 2] + ba[CH / 2] + bb[CH / 2];
                if (!(z > -1e4f && z < 1e4f)) {
                    for (i = 0; i < CH; i++)
                        bc[i] = ba[i] = bb[i] = 0.0f;
                    voice_start(v, v->note, v->vel, 1);
                }
            }
            al = v->gl * sl;                 /* the sides into the field, then the voice's place */
            ar = v->gr * sr;
            bl = v->gl * sr;
            br = v->gr * sl;
            for (i = 0; i < CH; i++) {
                g0 += dg;
                ol[c + i] += (bc[i] * v->gl + ba[i] * al + bb[i] * bl) * g0;
                or_[c + i] += (bc[i] * v->gr + ba[i] * ar + bb[i] * br) * g0;
            }
            if (g1 == 0.0f && !v->gate && v->env < TAIL_END)
                break;
        }
        if (!v->gate && v->env < TAIL_END)
            v->note = -1;                /* faded out: free */
    }
    if (bud_tps > 0.0f) {                /* (the voices' share of this render: make_room's foresight) */
        rv_ticks += prof_clk() - tv0;
        rv_vs += nr * n;
    }
    prof(FR_PROF_TRACKS);
}

/* ------------------------------------------------- PUMP, the mix, MASTERING --- */
static float bsl[MAXN], bsr[MAXN];       /* the bass, apart: its own level and duck */
static float mxl[32], mxr[32];           /* a chunk of the mix on its way out (frasil_render's stages) */
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

/* --------------------------------------- MASTER FILTER, LOFI TAPE, MASTER EQ --- */
/* The tracks as four groups where PUMP's gains are (the drones, the cloud's return, GLINT, BASS), each sent to one of
 * four buses by what is on it: X the filter and the tape, Y the filter alone, Z the tape alone, W neither. Out: the tape
 * of fA(X) + Z, fB(Y), W (one tape, two filters at most). The cloud's return goes with the drones, unless ROUTING has
 * the effect before the cloud: the filter then on each track it has before the cloud's share is taken (pf: the cloud
 * fed them filtered), the tape on the tracks alone (one loop: the cloud fed them, and comes back, as they are). Each
 * effect goes in and out through 10 ms (its mix); its TYPE, TRACKS or ROUTING change once its mix is down (a chunk at
 * 0 before), the new one settling at 0 (the filter a few chunks, the tape till its loop is full), then in: no step */
#define FX_RAMP (32.0f / 441.0f)          /* a chunk's step of a mix: 0 to 1 in 10 ms */
static int fl_type, fl_bits, tp_bits;     /* as they sound now (T's, once its mix is down) */
static int fl_pre_on, tp_pre_on;          /* and their ROUTING */
static uint32_t fl_warm, tp_warm;         /* chunks to settle, at 0, before coming in */
static float fl_mix, tp_mix, eq_mix;      /* 0 dry .. 1 */
static float fl_lc = 14.3f;               /* log2 of the cutoff, gliding a chunk at a time */
static float fl_k = 1.4142f;              /* 1 / Q, gliding the same way */
static float tp_sat_g, tp_wob_g;          /* SATURATION and WOBBLE as they sound, gliding (no step as they turn) */
static float pc_g = 1.0f;                 /* PUMP on the cloud's return, gliding in and out */
static float fa1, fa2, fb1, fb2, ga1, ga2, gb1, gb2;   /* the two filters' states, each side (Zavalishin's SVF) */
static float pf[12];                      /* before the cloud: the drones', GLINT's, BASS's filters, s1 s2 each side */
static float pan_s[4];                    /* the pans as they sound (frasil_params_t's pan), gliding a chunk at a time */
typedef struct {
    float m0, dm, c1, c2, c3, k;          /* the filter's mix at the chunk's start and its step; its coefficients */
    int type, bits, pre, reset;           /* (reset: changed at this chunk, its states from nothing: the passes empty
                                           * them as they come to it, not before the chunks before it are done) */
} flk_t;
static flk_t flk[MAXN / 32u];             /* the filter, each chunk of the block (made before the cloud) */

static inline float svf(float x, float *s1, float *s2, float a1, float a2, float a3, float k, int hp)
{
    float v3 = x - *s2, v1 = a1 * *s1 + a2 * v3, v2 = *s2 + a2 * *s1 + a3 * v3;
    *s1 = 2.0f * v1 - *s1;
    *s2 = 2.0f * v2 - *s2;
    return hp ? x - k * v1 - v2 : v2;
}

/* LOFI TAPE: what is on it recorded saturated (tanh's shape, a little lopsided: some even harmonics too; what comes
 * off kept as loud as what went on) onto a loop of tape, read back where its speed has it: wow (wandering between 0.3
 * and 0.8 Hz, a drift under it) and flutter (7.3 Hz), as deep as WOBBLE; now and then, the more WOBBLE the more often,
 * the tape drags: slower a moment (a tenth of a second to well over one; 2 to 8 % at most, the longest less), its level
 * dipping, then on at speed, catching up what it lost. Duller as SATURATION grows (a pole from 16 kHz down to 6) */
#define TP_N 2048u                        /* the loop: 46 ms each side, in the pool */
#define TP_D0 420.0f                      /* where it reads, at rest: 9.5 ms behind (beyond the wow's depth) */
#define TP_XMAX 1200.0f                   /* the most the drags may hold back (the longest the shallower) */
#define TP_ONE 26000.0f                   /* the tape's full level (what it records reaches 1.25) */
static int16_t tp_bl[TP_N] BR_POOL, tp_br[TP_N] BR_POOL;
static uint32_t tp_w;                     /* where it writes next */
static float tp_ph1, tp_ph2, tp_ph3, tp_d = TP_D0, tp_l, tp_r, tp_dcl, tp_dcr, tp_dip = 1.0f;
static float tp_ai, tp_ao, tp_ei, tp_eo;  /* what goes onto the tape and comes off, a chunk's and followed: the gain
                                          * that keeps it as loud */
static float tp_rate = 0.55f, tp_rate_to = 0.55f;   /* the wow's rate, and where it wanders to */
static float tp_x, tp_drag, tp_drag_n, tp_drag_s;   /* the delay the drags hold back; the one under way: its time
                                                     * left, its length, its depth */
static volatile float tp_speed = 1.0f;    /* frasil_tape_speed */
static volatile float tp_dragd;          /* frasil_tape_drag */

float frasil_tape_speed(void) { return tp_speed; }
float frasil_tape_drag(void) { return tp_dragd; }

static inline float tp_clip(float x)     /* tanh's shape, cheaper: a cubic to 1.5 (no division: the FM-1's is
                                          * slow), flat beyond */
{
    x = x > 1.5f ? 1.5f : x < -1.5f ? -1.5f : x;
    return x - 0.148148148f * x * x * x;
}

/* a sample each side onto the tape (driven by tg, lopsided by bias, cb its own level), and back read d samples
 * behind, at itg, its DC out, dulled (ta), at dip (a drag's, and the gain that keeps it as loud), in at mix */
static inline void tp_step(float *l, float *r, float d, float tg, float bias, float cb, float itg, float ta,
                           float dip, float mix)
{
    float at, f, a, b;
    uint32_t k0, k1;
    tp_ai += fm_fabsf(*l) + fm_fabsf(*r);
    tp_bl[tp_w] = (int16_t)(TP_ONE * (tp_clip(*l * tg + bias) - cb));
    tp_br[tp_w] = (int16_t)(TP_ONE * (tp_clip(*r * tg + bias) - cb));
    tp_w = (tp_w + 1u) & (TP_N - 1u);
    at = (float)tp_w - d;
    at += at < 0.0f ? (float)TP_N : 0.0f;
    k0 = (uint32_t)at;
    f = at - (float)k0;
    k0 &= TP_N - 1u;
    k1 = (k0 + 1u) & (TP_N - 1u);
    a = ((float)tp_bl[k0] + (float)(tp_bl[k1] - tp_bl[k0]) * f) * itg;
    b = ((float)tp_br[k0] + (float)(tp_br[k1] - tp_br[k0]) * f) * itg;
    tp_dcl += (a - tp_dcl) * 0.0007f;
    tp_dcr += (b - tp_dcr) * 0.0007f;
    tp_l += (a - tp_dcl - tp_l) * ta;
    tp_r += (b - tp_dcr - tp_r) * ta;
    tp_ao += fm_fabsf(tp_l) + fm_fabsf(tp_r);
    *l += (tp_l * dip - *l) * mix;
    *r += (tp_r * dip - *r) * mix;
}

/* MASTER EQ: three biquads each side (transposed direct form II); their coefficients made again when a setting moves,
 * the ones sounding gliding to them a chunk at a time */
static float eq_c[3][5] = {{1.0f}, {1.0f}, {1.0f}}, eq_t[3][5], eq_z[3][4], eq_key[7];

void frasil_eq_coefs(int kind, float hz, float db, float q, float *c)
{
    float A = fm_exp2f(db * 0.0830482f), w = 6.2831853f * hz * BR_INV_FS, cw = fm_cosf(w), sw = fm_sinf(w);
    float b0, b1, b2, a0, a1, a2;
    if (kind == 1) {
        float al = sw / (2.0f * q);
        b0 = 1.0f + al * A;
        b1 = -2.0f * cw;
        b2 = 1.0f - al * A;
        a0 = 1.0f + al / A;
        a1 = -2.0f * cw;
        a2 = 1.0f - al / A;
    } else {
        float sq = 2.0f * fm_sqrtf(A) * sw * 0.70710678f, ap = A + 1.0f, am = A - 1.0f, s = kind ? -1.0f : 1.0f;
        b0 = A * (ap - s * am * cw + sq);
        b1 = s * 2.0f * A * (am - s * ap * cw);
        b2 = A * (ap - s * am * cw - sq);
        a0 = ap + s * am * cw + sq;
        a1 = -s * 2.0f * (am + s * ap * cw);
        a2 = ap + s * am * cw - sq;
    }
    c[0] = b0 / a0;
    c[1] = b1 / a0;
    c[2] = b2 / a0;
    c[3] = a1 / a0;
    c[4] = a2 / a0;
}

static void eq_chunk(void)               /* a chunk's: its coefficients, aimed at and glided to; its mix */
{
    const float key[7] = {T.eq_low, T.eq_mid, T.eq_high, T.eq_lowf, T.eq_midf, T.eq_q, T.eq_highf};
    uint32_t b, j, moved = 0;
    for (j = 0; j < 7u; j++)
        if (key[j] != eq_key[j]) {
            eq_key[j] = key[j];
            moved = 1;
        }
    if (moved) {
        frasil_eq_coefs(0, T.eq_lowf, T.eq_low, 0.707f, eq_t[0]);
        frasil_eq_coefs(1, T.eq_midf, T.eq_mid, T.eq_q, eq_t[1]);
        frasil_eq_coefs(2, T.eq_highf, T.eq_high, 0.707f, eq_t[2]);
        if (eq_mix <= 0.0f)              /* (off: no glide from the last, it comes in at these) */
            for (b = 0; b < 3u; b++)
                for (j = 0; j < 5u; j++)
                    eq_c[b][j] = eq_t[b][j];
    }
    for (b = 0; b < 3u; b++)
        for (j = 0; j < 5u; j++)
            eq_c[b][j] += (eq_t[b][j] - eq_c[b][j]) * 0.3f;
}

static inline float eq_side(float x, uint32_t s)   /* the three bands on one side (s: 0 left, 2 right) */
{
    uint32_t b;
    for (b = 0; b < 3u; b++) {
        const float *c = eq_c[b];
        float *z = &eq_z[b][s], y = c[0] * x + z[0];
        z[0] = c[1] * x - c[3] * y + z[1];
        z[1] = c[2] * x - c[4] * y;
        x = y;
    }
    return x;
}

/* CUT (OCT- held, PLAY): the block after it is asked fades out (no click), the next one empties everything (the
 * voices, GLINT and its echoes, the cloud, TAPE's loop) and stays silent */
static volatile uint8_t cut_at;          /* 1: asked (this block fades out), 2: the next empties everything */
void frasil_cut(void)
{
    if (!cut_at)
        cut_at = 1;
}

void frasil_render(int32_t *lr, uint32_t n, float gain)
{
    uint32_t off = 0;
    if (cut_at == 2) {
        uint32_t i;
        frasil_panic();
        glint_cut();
        nuage_clear();
        for (i = 0; i < TP_N; i++)
            tp_bl[i] = tp_br[i] = 0;
        for (i = 0; i < 2u * n; i++)
            lr[i] = 0;
        cut_at = 0;
        return;
    }
    if (prof_clk) {
        prof_t = prof_clk();
        prof_at = FR_PROF_SETUP;
    }
    uint32_t t_in = bud_tps > 0.0f ? prof_clk() : 0u;
    rv_ticks = rv_vs = 0u;
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
        {                                /* (in passes, each a short loop: the same sums, sample for sample) */
            const float mxd = P.mx_drone;
            float l1 = dcl, r1 = dcr;
            for (i = 0; i < m; i++) {
                spl[i] = spr[i] = bsl[i] = bsr[i] = 0.0f;
                l1 += 0.0014f * (mixl[i] - l1);
                r1 += 0.0014f * (mixr[i] - r1);
                mixl[i] -= l1;
                mixr[i] -= r1;
            }
            dcl = l1;
            dcr = r1;
            if (P.drive * P.fullness > 0.001f)
                for (i = 0; i < m; i++) {
                    mixl[i] = fm_tanhf(mixl[i] * dk) * dmake;
                    mixr[i] = fm_tanhf(mixr[i] * dk) * dmake;
                }
            if (thin) {                  /* FULLNESS down: the body high-passed, 20 Hz .. 400 Hz */
                l1 = thl;
                r1 = thr;
                for (i = 0; i < m; i++) {
                    float xl = mixl[i], xr = mixr[i];
                    l1 += ha * (xl - l1);
                    r1 += ha * (xr - r1);
                    mixl[i] = rinl[i] = (xl - l1) * mxd;
                    mixr[i] = rinr[i] = (xr - r1) * mxd;
                }
                thl = l1;
                thr = r1;
            } else {
                thl = thr = 0.0f;        /* (from nothing the next time) */
                for (i = 0; i < m; i++) {
                    mixl[i] = rinl[i] = mixl[i] * mxd;
                    mixr[i] = rinr[i] = mixr[i] * mxd;
                }
            }
        }
        prof(FR_PROF_GLINT);
        {                                /* the glints at their level: dry beside the drones, their share into the
                                          * reverb, their echoes on them alone */
            glint_params_t gp = T.glint;
            gp.level *= P.mx_glint;
            glint_render(spl, spr, rinl, rinr, m, &gp);
            glint_delay(spl, spr, rinl, rinr, m, &gp);
        }
        prof(FR_PROF_BASS);
        bass_render(bsl, bsr, m, &T.bass);   /* the bass: none of it in the reverb */
        prof(FR_PROF_TRACKS);
        {                                /* MASTER FILTER, a chunk at a time (before the cloud: its ROUTING) */
            uint32_t k, j;
            for (k = 0, c = 0; c < m; c += 32u, k++) {
                uint32_t e = c + 32u > m ? m : c + 32u;
                int wt = T.fl_type, wb = wt ? T.fl_tracks : 0, wp = wt ? T.fl_pre : 0;
                flk_t *f = &flk[k];
                f->m0 = fl_mix;
                f->reset = 0;
                if (wt != fl_type || wb != fl_bits || wp != fl_pre_on) {
                    if (fl_mix <= 0.0f) {    /* (down already: the new one, at 0, settling a few chunks) */
                        fl_type = wt;
                        fl_bits = wb;
                        fl_pre_on = wp;
                        f->reset = 1;
                        fl_warm = 4u;
                    } else {
                        fl_mix = fl_mix - FX_RAMP < 0.0f ? 0.0f : fl_mix - FX_RAMP;
                    }
                } else if (fl_type) {
                    if (fl_warm)
                        fl_warm--;
                    else
                        fl_mix = fl_mix + FX_RAMP > 1.0f ? 1.0f : fl_mix + FX_RAMP;
                }
                f->dm = (fl_mix - f->m0) / (float)(e - c);
                f->type = fl_type;
                f->bits = fl_bits;
                f->pre = fl_pre_on;
                f->k = 1.0f;
                f->c1 = f->c2 = f->c3 = 0.0f;
                if (fl_type) {           /* its cutoff, gliding in octaves */
                    float g, lc = fm_log2f(T.fl_cut > 19000.0f ? 19000.0f : T.fl_cut);
                    fl_lc += (lc - fl_lc) * 0.15f;
                    g = fm_tanf(3.14159265f * fm_exp2f(fl_lc) * BR_INV_FS);
                    fl_k += (1.0f / T.fl_q - fl_k) * 0.15f;
                    f->k = fl_k;
                    f->c1 = 1.0f / (1.0f + g * (g + f->k));
                    f->c2 = g * f->c1;
                    f->c3 = g * f->c2;
                }
            }
            for (k = 0, c = 0; c < m; c += 32u, k++) {   /* before the cloud: each track it has through its own filter,
                                                          * the cloud fed them so (GLINT's share: SPACE's, glint_render's) */
                const flk_t *f = &flk[k];
                uint32_t e = c + 32u > m ? m : c + 32u, b = f->pre ? (uint32_t)f->bits : 0u;
                float fm = f->m0, sp = T.glint.space;
                int hp = f->type == 2;
                if (f->reset)
                    for (j = 0; j < 12u; j++)
                        pf[j] = 0.0f;
                if (!b)
                    continue;
                for (i = c; i < e; i++) {
                    fm += f->dm;
                    if (b & 1u) {
                        mixl[i] += (svf(mixl[i], &pf[0], &pf[1], f->c1, f->c2, f->c3, f->k, hp) - mixl[i]) * fm;
                        mixr[i] += (svf(mixr[i], &pf[2], &pf[3], f->c1, f->c2, f->c3, f->k, hp) - mixr[i]) * fm;
                    }
                    if (b & 2u) {
                        spl[i] += (svf(spl[i], &pf[4], &pf[5], f->c1, f->c2, f->c3, f->k, hp) - spl[i]) * fm;
                        spr[i] += (svf(spr[i], &pf[6], &pf[7], f->c1, f->c2, f->c3, f->k, hp) - spr[i]) * fm;
                    }
                    if (b & 4u) {
                        bsl[i] += (svf(bsl[i], &pf[8], &pf[9], f->c1, f->c2, f->c3, f->k, hp) - bsl[i]) * fm;
                        bsr[i] += (svf(bsr[i], &pf[10], &pf[11], f->c1, f->c2, f->c3, f->k, hp) - bsr[i]) * fm;
                    }
                    if (b & 3u) {
                        rinl[i] = mixl[i] + sp * spl[i];
                        rinr[i] = mixr[i] + sp * spr[i];
                    }
                }
            }
        }
        /* the reverb, and the mix of dry and wet (equal power: SPACE), the cloud's return at its level */
        prof(FR_PROF_CLOUD);
        nuage_process(rinl, rinr, wetl, wetr, m);
        prof(FR_PROF_MIX);
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
        if (T.eq_on || eq_mix > 0.0f)
            eq_chunk();
        /* the compressor's detector: 10 ms up, RELEASE down; its gain from it a chunk at a time */
        ka = 1.0f - fm_expf(-1.0f / (0.01f * BR_FS));
        kr = 1.0f - fm_expf(-1.0f / (P.comp_rel * BR_FS + 1.0f));
        for (c = 0; c < m; c += 32u) {
            float d1, dd, d = pump_d, g1 = 1.0f, cg, dcg, slope = 1.0f - 1.0f / (comp ? P.comp_ratio : 1.0f);
            const flk_t *f = &flk[c / 32u];
            float fm0 = f->m0, dfm = f->dm, fk = f->k, fc1 = f->c1, fc2 = f->c2, fc3 = f->c3;
            if (f->reset)
                fa1 = fa2 = fb1 = fb2 = ga1 = ga2 = gb1 = gb2 = 0.0f;
            float tm0, em0, td0, dp0, dtm, dem, ddt, ddp;
            float tg = 1.0f, tb = 0.0f, tcb = 0.0f, itg = 1.0f, ta = 1.0f, gl4[4], gr4[4];
            float wx[4], wy[4], wz[4];        /* the groups (the drones, the cloud's return, GLINT, BASS) in X, Y, Z */
            uint32_t j;
            int fb = f->pre ? 0 : f->bits, hp = f->type == 2, fx_all, hx = 0, hy = 0, hz = 0, pan_on = 0;
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
            tm0 = tp_mix;
            em0 = eq_mix;
            td0 = tp_d;
            dp0 = tp_dip;
            {                            /* LOFI TAPE: in, out, or out to change its TRACKS or ROUTING, the loop filled
                                          * at 0, then in */
                int wb = T.tp_on ? T.tp_tracks : 0, wp = wb ? T.tp_pre : 0;
                if (wb != tp_bits || wp != tp_pre_on) {
                    if (tp_mix <= 0.0f) {
                        uint32_t k;
                        tp_bits = wb;
                        tp_pre_on = wp;
                        for (k = 0; k < TP_N; k++)   /* (an empty tape: not what it had the last time) */
                            tp_bl[k] = tp_br[k] = 0;
                        tp_l = tp_r = tp_dcl = tp_dcr = tp_ai = tp_ao = tp_ei = tp_eo = 0.0f;
                        tp_warm = (uint32_t)((TP_D0 + 410.0f + tp_x) * (1.0f / 32.0f)) + 2u;
                    } else {
                        tp_mix = tp_mix - FX_RAMP < 0.0f ? 0.0f : tp_mix - FX_RAMP;
                    }
                } else if (tp_bits) {
                    if (tp_warm)
                        tp_warm--;
                    else
                        tp_mix = tp_mix + FX_RAMP > 1.0f ? 1.0f : tp_mix + FX_RAMP;
                }
            }
            eq_mix = T.eq_on ? (eq_mix + FX_RAMP > 1.0f ? 1.0f : eq_mix + FX_RAMP)
                             : (eq_mix - FX_RAMP < 0.0f ? 0.0f : eq_mix - FX_RAMP);
            dtm = (tp_mix - tm0) / (float)(e - c);
            dem = (eq_mix - em0) / (float)(e - c);
            {                            /* each group's bus (the cloud's return the drones', unless before the cloud) */
                int fg[4], tgp[4];
                fg[0] = fg[1] = fb & 1;
                fg[2] = fb >> 1 & 1;
                fg[3] = fb >> 2 & 1;
                tgp[0] = tp_bits & 1;
                tgp[1] = tp_pre_on ? 0 : tp_bits & 1;
                tgp[2] = tp_bits >> 1 & 1;
                tgp[3] = tp_bits >> 2 & 1;
                fx_all = (fb | tp_bits) != 0;
                for (j = 0; j < 4u; j++) {
                    wx[j] = (float)(fg[j] & tgp[j]);
                    wy[j] = (float)(fg[j] & (tgp[j] ^ 1));
                    wz[j] = (float)((fg[j] ^ 1) & tgp[j]);
                    hx |= fg[j] & tgp[j];
                    hy |= fg[j] & (tgp[j] ^ 1);
                    hz |= (fg[j] ^ 1) & tgp[j];
                    if (fg[j] != fg[0] || tgp[j] != tgp[0])
                        fx_all = 0;
                }
            }
            pc_g += ((T.pump_cloud ? 1.0f : 0.0f) - pc_g) * 0.15f;
            for (j = 0; j < 4u; j++) {   /* the pans: a balance, the far side down a quarter sine */
                float pn;
                pan_s[j] += (T.pan[j] - pan_s[j]) * 0.25f;
                pn = pan_s[j];
                gl4[j] = pn > 0.0f ? fm_sin_turns(0.25f - 0.25f * pn) : 1.0f;
                gr4[j] = pn < 0.0f ? fm_sin_turns(0.25f + 0.25f * pn) : 1.0f;
                pan_on |= pn > 0.001f || pn < -0.001f;
            }
            if (tp_bits) {               /* the tape's place at the chunk's end: wow, flutter, a drag */
                float w, ch = (float)(e - c) * BR_INV_FS, v = 0.0f, u;
                tp_sat_g += (T.tp_sat - tp_sat_g) * 0.15f;
                tp_wob_g += (T.tp_wobble - tp_wob_g) * 0.02f;   /* (some 40 ms: its depth moves the head slowly) */
                w = tp_wob_g;
                if ((br_rand(&rng) >> 22) == 0u)   /* (one chunk in 1024, some 0.7 s) the wow's rate: elsewhere */
                    tp_rate_to = 0.3f + 0.5f * (float)(br_rand(&rng) >> 8) * (1.0f / 16777216.0f);
                tp_rate += (tp_rate_to - tp_rate) * 0.003f;
                tp_ph1 += tp_rate * ch;
                tp_ph2 += 0.23f * ch;
                tp_ph3 += 7.3f * ch;
                tp_ph1 -= tp_ph1 >= 1.0f ? 1.0f : 0.0f;
                tp_ph2 -= tp_ph2 >= 1.0f ? 1.0f : 0.0f;
                tp_ph3 -= tp_ph3 >= 1.0f ? 1.0f : 0.0f;
                if (tp_drag > 0.0f) {    /* a drag under way: the speed it takes, most at its middle */
                    u = fm_sin_turns(0.5f * tp_drag / tp_drag_n);
                    v = tp_drag_s * u * u;
                    tp_x += v * (float)(e - c);
                    tp_drag -= ch;
                } else {                 /* on at speed, catching up (some 3 s); now and then a drag begins */
                    tp_x -= tp_x * ch * (1.0f / 3.0f);
                    if ((float)(br_rand(&rng) >> 8) * (1.0f / 16777216.0f) < w * w * ch * 0.25f) {
                        float r1 = (float)(br_rand(&rng) >> 8) * (1.0f / 16777216.0f);
                        float r2 = (float)(br_rand(&rng) >> 8) * (1.0f / 16777216.0f), most;
                        tp_drag_n = tp_drag = 0.1f * fm_exp2f(3.8f * r1 * r1);   /* 0.1 .. 1.4 s, short ones more */
                        tp_drag_s = w * (0.02f + 0.06f * r2);
                        most = (TP_XMAX - tp_x) / (tp_drag_n * BR_FS * 0.5f);   /* (what the loop has room for) */
                        tp_drag_s = tp_drag_s > most ? most : tp_drag_s;
                        if (tp_drag_s < 0.005f)
                            tp_drag = 0.0f;
                    }
                }
                u = w * fm_sqrtf(w);
                tp_d = TP_D0 + 400.0f * u * (0.7f * fm_sin_turns(tp_ph1) + 0.3f * fm_sin_turns(tp_ph2)) +
                       6.0f * w * fm_sin_turns(tp_ph3) + tp_x;
                tp_ei += (tp_ai - tp_ei) * 0.02f;   /* (some 70 ms) */
                tp_eo += (tp_ao - tp_eo) * 0.02f;
                tp_ai = tp_ao = 0.0f;
                u = (tp_ei + 1e-3f) / (tp_eo + 1e-3f);
                tp_dip = (1.0f - 6.0f * v) * (u < 0.5f ? 0.5f : u > 4.0f ? 4.0f : u);
                tg = 1.0f + 30.0f * tp_sat_g * tp_sat_g;
                tb = 0.25f * tp_sat_g;
                tcb = tp_clip(tb);
                itg = 1.0f / (tg * TP_ONE);
                ta = 1.0f - fm_expf(-6.2831853f * (16000.0f - 10000.0f * tp_sat_g) * BR_INV_FS);
            }
            ddt = (tp_d - td0) / (float)(e - c);
            tp_speed = tp_bits ? 1.0f - ddt : 1.0f;   /* (reading further behind: slower) */
            tp_dragd = tp_bits && tp_drag > 0.0f ? tp_drag_s : 0.0f;
            ddp = (tp_dip - dp0) / (float)(e - c);
            {                            /* the chunk in stages, each a short loop (on the FM-1 the FPU's registers
                                          * are the CPU's sixteen: one loop for all of it spilled at every turn): the
                                          * groups to the sides (the filter and the tape on theirs) with the meters,
                                          * then SINK, MASTER EQ, MASTERING and the tick when they are on, then out.
                                          * Sample for sample the same sums as the one loop made */
                const float kd = 0.95f * P.pump_drone, kb = 0.95f * P.pump_bass, kg = 0.95f * P.pump_glint;
                const float mb = P.mx_bass;
                uint32_t k;
                if (!(fb | tp_bits)) {   /* (no effect: the groups summed) */
                    for (i = c; i < e; i++) {
                        float gdr = 1.0f - kd * d, gb = (1.0f - kb * d) * mb, gg = 1.0f - kg * d, a;
                        float dl = mixl[i] * wd * gdr, dr = mixr[i] * wd * gdr;
                        float wl = wetl[i] * ww * (1.0f - pc_g * (1.0f - gdr));
                        float wr = wetr[i] * ww * (1.0f - pc_g * (1.0f - gdr));
                        float gl_ = spl[i] * gg, gr_ = spr[i] * gg, bl2 = bsl[i] * gb, br2 = bsr[i] * gb;
                        d += dd;
                        if (pan_on) {
                            dl *= gl4[0];
                            dr *= gr4[0];
                            wl *= gl4[3];
                            wr *= gr4[3];
                            gl_ *= gl4[1];
                            gr_ *= gr4[1];
                            bl2 *= gl4[2];
                            br2 *= gr4[2];
                        }
                        mxl[i - c] = dl + wl + gl_ + bl2;
                        mxr[i - c] = dr + wr + gr_ + br2;
                        if (!(i & 3u)) { /* the meters: one sample in four is enough for the screen */
                            a = fm_fabsf(dl) + fm_fabsf(dr);
                            pk[0] = a > pk[0] ? a : pk[0];
                            a = fm_fabsf(bsl[i] * gb);
                            pk[1] = a > pk[1] ? a : pk[1];
                            a = (fm_fabsf(spl[i]) + fm_fabsf(spr[i])) * gg;
                            pk[2] = a > pk[2] ? a : pk[2];
                            a = fm_fabsf(wl) + fm_fabsf(wr);
                            pk[3] = a > pk[3] ? a : pk[3];
                        }
                    }
                } else {
                    for (i = c; i < e; i++) {
                        float gdr = 1.0f - kd * d, gb = (1.0f - kb * d) * mb, gg = 1.0f - kg * d;
                        float dl, dr, wl, wr, l, r, a, gl_, gr_, bl2, br2;
                        d += dd;
                        dl = mixl[i] * wd * gdr;
                        dr = mixr[i] * wd * gdr;
                        wl = wetl[i] * ww * (1.0f - pc_g * (1.0f - gdr));
                        wr = wetr[i] * ww * (1.0f - pc_g * (1.0f - gdr));
                        gl_ = spl[i] * gg;
                        gr_ = spr[i] * gg;
                        bl2 = bsl[i] * gb;
                        br2 = bsr[i] * gb;
                        if (pan_on) {
                            dl *= gl4[0];
                            dr *= gr4[0];
                            wl *= gl4[3];
                            wr *= gr4[3];
                            gl_ *= gl4[1];
                            gr_ *= gr4[1];
                            bl2 *= gl4[2];
                            br2 *= gr4[2];
                        }
                        fm0 += dfm;
                        tm0 += dtm;
                        td0 += ddt;
                        dp0 += ddp;
                        if (fx_all) {        /* (each on every track, or off: one bus) */
                            l = dl + wl + gl_ + bl2;
                            r = dr + wr + gr_ + br2;
                            if (fb) {
                                l += (svf(l, &fa1, &fa2, fc1, fc2, fc3, fk, hp) - l) * fm0;
                                r += (svf(r, &ga1, &ga2, fc1, fc2, fc3, fk, hp) - r) * fm0;
                            }
                            if (tp_bits)
                                tp_step(&l, &r, td0, tg, tb, tcb, itg, ta, dp0, tm0);
                        } else {             /* the groups to their buses, the filters on X and Y, the tape on
                                              * fA(X) + Z, W the rest */
                            float xl = wx[0] * dl + wx[1] * wl + wx[2] * gl_ + wx[3] * bl2;
                            float xr = wx[0] * dr + wx[1] * wr + wx[2] * gr_ + wx[3] * br2;
                            float yl = wy[0] * dl + wy[1] * wl + wy[2] * gl_ + wy[3] * bl2;
                            float yr = wy[0] * dr + wy[1] * wr + wy[2] * gr_ + wy[3] * br2;
                            float zl = wz[0] * dl + wz[1] * wl + wz[2] * gl_ + wz[3] * bl2;
                            float zr = wz[0] * dr + wz[1] * wr + wz[2] * gr_ + wz[3] * br2;
                            float wl_ = dl + wl + gl_ + bl2 - xl - yl - zl, wr_ = dr + wr + gr_ + br2 - xr - yr - zr;
                            if (hx) {
                                xl += (svf(xl, &fa1, &fa2, fc1, fc2, fc3, fk, hp) - xl) * fm0;
                                xr += (svf(xr, &ga1, &ga2, fc1, fc2, fc3, fk, hp) - xr) * fm0;
                            }
                            if (hy) {
                                yl += (svf(yl, &fb1, &fb2, fc1, fc2, fc3, fk, hp) - yl) * fm0;
                                yr += (svf(yr, &gb1, &gb2, fc1, fc2, fc3, fk, hp) - yr) * fm0;
                            }
                            l = xl + zl;
                            r = xr + zr;
                            if (tp_bits && (hx | hz))
                                tp_step(&l, &r, td0, tg, tb, tcb, itg, ta, dp0, tm0);
                            l += yl + wl_;
                            r += yr + wr_;
                        }
                        mxl[i - c] = l;
                        mxr[i - c] = r;
                        if (!(i & 3u)) {
                            a = fm_fabsf(dl) + fm_fabsf(dr);
                            pk[0] = a > pk[0] ? a : pk[0];
                            a = fm_fabsf(bsl[i] * gb);
                            pk[1] = a > pk[1] ? a : pk[1];
                            a = (fm_fabsf(spl[i]) + fm_fabsf(spr[i])) * gg;
                            pk[2] = a > pk[2] ? a : pk[2];
                            a = fm_fabsf(wl) + fm_fabsf(wr);
                            pk[3] = a > pk[3] ? a : pk[3];
                        }
                    }
                }
                if (sink)
                    for (k = 0; k < e - c; k++) {
                        float l = mxl[k], r = mxr[k], v1, v2, v3 = l - sk_l2;
                        v1 = sa1 * sk_l1 + sa2 * v3;
                        v2 = sk_l2 + sa2 * sk_l1 + sa3 * v3;
                        sk_l1 = 2.0f * v1 - sk_l1;
                        sk_l2 = 2.0f * v2 - sk_l2;
                        mxl[k] = l + (v2 - l) * sw;
                        v3 = r - sk_r2;
                        v1 = sa1 * sk_r1 + sa2 * v3;
                        v2 = sk_r2 + sa2 * sk_r1 + sa3 * v3;
                        sk_r1 = 2.0f * v1 - sk_r1;
                        sk_r2 = 2.0f * v2 - sk_r2;
                        mxr[k] = r + (v2 - r) * sw;
                    }
                if (em0 > 0.0f || eq_mix > 0.0f)   /* MASTER EQ, before MASTERING */
                    for (k = 0; k < e - c; k++) {
                        em0 += dem;
                        if (em0 > 0.0f) {
                            mxl[k] += (eq_side(mxl[k], 0u) - mxl[k]) * em0;
                            mxr[k] += (eq_side(mxr[k], 2u) - mxr[k]) * em0;
                        }
                    }
                if (comp)                /* MASTERING: the detector, then the gain and the make-up */
                    for (k = 0; k < e - c; k++) {
                        float l = mxl[k], r = mxr[k], a = fm_fabsf(l) > fm_fabsf(r) ? fm_fabsf(l) : fm_fabsf(r);
                        cg += dcg;
                        comp_env += (a - comp_env) * (a > comp_env ? ka : kr);
                        mxl[k] = l * (cg * P.comp_gain);
                        mxr[k] = r * (cg * P.comp_gain);
                    }
                for (k = 0; k < e - c && ck_env > 0.0005f; k++) {   /* the metronome's tick, over everything (no
                                                                     * cloud, no SINK, no meter), some 15 ms: a 3 ms
                                                                     * fall */
                    float a = fm_sin_turns(ck_ph) * ck_env;
                    ck_ph += ck_inc;
                    ck_ph -= ck_ph >= 1.0f ? 1.0f : 0.0f;
                    ck_env *= 0.9925f;
                    mxl[k] += a;
                    mxr[k] += a;
                }
                for (k = 0, i = c; i < e; i++, k++) {   /* out: the soft limit, the peak, MASTER */
                    float l = br_soft(mxl[k]), r = br_soft(mxr[k]), a = fm_fabsf(l) > fm_fabsf(r) ? fm_fabsf(l) : fm_fabsf(r);
                    peak = a > peak ? a : peak;
                    gain_now += dg;
                    lr[2u * (off + i)] = (int32_t)(l * gain_now * 8300000.0f);
                    lr[2u * (off + i) + 1u] = (int32_t)(r * gain_now * 8300000.0f);
                }
            }
        }
        {
            uint32_t j;
            for (j = 0; j < 12u; j++)
                pf[j] = fm_flush(pf[j]);
        }
        fa1 = fm_flush(fa1);
        fa2 = fm_flush(fa2);
        fb1 = fm_flush(fb1);
        fb2 = fm_flush(fb2);
        ga1 = fm_flush(ga1);
        ga2 = fm_flush(ga2);
        gb1 = fm_flush(gb1);
        gb2 = fm_flush(gb2);
        tp_l = fm_flush(tp_l);
        tp_r = fm_flush(tp_r);
        tp_dcl = fm_flush(tp_dcl);
        tp_dcr = fm_flush(tp_dcr);
        {
            uint32_t b, j;
            for (b = 0; b < 3u; b++)
                for (j = 0; j < 4u; j++)
                    eq_z[b][j] = eq_mix > 0.0f ? fm_flush(eq_z[b][j]) : 0.0f;
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
        prof(FR_PROF_SETUP);
    }
    if (bud_tps > 0.0f) {                /* what this render cost, smoothed: its ticks a sample, the voices' apart */
        float all = (float)(prof_clk() - t_in) / (float)n, v = (float)rv_ticks / (float)n;
        cost_base += (all - v - cost_base) * 0.1f;
        if (rv_vs)
            cost_voice += ((float)rv_ticks / (float)rv_vs - cost_voice) * 0.1f;
    }
    if (cut_at == 1) {                   /* CUT: this block out to silence */
        uint32_t i;
        for (i = 0; i < n; i++) {
            float g = 1.0f - (float)(i + 1u) / (float)n;
            lr[2u * i] = (int32_t)((float)lr[2u * i] * g);
            lr[2u * i + 1u] = (int32_t)((float)lr[2u * i + 1u] * g);
        }
        cut_at = 2;
    }
    if (prof_clk)
        prof_n += n;
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
void frasil_shed(int level) { shed = level < 0 ? 0 : level > 5 ? 5 : level; }
void frasil_profile(uint32_t (*clock)(void)) { prof_clk = clock; }
void frasil_budget(float ticks_a_sample) { bud_tps = prof_clk ? ticks_a_sample : 0.0f; }
void frasil_profile_read(uint32_t acc[FR_PROF_N], uint32_t *samples)
{
    uint32_t k;
    for (k = 0; k < FR_PROF_N; k++)
        acc[k] = prof_acc[k];
    *samples = prof_n;
}
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
