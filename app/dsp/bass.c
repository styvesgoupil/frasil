/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* BASS (bass.h). The phases are 32 bits (they wrap by themselves); a sine from one is a parabola refined once (no
 * table, 0.1 % off); the square and the saws are PolyBLEP's (their steps smoothed, no aliasing to speak of this
 * low). A chunk of 32 frames at a time: the model's wave into a buffer, then DRIVE, the low-pass (a TPT state
 * variable filter, 12 dB an octave, its coefficients a chunk) and the gain. The pitch glides in octaves a chunk at a
 * time; the envelope, the level and the pressure ramp across a chunk, as the drones' do. The envelope: ATTACK up to the
 * top (95 %, at the time ATTACK shows), then DECAY to SUSTAIN while held (at 100 %, on up as the attack went: the bass
 * as it was before DECAY and SUSTAIN), RELEASE once let go; each step a chunk's share of the way left. A new MODEL comes
 * in through a dip: the old one out over four chunks, the new one in over four (6 ms in all), no click. */
#include "dsp.h"
#include "bass.h"

#define CH 32u
#define BASS_G 0.42f                     /* a full bass beside the drones */

static struct {
    int8_t note;                         /* -1: silent; else as it is played (OCTAVE not on it) */
    int8_t octv;                         /* OCTAVE as it is now on it */
    uint8_t gate;
    uint8_t dec;                         /* held, the attack done: DECAY toward SUSTAIN */
    float ts;                            /* seconds in its stage: ATTACK, DECAY (dec), RELEASE (the screen) */
    uint8_t let_dec;                     /* let go: in its DECAY then, and how long it had been in its stage */
    float let_ts;
    uint32_t again;                      /* PULSE's gap: the chunks till the note sounds again (0: none) */
    uint32_t pulses;                     /* the notes played again so (bass_view) */
    uint32_t ons;                        /* the notes struck (bass_view) */
    int model;                           /* the one sounding */
    float mg;                            /* its share, 0..1: a new model comes in through a dip of 6 ms */
    float vel, vel_t;                    /* velocity's share of the level, gliding (a legato note's: no step) */
    float oct, oct_t;                    /* the pitch now and where it glides, in octaves above 440 Hz */
    uint32_t ph, ph2, phs;               /* the phases: the note, REESE's second saw, SUB's octave below */
    float env, lvl;                      /* the envelope 0..1 (the gain its square), the gain at the last chunk's end */
    float prs, prs_t;
    float s1, s2;                        /* the low-pass's two states */
    float flt, res, drv;                 /* FILTER, RESONANCE and DRIVE as they glide (~7 ms: an LFO's steps smoothed) */
    uint8_t set;                         /* (they have a value: from silence, at once) */
} b = {.note = -1, .mg = 1.0f};

/* each model's level, so that they sound about as loud as the sine through FILTER's default */
static const float MODEL_G[BASS_NMODEL] = {1.0f, 1.1f, 0.62f, 1.0f, 1.6f, 1.0f, 1.1f, 1.15f};

void bass_init(void)
{
    b.note = -1;
    b.again = 0;
}

static int sounding(int note, int octave)   /* a note with OCTAVE on it, C1 at the lowest (under it, nothing a
                                             * speaker plays) */
{
    note += 12 * octave;
    while (note < 24)
        note += 12;
    return note > 127 ? 127 : note;
}

void bass_on(int note, float vel, const bass_params_t *p)
{
    float o = ((float)sounding(note, p->octave) - 69.0f) * (1.0f / 12.0f);
    if (b.note < 0) {                    /* from silence: no glide from nowhere, the phases from zero */
        b.oct = o;
        b.ph = b.ph2 = b.phs = 0;
        b.env = b.lvl = 0.0f;
        b.prs = b.prs_t = 0.0f;
        b.set = 0;
        b.s1 = b.s2 = 0.0f;
        b.vel = vel;
    }
    b.oct_t = o;
    b.note = (int8_t)note;
    b.octv = (int8_t)p->octave;
    b.ons++;
    b.gate = 1;
    b.dec = 0;                           /* (a note played: from its ATTACK, a legato one too) */
    b.ts = 0.0f;
    b.again = 0;
    b.vel_t = vel;
}

static void let_go(void)                 /* (where the envelope was, for the screen) */
{
    if (b.gate) {
        b.let_dec = b.dec;
        b.let_ts = b.ts;
        b.ts = 0.0f;
    }
}

void bass_off(void)
{
    let_go();
    b.gate = 0;
    b.again = 0;
    b.prs_t = 0.0f;                      /* the next note starts unpressed */
}

/* PULSE: let go (RELEASE) for a moment, then again from ATTACK; its note, velocity and pressure as they were, nothing to
 * glide */
void bass_pulse(float gap)
{
    uint32_t n = (uint32_t)(gap * BR_FS * (1.0f / (float)CH) + 0.5f);
    if (b.note < 0 || !b.gate)
        return;                          /* (let go already: nothing to play again) */
    let_go();
    b.gate = 0;
    b.again = n ? n : 1u;
}

void bass_retune(int note, float vel)
{
    if (b.note < 0)
        return;
    b.note = (int8_t)note;
    b.oct_t = ((float)sounding(note, b.octv) - 69.0f) * (1.0f / 12.0f);
    b.vel_t = vel;
}

void bass_press(float p) { b.prs_t = br_clampf(p, 0.0f, 1.0f); }

/* sin(2 pi ph / 2^32) */
static inline float psin(uint32_t ph)
{
    float x = (float)(int32_t)ph * (1.0f / 2147483648.0f), y;   /* -1 .. 1: half a turn either way */
    y = 4.0f * (x - x * fm_fabsf(x));
    return y + 0.225f * (y * fm_fabsf(y) - y);
}

/* PolyBLEP: the step's correction at phase ph (2^32 a turn), d a sample's increment */
static inline float blep(uint32_t ph, uint32_t d)
{
    float t;
    if (ph < d) {
        t = (float)ph / (float)d;
        return t + t - t * t - 1.0f;
    }
    if (ph > 0xFFFFFFFFu - d) {
        t = -(float)(0xFFFFFFFFu - ph + 1u) / (float)d;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

static inline float saw(uint32_t ph, uint32_t d) { return (float)ph * (2.0f / 4294967296.0f) - 1.0f - blep(ph, d); }

/* a chunk of the model's wave */
static void wave(int model, float *s, uint32_t d)
{
    uint32_t i, ph = b.ph;
    switch (model) {
    case 1:                              /* TRIANGLE */
        for (i = 0; i < CH; i++, ph += d)
            s[i] = 2.0f * fm_fabsf((float)(int32_t)ph * (1.0f / 2147483648.0f)) - 1.0f;
        break;
    case 2:                              /* SQUARE */
        for (i = 0; i < CH; i++, ph += d)
            s[i] = (ph < 0x80000000u ? 1.0f : -1.0f) + blep(ph, d) - blep(ph + 0x80000000u, d);
        break;
    case 3:                              /* SAW */
        for (i = 0; i < CH; i++, ph += d)
            s[i] = saw(ph, d);
        break;
    case 4: {                            /* REESE: two saws ten cents apart, beating slowly */
        uint32_t ph2 = b.ph2, d2 = (uint32_t)(int32_t)((float)(int32_t)d * 1.0058f);
        for (i = 0; i < CH; i++, ph += d, ph2 += d2)
            s[i] = 0.5f * (saw(ph, d) + saw(ph2, d2));
        b.ph2 = ph2;
        break;
    }
    case 5: {                            /* SUB: the sine and its octave below */
        uint32_t phs = b.phs, ds = d >> 1;
        for (i = 0; i < CH; i++, ph += d, phs += ds)
            s[i] = 0.6f * psin(ph) + 0.65f * psin(phs);
        b.phs = phs;
        break;
    }
    case 6:                              /* ORGAN: 8', 4' and 2 2/3' */
        for (i = 0; i < CH; i++, ph += d)
            s[i] = 0.62f * psin(ph) + 0.34f * psin(ph * 2u) + 0.22f * psin(ph * 3u);
        break;
    case 7:                              /* FM: the sine modulating itself, index 1.6 */
        for (i = 0; i < CH; i++, ph += d)
            s[i] = psin(ph + (uint32_t)(int32_t)(psin(ph) * (1.6f * 683565275.6f)));
        break;
    default:                             /* SINE */
        for (i = 0; i < CH; i++, ph += d)
            s[i] = psin(ph);
        break;
    }
    b.ph = ph;
}

void bass_render(float *l, float *r, uint32_t n, const bass_params_t *p)
{
    float kg, ka, kd, kr, sus, drv, norm, mix, k, trim;
    uint32_t c, i;
    if (b.note < 0)
        return;
    /* a chunk's steps: the glide (a third of GLIDE a time constant), the envelope's */
    {
        float tg = 2.0f * p->glide * p->glide;
        kg = tg < 0.002f ? 1.0f : 1.0f - fm_expf(-(float)CH * BR_INV_FS * 3.0f / tg);
    }
    ka = 1.0f - fm_expf(-(float)CH * BR_INV_FS * 3.0f / (0.005f * fm_powf(1000.0f, p->attack)));
    kr = 1.0f - fm_expf(-(float)CH * BR_INV_FS * 3.0f / (0.03f * fm_powf(500.0f, p->release)));
    kd = 1.0f - fm_expf(-(float)CH * BR_INV_FS * 3.0f / (0.01f * fm_powf(1000.0f, p->decay)));
    sus = p->sustain >= 1.0f ? 1.0f : fm_sqrtf(p->sustain);   /* (the gain is the envelope's square: SUSTAIN a level) */
    if (!b.set) {
        b.flt = p->filter;
        b.res = p->resonance;
        b.drv = p->drive;
        b.set = 1;
    }
    for (c = 0; c < n; c += CH) {
        float s[CH], g0 = b.lvl, g1, dg, inc, f, fc, g, a1, a2, a3, z1 = b.s1, z2 = b.s2;
        int model = p->model >= 0 && p->model < BASS_NMODEL ? p->model : 0, swap = 0;
        uint32_t d;
        if (model != b.model && b.lvl == 0.0f && b.env < 0.001f)
            b.model = model;             /* silent: at once */
        if (model != b.model) {          /* the dip: out, then the new one */
            b.mg -= 0.25f;
            if (b.mg <= 0.0f) {
                b.mg = 0.0f;
                swap = 1;
            }
        } else if (b.mg < 1.0f) {
            b.mg = b.mg + 0.25f > 1.0f ? 1.0f : b.mg + 0.25f;
        }
        if (b.octv != p->octave) {       /* OCTAVE turned: the note sounding moves there, gliding as GLIDE does */
            b.octv = (int8_t)p->octave;
            b.oct_t = ((float)sounding(b.note, b.octv) - 69.0f) * (1.0f / 12.0f);
        }
        b.oct += (b.oct_t - b.oct) * kg;
        if (b.again && !--b.again) {     /* PULSE's gap gone by: the note again, from its ATTACK */
            b.gate = 1;
            b.dec = 0;
            b.ts = 0.0f;
            b.pulses++;
        }
        b.ts += (float)CH * BR_INV_FS;
        if (!b.gate) {
            b.env += (0.0f - b.env) * kr;
        } else if (!b.dec) {
            b.env += (1.0f - b.env) * ka;
            b.dec = b.env >= 0.95f;
            if (b.dec)
                b.ts = 0.0f;
        } else {
            b.env += (sus - b.env) * (sus < 1.0f ? kd : ka);
        }
        b.prs += (b.prs_t - b.prs) * 0.15f;
        b.vel += (b.vel_t - b.vel) * 0.15f;
        b.flt += (p->filter - b.flt) * 0.12f;
        b.res += (p->resonance - b.res) * 0.12f;
        b.drv += (p->drive - b.drv) * 0.12f;
        drv = 1.0f + 2.0f * b.drv;       /* DRIVE: into the soft clip, crossfaded from the clean wave */
        norm = 1.0f / br_soft(drv > 1.5f ? 1.5f : drv);
        mix = b.drv;
        k = 2.0f - 1.83f * br_clampf(b.res, 0.0f, 1.0f);   /* 1 / Q: 0.5 .. 6 */
        trim = 1.0f / (1.0f + 0.6f * br_clampf(b.res, 0.0f, 1.0f));   /* its peak, kept under the limit */
        f = 440.0f * fm_exp2f(b.oct);
        inc = br_clampf(f * BR_INV_FS, 0.0f, 0.45f);
        d = (uint32_t)(int32_t)(inc * 4294967296.0f);
        /* the low-pass: 40 Hz .. 5 kHz, opened up to two octaves by pressure */
        fc = br_clampf(40.0f * fm_powf(125.0f, b.flt) * fm_exp2f(2.0f * b.prs), 20.0f, 12000.0f);
        g = fm_tanf(3.14159265f * fc * BR_INV_FS);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        g1 = BASS_G * MODEL_G[b.model] * p->level * p->level * (0.6f + 0.4f * b.vel) * (1.0f + 0.6f * b.prs) *
             b.env * b.env * b.mg * b.mg * (3.0f - 2.0f * b.mg);
        dg = (g1 - g0) * (1.0f / (float)CH);
        wave(b.model, s, d);
        for (i = 0; i < CH; i++) {
            float x = s[i] + mix * (br_soft(s[i] * drv) * norm - s[i]), v1, v2, v3, y;
            v3 = x - z2;                 /* the state variable filter, its low-pass out */
            v1 = a1 * z1 + a2 * v3;
            v2 = z2 + a2 * z1 + a3 * v3;
            z1 = 2.0f * v1 - z1;
            z2 = 2.0f * v2 - z2;
            g0 += dg;
            y = v2 * g0 * trim * 0.70710678f;   /* the centre */
            l[c + i] += y;
            r[c + i] += y;
        }
        b.s1 = fm_flush(z1);
        b.s2 = fm_flush(z2);
        b.lvl = g1;
        if (swap)                        /* faded out: the new model from the next chunk */
            b.model = model;
    }
    if (!b.gate && !b.again && b.env < 0.001f)
        b.note = -1;
}

void bass_view(bass_view_t *v)
{
    v->note = b.note < 0 ? -1 : (int8_t)sounding(b.note, b.octv);
    v->gate = b.gate;
    v->stage = !b.gate ? 2u : b.dec ? 1u : 0u;
    v->t = b.ts;
    v->let_stage = b.let_dec;
    v->let_t = b.let_ts;
    v->level = b.env;
    v->pressure = b.prs;
    v->pulses = b.pulses;
    v->ons = b.ons;
}
