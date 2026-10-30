/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* GLINT (glint.h). Phases are 32-bit (they wrap by themselves); the sine is a 2048-point table with linear
 * interpolation; the envelopes are per-sample multiplies: the amplitude over DECAY to silence, or while the key is
 * down to SUSTAIN's level, over RELEASE once the key is up, the index from bright at the strike down to half (a ping,
 * then a softer ring). */
#include "dsp.h"
#include "glint.h"

#define TBITS 11u
#define TN (1u << TBITS)
#define SPREAD 0.63f                    /* each glint's place in the field (either way) and its detune (8 cents a
                                         * whole), at random: SCATTER's default, fixed since it went */
#define IDX_UNIT 2670176.8f             /* the index in 2^-24 turns a radian (2^24 / 2 pi): idx x m stays well inside
                                         * an int32, then << 8 is a phase offset that wraps by itself */

static float sintab[TN + 1u];

typedef struct {
    int8_t note;                         /* as it was played (OCTAVE not on it) */
    uint8_t gate;
    int8_t octv;                         /* OCTAVE as it is on it now */
    float f0, ratio;                     /* its frequency before OCTAVE (its detune on it), RATIO at the strike */
    uint32_t pc, pm, dc, dm;             /* carrier and modulator: phases, increments */
    float amp, ka, kr;                   /* amplitude, its decay a sample (held), and once let go (RELEASE) */
    float amp0;                          /* its level at the strike (SUSTAIN's share of it is held) */
    float att, katt;                     /* the onset, 0 .. 1, and its step a sample (ATTACK) */
    float idx, idx_end, ki;              /* the index (in phase units), where it settles, its decay a sample */
    float gl, gr;
    float prs, pg;                       /* pressure 0..1, and the gain it gives now (gliding to 1 + 1.5 prs) */
    float pf, pa;                        /* the pressure smoothed (~15 ms), and its slow average (~200 ms): VIBRATO bends
                                          * the pitch by their difference, the finger's wiggle */
    float cents;                         /* the bend now (the screen) */
    uint8_t landed, calm;                /* the finger has pressed in and settled (100 ms steady): from then on, its
                                          * changes are the wiggle; let go to nothing, it lands again */
    uint32_t age, blocks;
    float t, toff;                       /* seconds since the strike, since let go (the screen) */
} glint_t;

static glint_t sp[GLINT_NV];
static int sounding;                     /* a glint sounded in the last glint_render (glint_delay: else its input is 0) */
static uint32_t sp_age, sp_rng = 0x6C8E9CF5u;

static inline float tsin(uint32_t ph)
{
    uint32_t i = ph >> (32u - TBITS);
    float f = (float)(ph & ((1u << (32u - TBITS)) - 1u)) * (1.0f / (float)(1u << (32u - TBITS)));
    return sintab[i] + (sintab[i + 1u] - sintab[i]) * f;
}

void glint_init(void)
{
    uint32_t i;
    for (i = 0; i <= TN; i++)
        sintab[i] = fm_sinf(6.2831853f * (float)i / (float)TN);
    for (i = 0; i < GLINT_NV; i++)
        sp[i].note = -1;
}

static float decay_k(float seconds) { return fm_exp2f(-9.965784f / (seconds * BR_FS)); }   /* -60 dB over it */

/* a glint's increments, as fractions of a turn, OCTAVE on it (kept under 0.45: no wrap past Nyquist, and inside an
 * int32) */
static void tune(glint_t *s, int octave)
{
    float f = s->f0 * fm_exp2f((float)octave);
    s->octv = (int8_t)octave;
    s->dc = (uint32_t)(int32_t)(br_clampf(f * BR_INV_FS, 0.0f, 0.45f) * 4294967296.0f);
    s->dm = (uint32_t)(int32_t)(br_clampf(f * s->ratio * BR_INV_FS, 0.0f, 0.45f) * 4294967296.0f);
}

void glint_on(int note, float vel, const glint_params_t *p)
{
    glint_t *s = 0;
    uint32_t i;
    float f, t, pan, det;
    for (i = 0; i < GLINT_NV && !s; i++)   /* a free one, else the quietest */
        if (sp[i].note < 0)
            s = &sp[i];
    if (!s)
        for (i = 0; i < GLINT_NV; i++)
            if (!s || sp[i].amp < s->amp)
                s = &sp[i];
    det = br_randf(&sp_rng) * SPREAD * 8.0f;                     /* cents */
    pan = br_randf(&sp_rng) * SPREAD;
    f = 440.0f * fm_exp2f(((float)note - 69.0f + det * 0.01f) * (1.0f / 12.0f));
    t = 0.08f * fm_powf(75.0f, br_clampf(p->decay, 0.0f, 1.0f));
    s->note = (int8_t)note;
    s->gate = 1;
    s->f0 = f;
    s->ratio = p->ratio;
    tune(s, p->octave);
    s->pc = 0;
    s->pm = 0;
    s->amp = s->amp0 = (0.35f + 0.65f * vel) * 0.3f;
    s->att = 0.0f;
    s->ka = decay_k(t);
    s->kr = decay_k(0.02f * fm_powf(300.0f, br_clampf(p->release, 0.0f, 1.0f)));   /* let go: RELEASE */
    {                                    /* the index: from the strike (PING brighter) down to the ring */
        float base = p->bright * p->bright * 5.0f * IDX_UNIT, pg = br_clampf(p->ping, 0.0f, 1.0f);
        s->idx = base * (1.0f + 1.5f * pg);
        s->idx_end = base * (1.0f - 0.85f * pg);
        s->ki = decay_k(0.03f + t * 0.2f * (1.0f - 0.5f * pg));
    }
    {                                    /* the onset: ATTACK, a third of it a time constant */
        float ta = 0.002f * fm_powf(750.0f, br_clampf(p->attack, 0.0f, 1.0f));   /* 2 ms (no click) .. 1.5 s */
        s->katt = 1.0f - fm_expf(-3.0f / (ta * BR_FS));
    }
    s->gl = fm_sqrtf(0.5f * (1.0f - pan));
    s->gr = fm_sqrtf(0.5f * (1.0f + pan));
    s->prs = 0.0f;
    s->pg = 1.0f;
    s->pf = s->pa = 0.0f;
    s->cents = 0.0f;
    s->landed = s->calm = 0;
    s->age = ++sp_age;
    s->blocks = 0;
    s->t = s->toff = 0.0f;
}

void glint_off(int note, const glint_params_t *p)
{
    uint32_t i;
    for (i = 0; i < GLINT_NV; i++)
        if ((note < 0 || sp[i].note == note) && sp[i].gate) {   /* (-1: every one) */
            sp[i].gate = 0;
            sp[i].toff = 0.0f;
            sp[i].kr = decay_k(0.02f * fm_powf(300.0f, br_clampf(p->release, 0.0f, 1.0f)));   /* (RELEASE now) */
        }
}

void glint_press(int note, float p)
{
    uint32_t i;
    for (i = 0; i < GLINT_NV; i++)
        if (sp[i].note == note) {
            sp[i].prs = br_clampf(p, 0.0f, 1.0f);
            if (!sp[i].blocks) {         /* pressed from the strike (an arpeggio's step): landed already */
                sp[i].pf = sp[i].pa = sp[i].prs;
                sp[i].landed = sp[i].prs >= 0.03f;
            }
        }
}

void glint_render(float *l, float *r, float *send_l, float *send_r, uint32_t n, const glint_params_t *p)
{
    uint32_t i, j;
    float lv = p->level, snd = p->space;
    sounding = 0;
    for (j = 0; j < GLINT_NV; j++) {
        glint_t *s = &sp[j];
        uint32_t pc, pm, dm, dc;
        float amp, att, katt, idx, ie, k, ki, gl, gr, pg, dpg, fl;
        if (s->note < 0)
            continue;
        sounding = 1;
        if (s->octv != p->octave)        /* OCTAVE turned: it sounds there at once */
            tune(s, p->octave);
        s->blocks++;
        s->t += (float)n * BR_INV_FS;
        if (!s->gate)
            s->toff += (float)n * BR_INV_FS;
        /* pressure: louder (up to +8 dB, gliding across the block), and while the key is held the bell rings on
         * (its decay slowed as it is pressed) */
        pg = s->pg;
        dpg = (1.0f + 1.5f * s->prs - pg) * (1.0f / (float)n);
        s->pg = 1.0f + 1.5f * s->prs;
        dc = s->dc;
        pc = s->pc;
        pm = s->pm;
        dm = s->dm;
        s->pf += (s->prs - s->pf) * 0.3f;
        if (s->prs < 0.03f)
            s->landed = s->calm = 0;
        if (!s->landed) {                /* pressing in: not a wiggle */
            s->calm = s->pf > 0.05f && fm_fabsf(s->prs - s->pf) < 0.03f ? (uint8_t)(s->calm + 1u) : 0u;
            s->landed = s->calm >= 17u;
            s->pa = s->pf;
        } else if (s->prs < 0.7f * s->pa) {   /* letting go (a wiggle stays within 30 %): no dip on the way out */
            s->pa += (s->pf - s->pa) * 0.4f;
        } else {
            s->pa += (s->pf - s->pa) * 0.03f;
        }
        s->cents = 0.0f;
        if (p->vibrato > 0.001f) {       /* VIBRATO: both operators bent together, a block at a time */
            float bend;
            s->cents = p->vibrato * 25.0f * br_clampf((s->pf - s->pa) * 4.0f, -1.0f, 1.0f);
            bend = fm_exp2f(s->cents * (1.0f / 1200.0f));
            dc = (uint32_t)(int32_t)((float)(int32_t)dc * bend);
            dm = (uint32_t)(int32_t)((float)(int32_t)dm * bend);
        }
        amp = s->amp;
        att = s->att;
        katt = s->katt;
        idx = s->idx;
        ie = s->idx_end;
        k = s->gate ? (s->prs > 0.01f ? fm_exp2f(fm_log2f(s->ka) * (1.0f - 0.85f * s->prs)) : s->ka) : s->kr;
        fl = s->gate ? s->amp0 * p->sustain : 0.0f;   /* held: down to SUSTAIN's level, not to silence */
        ki = s->ki;
        gl = s->gl * lv;
        gr = s->gr * lv;
        for (i = 0; i < n; i++) {
            float m = tsin(pm), y;
            pm += dm;
            pg += dpg;
            y = tsin(pc + ((uint32_t)(int32_t)(idx * m) << 8)) * amp * att * pg;
            pc += dc;
            att += (1.0f - att) * katt;
            amp = fl + (amp - fl) * k;
            idx = ie + (idx - ie) * ki;
            l[i] += y * gl;
            r[i] += y * gr;
            send_l[i] += y * gl * snd;
            send_r[i] += y * gr * snd;
        }
        s->pc = pc;
        s->pm = pm;
        s->amp = amp;
        s->att = att;
        s->idx = idx;
        if (amp < 1e-5f)
            s->note = -1;
    }
}

void glint_view(glint_view_t v[GLINT_NV])
{
    uint32_t i;
    for (i = 0; i < GLINT_NV; i++) {
        v[i].note = sp[i].note < 0 ? -1 : (int8_t)(sp[i].note + 12 * sp[i].octv > 127 ? 127 : sp[i].note + 12 * sp[i].octv);
        v[i].gate = sp[i].gate;
        v[i].level = sp[i].amp * sp[i].pg * (1.0f / 0.3f);
        v[i].env = sp[i].amp0 > 0.0f ? sp[i].att * sp[i].amp / sp[i].amp0 : 0.0f;
        v[i].t = sp[i].t;
        v[i].toff = sp[i].toff;
        v[i].age = sp[i].blocks;
        v[i].cents = sp[i].cents;
    }
}

/* ------------------------------------------------------------- the delay --- */
/* Two lines at half the rate (the echoes a little darker each time round, and twice the time in the memory), 16-bit;
 * ping-pong: left = the input and the right's return, right = the left's; mono: one line, both sides. The feedback
 * through a low-pass (~4.5 kHz) and a high-pass (~120 Hz): the repeats stay clear. The time glides (a tape's bend,
 * no click) */
#define DL_N 20480u                      /* each line, half-rate samples: 0.93 s */
#define DL_Q 16384.0f
static int16_t dl[2][DL_N] BR_POOL;

/* every glint silent and its echoes gone (the render's, between blocks: FRASIL's CUT) */
void glint_cut(void)
{
    uint32_t i;
    for (i = 0; i < GLINT_NV; i++)
        sp[i].note = -1;
    for (i = 0; i < DL_N; i++)
        dl[0][i] = dl[1][i] = 0;
}
static struct {
    uint32_t w;
    uint32_t zrun;                       /* how many steps the lines have been written nothing but 0 (DL_N on: they
                                          * hold nothing) */
    int pp;                              /* the ping-pong it counted them in (mono writes the left line alone) */
    float d;                             /* the time now, half-rate samples */
    float lp[2], hp[2];                  /* the feedback's filters, a line each */
    float prev[2];                       /* the last output, for the upsampling */
    float mix;
} dly;

static inline float dl_get(uint32_t c, float age)   /* (age < DL_N: no modulo, the FM-1's division is slow) */
{
    uint32_t k = (uint32_t)age, i = dly.w + DL_N - k, j;
    float f = age - (float)k;
    i -= i >= DL_N ? DL_N : 0u;
    j = i ? i - 1u : DL_N - 1u;
    return ((float)dl[c][i] + ((float)dl[c][j] - (float)dl[c][i]) * f) * (1.0f / DL_Q);
}

static inline int dl_put(uint32_t c, float x)   /* 1: it wrote 0 */
{
    x = br_clampf(x * DL_Q, -32767.0f, 32767.0f);
    dl[c][dly.w] = (int16_t)x;
    return dl[c][dly.w] == 0;
}

void glint_delay(float *l, float *r, float *send_l, float *send_r, uint32_t n, const glint_params_t *p)
{
    uint32_t i, c;
    float target = br_clampf(p->delay_s, 0.02f, GLINT_DELAY_MAX) * (BR_FS * 0.5f);
    float fb = br_clampf(p->delay_fb, 0.0f, 1.0f) * 0.88f, snd = p->space;
    dly.mix += (br_clampf(p->delay_mix, 0.0f, 1.0f) - dly.mix) * 0.1f;
    if (dly.mix < 0.001f && fb < 0.001f)
        return;
    if (dly.d < 1.0f)
        dly.d = target;
    if (p->delay_pp != dly.pp) {         /* (the right line, unwritten in mono, may hold echoes: count again) */
        dly.pp = p->delay_pp;
        dly.zrun = 0;
    }
    if (!sounding && dly.zrun >= DL_N && dly.lp[0] == 0.0f && dly.lp[1] == 0.0f && dly.hp[0] == 0.0f &&
        dly.hp[1] == 0.0f && dly.prev[0] == 0.0f && dly.prev[1] == 0.0f) {
        for (i = 0; i + 1u < n; i += 2u) /* (nothing in, nothing in the lines: nothing out, the time gliding on) */
            dly.d += (target - dly.d) * 0.0015f;
        return;
    }
    for (i = 0; i + 1u < n; i += 2u) {
        float x = (l[i] + l[i + 1u] + r[i] + r[i + 1u]) * 0.25f, y[2];
        dly.d += (target - dly.d) * 0.0015f;
        y[0] = dl_get(0, dly.d);
        y[1] = p->delay_pp ? dl_get(1, dly.d) : y[0];
        for (c = 0; c < 2u; c++) {       /* the feedback's filters, on what goes back in */
            float z = c ? (p->delay_pp ? y[0] : y[1]) : (p->delay_pp ? y[1] : y[0]);
            dly.lp[c] += 0.72f * (z - dly.lp[c]);
            dly.hp[c] += 0.034f * (dly.lp[c] - dly.hp[c]);
        }
        if (p->delay_pp) {               /* left: the input and the right coming back; right: the left */
            int z = dl_put(0, x + fb * (dly.lp[0] - dly.hp[0]));
            z &= dl_put(1, fb * (dly.lp[1] - dly.hp[1]));
            dly.zrun = z ? dly.zrun + (dly.zrun < DL_N) : 0u;
        } else {
            dly.zrun = dl_put(0, x + fb * (dly.lp[0] - dly.hp[0])) ? dly.zrun + (dly.zrun < DL_N) : 0u;
        }
        dly.w = dly.w + 1u < DL_N ? dly.w + 1u : 0u;
        for (c = 0; c < 2u; c++) {       /* back to the full rate: a step between, then the sample */
            float a = (dly.prev[c] + y[c]) * 0.5f * dly.mix, b = y[c] * dly.mix;
            float *o = c ? r : l, *sd = c ? send_r : send_l;
            o[i] += a;
            o[i + 1u] += b;
            sd[i] += a * snd;
            sd[i + 1u] += b * snd;
            dly.prev[c] = y[c];
        }
    }
    for (c = 0; c < 2u; c++)
        if (dly.hp[c] > -1e-15f && dly.hp[c] < 1e-15f)
            dly.hp[c] = dly.lp[c] = 0.0f;
}

