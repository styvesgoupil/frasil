/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* NUAGE: FRASIL's reverb (nuage.h).
 *
 * in L, R -> low cut -> pre-delay -> (+ shimmer) -> 4 allpasses each -> into the 8 lines (L the even ones, R the odd)
 *
 * The loop, a sample: each line read at its length (size, and a slow sine each, so the tail chorusses), through an
 * allpass of its own (the diffusion that turns echoes into a cloud), a one-pole low-pass (damping) and its gain
 * (decay), then an 8x8 Hadamard matrix back into the lines with the input. The outputs are taps on the lines before
 * the matrix: the even ones left, the odd ones right, signs alternating.
 *
 * Decay and damping per line from its length, so every path round the loop decays alike: RT60 from DECAY, and the
 * highs' own RT60 from DAMP (the one-pole's attenuation at Nyquist each pass is what the difference needs).
 *
 * The shimmer: the tail's sum, band-limited, into a two-tap pitch shifter (crossfaded windows), whose output goes
 * back in with the input. Each time round the loop it climbs the interval again.
 *
 * Memory: the lines, the pre-delay and the shifter share one 64 Ki x 16-bit buffer, one write position for all (each
 * element has a region; a sample ages towards the region's start); the allpasses are float (their states can
 * exceed the lines' range) in a smaller buffer the same way. */
#include "dsp.h"
#include "nuage.h"

#define NL 8
#define SUB 32u                          /* glides and the modulation's ramps, every SUB samples */
#define MEM_N 65536u
#define MEM_M (MEM_N - 1u)
#define FM_N 8192u
#define FM_M (FM_N - 1u)
#define QS 16384.0f                      /* a line's int16 is QS x: +-2.0 of headroom */
#define IQS (1.0f / 16384.0f)
#define MODMAX 40.0f                     /* the most the modulation lengthens a line, samples */
#define PD_MAX 8820u                     /* 200 ms */
#define SH_W 3072.0f                     /* the shifter's window, samples (70 ms) */
#define SH_N 3080u
#define OUT_G 0.85f

static int16_t mem[MEM_N] BR_POOL;
static float fmem[FM_N] BR_POOL;

static const uint16_t LINE_LEN[NL] = {2617, 2963, 3433, 3821, 4327, 4861, 5393, 6079};   /* at size 1: 59 .. 138 ms */
static const uint16_t LAP_LEN[NL] = {337, 397, 467, 541, 613, 691, 787, 863};
static const uint16_t DIF_LEN[2][4] = {{251, 191, 631, 479}, {263, 181, 673, 461}};
static const float LFO_HZ[NL] = {0.071f, 0.113f, 0.097f, 0.137f, 0.083f, 0.151f, 0.061f, 0.127f};

/* each element's region ends at its base: age a of it is at w + base - a */
static uint32_t line_base[NL], pd_base[2], sh_base;   /* in mem */
static uint32_t lap_base[NL], dif_base[2][4];         /* in fmem */
static uint32_t w;                                    /* the write position, one up a sample */

typedef struct {
    float g[NL], gt[NL];                 /* each line's gain a pass (with the matrix's 1 / sqrt 8), and its target */
    float da[NL], dat[NL];               /* damping: the one-pole's coefficient (1: none) */
    float lp[NL];                        /* damping states */
    float d[NL];                         /* each line's length now, samples */
    float ph[NL];                        /* modulation phases, 0..1 */
    float size, size_t;                  /* the lines' scale */
    float depth, depth_t;                /* modulation depth, samples (the line lengthens by 0 .. 2 depth) */
    float mrate;                         /* modulation rate multiplier */
    float gi, gi_t, gl, gl_t;            /* allpass coefficients: input, loop */
    float hpa, hpl, hpr;                 /* low cut: coefficient, states */
    float pd, pd_t;                      /* pre-delay, samples */
    float frz, frz_t;                    /* freeze, 0..1 */
    float sh, sh_t;                      /* shimmer gain */
    float sh_step, sh_p;                 /* the shifter's phase step a sample, its phase 0..1 */
    float sh_hp, sh_lp, sh_lp2, sh_out;  /* its band limit's states (a high-pass, two low-pass), its last output */
    float sha, sha_t;                    /* the low-pass's coefficient (SHIMMER's BRIGHT) */
    float wid, wid_t;
    float ee, ee_t;                      /* the diffused input straight out (an early cloud) */
    float rt60, rthf;                    /* the decay times the gains were made for */
} nuage_t;
static nuage_t nu;

#define NU_INLINE static inline __attribute__((always_inline))   /* (nuage_sub, in RAM, would call them) */
NU_INLINE float get(uint32_t base, uint32_t age) { return (float)mem[(w + base - age) & MEM_M]; }
NU_INLINE float geti(uint32_t base, float age)   /* age >= 0, linear interpolation */
{
    uint32_t k = (uint32_t)age;
    float f = age - (float)k, a = get(base, k), b = get(base, k + 1u);
    return a + (b - a) * f;
}
/* x in the lines' units, rounded to nearest (truncation would take up to an LSB a pass: quiet tails die early, and
 * grainy), by the float adder: 1.5 x 2^23 added leaves the integer in the low mantissa bits */
NU_INLINE void put(uint32_t base, float x)
{
    union { float f; int32_t i; } u;
    u.f = br_clampf(x, -32767.0f, 32767.0f) + 12582912.0f;
    mem[(w + base) & MEM_M] = (int16_t)(u.i - 0x4B400000);
}
NU_INLINE float getf(uint32_t base, uint32_t age) { return fmem[(w + base - age) & FM_M]; }
NU_INLINE void putf(uint32_t base, float x) { fmem[(w + base) & FM_M] = x; }

/* an allpass: v = x + g z, y = z - g v, with z = v delayed by len */
#define ALLPASS(base, len, g, x)                                                                                       \
    do {                                                                                                               \
        float z_ = getf((base), (len)), v_ = (x) + (g) * z_;                                                           \
        putf((base), v_);                                                                                              \
        (x) = z_ - (g) * v_;                                                                                           \
    } while (0)

static uint32_t region(uint32_t *top, uint32_t len)   /* the next region of len (+ room for interpolation): its base */
{
    *top += len + 3u;
    return *top;
}

/* each line's decay and damping for its length at the target size */
static void gains(void)
{
    uint32_t j;
    for (j = 0; j < NL; j++) {
        float len = (float)LINE_LEN[j] * nu.size_t + nu.depth_t + (float)LAP_LEN[j];   /* a pass, samples */
        float g = fm_exp2f(-9.965784f * len / (nu.rt60 * BR_FS));                       /* 10^(-3 len / (rt fs)) */
        float h = 1.0f;
        if (nu.rthf < nu.rt60)                                                         /* the highs' extra loss */
            h = fm_exp2f(-9.965784f * len * BR_INV_FS * (1.0f / nu.rthf - 1.0f / nu.rt60));
        nu.gt[j] = g * 0.35355339f;
        nu.dat[j] = 2.0f * h / (1.0f + h);   /* one-pole a / (2 - a) at Nyquist = h */
    }
}

void nuage_init(void)
{
    uint32_t j, c, top = 0, ftop = 0;
    for (j = 0; j < NL; j++)
        line_base[j] = region(&top, LINE_LEN[j] + (uint32_t)MODMAX + 2u);
    pd_base[0] = region(&top, PD_MAX + 2u);
    pd_base[1] = region(&top, PD_MAX + 2u);
    sh_base = region(&top, SH_N);
    for (c = 0; c < 2; c++)
        for (j = 0; j < 4; j++)
            dif_base[c][j] = region(&ftop, DIF_LEN[c][j]);
    for (j = 0; j < NL; j++)             /* (a sub-block's room before each: nuage_process runs a line's allpass over
                                          * the sub-block before the next one's, whose oldest would be the last's newest) */
        lap_base[j] = region(&ftop, LAP_LEN[j] + SUB);
    for (j = 0; j < MEM_N; j++)
        mem[j] = 0;
    for (j = 0; j < FM_N; j++)
        fmem[j] = 0.0f;
    {
        static const nuage_t zero;
        nu = zero;
    }
    nu.size = nu.size_t = 1.0f;
    nu.rt60 = 4.0f;
    nu.rthf = 2.0f;
    nu.wid = nu.wid_t = 1.0f;
    gains();
    for (j = 0; j < NL; j++) {
        nu.g[j] = nu.gt[j];
        nu.da[j] = nu.dat[j];
        nu.d[j] = (float)LINE_LEN[j];
        nu.ph[j] = (float)j * 0.125f;
    }
    nu.sh_step = -1.0f / SH_W;
}

void nuage_set(const nuage_params_t *p)
{
    float ratio;
    nu.size_t = 0.15f * fm_powf(1.0f / 0.15f, br_clampf(p->size, 0.0f, 1.0f));
    nu.rt60 = 0.4f * fm_powf(150.0f, br_clampf(p->decay, 0.0f, 1.0f));
    nu.rthf = p->damp >= 0.995f ? 1e9f : 0.25f * fm_powf(160.0f, br_clampf(p->damp, 0.0f, 1.0f));
    nu.depth_t = 0.5f * MODMAX * p->mod * p->mod;
    gains();
    nu.mrate = 0.6f + 1.6f * p->mod;
    nu.gi_t = 0.5f + 0.3f * p->diffusion;
    nu.gl_t = 0.25f + 0.45f * p->diffusion;
    nu.hpa = 6.2831853f * 20.0f * fm_powf(25.0f, br_clampf(p->lowcut, 0.0f, 1.0f)) * BR_INV_FS;
    nu.pd_t = (float)(uint32_t)(br_clampf(p->predelay, 0.0f, 1.0f) * (float)PD_MAX);
    nu.frz_t = p->freeze ? 1.0f : 0.0f;
    nu.sh_t = 1.2f * br_clampf(p->shimmer, 0.0f, 1.0f);
    nu.sha_t = 1.0f - fm_expf(-6.2831853f * 700.0f * fm_powf(11.43f, br_clampf(p->shimmer_tone, 0.0f, 1.0f)) * BR_INV_FS);
    ratio = fm_exp2f(p->interval * (1.0f / 12.0f));
    nu.sh_step = (1.0f - ratio) / SH_W;
    nu.wid_t = br_clampf(p->width, 0.0f, 1.0f);
    nu.ee_t = 0.35f * br_clampf(p->diffusion, 0.0f, 1.0f);
}

/* the stages of a sub-block (nuage_process), each a short loop over its SUB samples */
static float nb_y[NL][SUB];              /* each line's output, damped and decayed */
static float nb_xl[SUB], nb_xr[SUB];     /* the input as it goes into the lines (the diffused, halved) */
static float nb_el[SUB], nb_er[SUB];     /* the diffused input straight out */
static float nb_sh[SUB];                 /* the shimmer's output after each sample (the next one's input) */

NU_INLINE float nu_fabs(float x) { return x < 0.0f ? -x : x; }   /* (for its comparisons: -0 as 0) */
static float nb_sn[NL];                  /* the sub-block's LFO sines (nuage_process: fm_sinf is a call) */

/* a sub-block from i0, its LFOs' sines in nb_sn: the whole of it but them, from RAM on the device (BR_HOT) */
static BR_HOT void nuage_sub(const float *in_l, const float *in_r, float *out_l, float *out_r, uint32_t i0)
{
    /* the loop runs in the lines' units (QS x): no scaling at each read and write. A sub-block goes in stages, each a
     * loop over its samples with few values to hold (on the FM-1 the FPU's registers are the CPU's sixteen: the loop
     * as one spilled at every turn): every line, read, is at least SUB samples behind what the sub-block writes (the
     * shortest line some 390, an allpass 181), so the lines are read for the whole sub-block first; then the shimmer
     * from what they gave (its output the next sample's input), the input, its diffusion, and the matrix back into the
     * lines with the input, and out. Sample for sample the same sums, in the same order: the same tail, bit for bit */
    uint32_t i, j;
    const uint32_t pd0 = pd_base[0], pd1 = pd_base[1], shb = sh_base;
    uint32_t lb[NL], lab[NL], db[2][4];
    for (j = 0; j < NL; j++) {
        lb[j] = line_base[j];
        lab[j] = lap_base[j] - LAP_LEN[j];   /* an allpass reads at age LAP_LEN: base - age folded */
    }
    for (j = 0; j < 4; j++) {
        db[0][j] = dif_base[0][j];
        db[1][j] = dif_base[1][j];
    }
    {
        float d[NL], dd[NL], g[NL], da[NL], lp[NL], gi, gi2, gl, ing, sh, wid, ee, pd, pd_step, hpa, hpl, hpr;
        float sh_out, sh_hp, sh_lp, sh_lp2, sha, sh_p, sh_step, inq, outg;
        const uint32_t ww = w;
        uint32_t pdi;
        int shim;
        /* the glides, a sub-block at a time */
        nu.size += (nu.size_t - nu.size) * 0.002f;      /* ~0.4 s: the space bends as it grows */
        nu.depth += (nu.depth_t - nu.depth) * 0.01f;
        nu.frz += (nu.frz_t - nu.frz) * 0.03f;          /* ~0.1 s */
        nu.sh += (nu.sh_t - nu.sh) * 0.01f;
        nu.sha += (nu.sha_t - nu.sha) * 0.02f;
        nu.gi += (nu.gi_t - nu.gi) * 0.02f;
        nu.gl += (nu.gl_t - nu.gl) * 0.02f;
        nu.wid += (nu.wid_t - nu.wid) * 0.02f;
        nu.ee += (nu.ee_t - nu.ee) * 0.02f;
        for (j = 0; j < NL; j++) {
            float to;
            nu.g[j] += (nu.gt[j] - nu.g[j]) * 0.02f;
            nu.da[j] += (nu.dat[j] - nu.da[j]) * 0.02f;
            g[j] = nu.g[j] + (0.99995f * 0.35355339f - nu.g[j]) * nu.frz;
            da[j] = nu.da[j] + (1.0f - nu.da[j]) * nu.frz;
            to = (float)LINE_LEN[j] * nu.size + nu.depth * (1.0f + nb_sn[j]);
            d[j] = nu.d[j];
            dd[j] = (to - d[j]) * (1.0f / (float)SUB);
            lp[j] = nu.lp[j] > -1e-12f && nu.lp[j] < 1e-12f ? 0.0f : nu.lp[j];   /* no denormals */
        }
        gi = nu.gi;
        gi2 = gi * 0.85f;
        gl = nu.gl;
        ing = 1.0f - nu.frz;
        inq = ing * QS;
        sh = nu.sh;
        shim = sh > 1e-4f || nu.sh_out != 0.0f;
        wid = nu.wid * 0.5f;
        ee = nu.ee;
        outg = OUT_G * IQS;
        hpa = nu.hpa;
        hpl = nu.hpl > -1e-15f && nu.hpl < 1e-15f ? 0.0f : nu.hpl;
        hpr = nu.hpr > -1e-15f && nu.hpr < 1e-15f ? 0.0f : nu.hpr;
        pd = nu.pd;
        if (nu_fabs(nu.pd_t - pd) < 0.01f)
            pd = nu.pd_t;
        pd_step = br_clampf(nu.pd_t - pd, -0.5f * (float)SUB, 0.5f * (float)SUB) * (1.0f / (float)SUB);
        pdi = (uint32_t)pd;
        sh_out = nu.sh_out;
        sh_hp = nu.sh_hp;
        sh_lp = nu.sh_lp;
        sh_lp2 = nu.sh_lp2;
        sha = nu.sha;
        sh_p = nu.sh_p;
        sh_step = nu.sh_step;

        /* the lines: read, diffuse, damp, decay (a line at a time) */
        for (j = 0; j < NL; j++) {
            const uint32_t b = lb[j], za = lab[j], ua = lab[j] + LAP_LEN[j];
            float dj = d[j], ddj = dd[j], lj = lp[j], daj = da[j], gj = g[j], *y = nb_y[j];
            for (i = 0; i < SUB; i++) {
                float v, z, u;
                w = ww + i;
                v = geti(b, dj += ddj);
                z = fmem[(ww + i + za) & FM_M];
                u = v + gl * z;
                fmem[(ww + i + ua) & FM_M] = u;
                v = z - gl * u;
                lj += daj * (v - lj);
                y[i] = lj * gj;
            }
            d[j] = dj;
            lp[j] = lj;
        }
        /* the shimmer: the tail up an interval, each sample's for the next one's input */
        if (shim) {
            for (i = 0; i < SUB; i++) {
                float ol = nb_y[0][i] - nb_y[2][i] + nb_y[4][i] - nb_y[6][i];
                float orr = nb_y[1][i] - nb_y[3][i] + nb_y[5][i] - nb_y[7][i];
                float x = (ol + orr) * 0.5f, p2, t, w1;
                w = ww + i;
                sh_hp += 0.02f * (x - sh_hp);           /* ~140 Hz high-pass */
                sh_lp += sha * (x - sh_hp - sh_lp);     /* BRIGHT: two one-poles, 700 Hz .. 8 kHz */
                sh_lp2 += sha * (sh_lp - sh_lp2);
                put(shb, sh_lp2);
                sh_p += sh_step;
                if (sh_p < 0.0f)
                    sh_p += 1.0f;
                else if (sh_p >= 1.0f)
                    sh_p -= 1.0f;
                p2 = sh_p + 0.5f;
                if (p2 >= 1.0f)
                    p2 -= 1.0f;
                t = 1.0f - nu_fabs(2.0f * sh_p - 1.0f);   /* 0 where the tap wraps, 1 half way */
                w1 = t * t * (3.0f - 2.0f * t);           /* and the other tap 1 - w1 */
                x = w1 * geti(shb, sh_p * SH_W + 1.0f) + (1.0f - w1) * geti(shb, p2 * SH_W + 1.0f);
                nb_sh[i] = sh_out = br_soft(x * sh * IQS) * inq;
            }
        }
        /* the input: low cut, pre-delay (interpolated only while it glides), the shimmer back in */
        for (i = 0; i < SUB; i++) {
            float xl, xr, si = shim && i ? nb_sh[i - 1] : nu.sh_out;
            w = ww + i;
            hpl += hpa * (in_l[i0 + i] - hpl);
            hpr += hpa * (in_r[i0 + i] - hpr);
            put(pd0, (in_l[i0 + i] - hpl) * inq);
            put(pd1, (in_r[i0 + i] - hpr) * inq);
            if (pd_step != 0.0f) {
                pd += pd_step;
                xl = geti(pd0, pd);
                xr = geti(pd1, pd);
            } else {
                xl = get(pd0, pdi);
                xr = get(pd1, pdi);
            }
            nb_xl[i] = xl + si;
            nb_xr[i] = xr + si;
        }
        /* its diffusion (the sides' allpasses in turn, a sample at a time: their regions are next to each other) */
        for (i = 0; i < SUB; i++) {
            float xl = nb_xl[i], xr = nb_xr[i];
            w = ww + i;
            ALLPASS(db[0][0], DIF_LEN[0][0], gi, xl);
            ALLPASS(db[1][0], DIF_LEN[1][0], gi, xr);
            ALLPASS(db[0][1], DIF_LEN[0][1], gi, xl);
            ALLPASS(db[1][1], DIF_LEN[1][1], gi, xr);
            ALLPASS(db[0][2], DIF_LEN[0][2], gi2, xl);
            ALLPASS(db[1][2], DIF_LEN[1][2], gi2, xr);
            ALLPASS(db[0][3], DIF_LEN[0][3], gi2, xl);
            ALLPASS(db[1][3], DIF_LEN[1][3], gi2, xr);
            nb_el[i] = xl * ee;
            nb_er[i] = xr * ee;
            nb_xl[i] = xl * 0.5f;
            nb_xr[i] = xr * 0.5f;
        }
        /* the matrix (8-point Walsh-Hadamard; its 1 / sqrt 8 is in g) back into the lines with the input; out, with
         * the width */
        for (i = 0; i < SUB; i++) {
            float y0 = nb_y[0][i], y1 = nb_y[1][i], y2 = nb_y[2][i], y3 = nb_y[3][i];
            float y4 = nb_y[4][i], y5 = nb_y[5][i], y6 = nb_y[6][i], y7 = nb_y[7][i], xl = nb_xl[i], xr = nb_xr[i];
            float ol = y0 - y2 + y4 - y6, orr = y1 - y3 + y5 - y7, m, s;
            float a0 = y0 + y1, a1 = y0 - y1, a2 = y2 + y3, a3 = y2 - y3;
            float a4 = y4 + y5, a5 = y4 - y5, a6 = y6 + y7, a7 = y6 - y7;
            float b0 = a0 + a2, b2 = a0 - a2, b1 = a1 + a3, b3 = a1 - a3;
            float b4 = a4 + a6, b6 = a4 - a6, b5 = a5 + a7, b7 = a5 - a7;
            w = ww + i;
            put(lb[0], b0 + b4 + xl);
            put(lb[1], b1 + b5 + xr);
            put(lb[2], b2 + b6 - xl);
            put(lb[3], b3 + b7 - xr);
            put(lb[4], b0 - b4 + xl);
            put(lb[5], b1 - b5 + xr);
            put(lb[6], b2 - b6 - xl);
            put(lb[7], b3 - b7 - xr);
            m = (ol + orr + nb_el[i] + nb_er[i]) * 0.5f;
            s = (ol - orr + nb_el[i] - nb_er[i]) * wid;
            out_l[i0 + i] = (m + s) * outg;
            out_r[i0 + i] = (m - s) * outg;
        }
        w = ww + SUB;
        for (j = 0; j < NL; j++) {
            nu.d[j] = d[j];
            nu.lp[j] = lp[j];
        }
        nu.hpl = hpl;
        nu.hpr = hpr;
        nu.pd = pd;
        if (!(sh > 1e-4f) && nu_fabs(sh_out) < 1e-6f * QS)
            sh_out = 0.0f;
        nu.sh_out = sh_out;
        nu.sh_hp = sh_hp;
        nu.sh_lp = sh_lp;
        nu.sh_lp2 = sh_lp2;
        nu.sh_p = sh_p;
    }
}

/* the room emptied: its memories silent, its settings kept (the render's, between blocks: FRASIL's CUT) */
void nuage_clear(void)
{
    uint32_t j;
    for (j = 0; j < MEM_N; j++)
        mem[j] = 0;
    for (j = 0; j < FM_N; j++)
        fmem[j] = 0.0f;
    for (j = 0; j < SUB; j++)
        nb_sh[j] = 0.0f;
}

void nuage_process(const float *in_l, const float *in_r, float *out_l, float *out_r, uint32_t n)
{
    static void (*volatile sub)(const float *, const float *, float *, float *, uint32_t) = nuage_sub;   /* (from the
                                                                                                    * flash, RAM is
                                                                                                    * out of a call's
                                                                                                    * reach) */
    uint32_t i0, j;
    for (i0 = 0; i0 < n; i0 += SUB) {
        for (j = 0; j < NL; j++) {       /* the lines' LFOs, a sub-block's step */
            nu.ph[j] += LFO_HZ[j] * nu.mrate * ((float)SUB * BR_INV_FS);
            if (nu.ph[j] >= 1.0f)
                nu.ph[j] -= 1.0f;
            nb_sn[j] = fm_sinf(6.2831853f * nu.ph[j]);
        }
        sub(in_l, in_r, out_l, out_r, i0);
    }
}
