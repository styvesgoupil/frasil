/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* LEAD (lead.h, card #144). A chunk of 32 frames at a time, as BASS: each voice's pitch (the glide in octaves, VIBRATO,
 * the pitch wheel, a slow drift of its own, as an analog one wanders; WHISTLE's scoop), its envelope (BASS's ADSR, the
 * gain its square), FILTER ENV's own (the same ADSR's way, its own four: admin #619) and its filter's coefficients a
 * chunk; the oscillators, the breath, the drive and the filter a sample.
 * The phases are 32 bits (they wrap by themselves); the saws PolyBLEP's; a square and a pulse each the difference of two
 * saws (no DC whatever its width); WHISTLE's sine a parabola made round (its harmonics some 60 dB down: no table, no
 * RAM). MONO is the first voice alone, as the first LEAD was; POLY all LEAD_NV, each a little lower (LEAD_PG). The breath
 * a noise a chunk, the voices' one. What an LFO moves (its steps 10 ms apart) glides: CUTOFF and RESONANCE a voice's,
 * as before; DETUNE, WAVE, DRIVE, BREATH, FILTER ENV and the delay's MIX, SPACE and FEEDBACK a chunk's step at a time
 * (L.sm); TIME as the delay always had it; the level in each chunk's gain ramp.
 * The delay, GLINT's (glint.c glint_delay, admin #570): two lines, ping-pong (left: the dry and the right's return;
 * right: the left's), the feedback through a low-pass and a high-pass (~120 Hz: the repeats stay clear), the time
 * gliding to TIME's (the beat's, core.c delay_sync), each side's echo on its side. Not GLINT's size (two lines of 20480
 * at half the rate, 80 KB of the pool: no room for a second): a quarter of the rate (admin #627: at a twelfth, 12 bits,
 * two one-poles around it, its echoes carried 26 .. 35 dB more than GLINT's off the note's partials, aliases, images
 * and noise: metallic), in 16 KB of the pool the reverb's ring gave up (nuage.c NU_RING). The dry through a four-pole
 * elliptic low-pass (flat to 4 kHz, 38 dB under from 7 kHz: what would fold back under 4 kHz) and taken one sample in
 * four (11025 Hz) into two lines of LD_N, 12 bits a sample as a little float (a sign, an exponent, 8 bits and one
 * implied: some 54 dB under the sample whatever its level, an echo's tail as clean as its start; two in three bytes,
 * 8.3 KB a line), 0.5 s at most (LEAD_DELAY_MAX: longer, halved, on the beat still), read back a fraction of a sample at
 * a time through four of them (Hermite, each decoded once); the repeats' low-pass some 3.5 kHz, a little darker each
 * time round, as GLINT's; back to the full rate by a ramp and a two-pole elliptic low-pass a side (its ripple the
 * ramp's droop made up, its notch on the ramp's images). Silent (the notes let go, the echoes gone), nothing runs at
 * all; the lines cleared before they run again.
 * Its share of the cloud (admin #627: a mono saw straight in, its tail 30 dB brighter over 3 kHz than GLINT's, rang the
 * tank): band-limited as the others' come (a high-pass at 150 Hz, two low-passes at 4 kHz), the right side's 2.9 ms
 * later (the two sides apart: the tank fed in stereo, as the drones' and GLINT's voices feed it). */
#include "dsp.h"
#include "lead.h"

#if FRS_LEAD
#define CH 32u
#define LEAD_G 0.26f                     /* the lead over everything (some 7 dB over a drone chord at their defaults) */
#define LEAD_PG 0.7f                     /* POLY's voice: three notes some 2 dB over MONO's one */
#define LD_N 5520u                       /* each of the delay's two lines, at the low rate (even: two samples in 3 bytes) */
#define LD_B (LD_N * 3u / 2u)            /* a line's bytes */
#define LD_RATE (44100.0f / 4.0f)        /* the low rate (Hz): a quarter of 44.1 kHz */
#define LD_Q 16384.0f                    /* (a line's sample: +-2 its full scale, as GLINT's and the reverb's) */
#define LD_CUT 4000.0f                   /* the low-passes into the lines and out of them */
#define LD_DC 128u                       /* the cloud's right side, so many samples later (2.9 ms) */
_Static_assert(LD_N % 2u == 0u && LD_N >= 5513u + 6u, "the lines hold LEAD's longest repeat (LEAD_DELAY_MAX, 0.5 s at "
                                                       "11025 Hz: 5513) and Hermite's four");
#define LD_SCOOP 0.7f                    /* WHISTLE's attack: so many semitones under the note, gone in some 40 ms */
#define LD_BPK 0.18f                     /* WHISTLE's breath: its band-pass's 1/Q */
#define LD_SWAP 4u                       /* POLY's voice taken while it sounds: out over so many chunks (2.9 ms), then
                                          * its note from silence (admin #676 .. #681: its pitch moved at once at its
                                          * level, a click at nearly every note, RELEASE's tails keeping the four voices
                                          * sounding) */
#define LD_SWAP_ENV 0.1f                 /* (its envelope under it, -40 dB: taken at once, as before) */
/* POLY's voices at each of the shedding's steps (0 .. FR_SHED_TOP, the SMK-37's 8; past the table its last): all of
 * them while the drone alone gives (1 .. 4: frasil.c SHED_NP, SHED_NV), a voice fewer at 5 (the LEAD's step, as
 * before), at 7 and at 8 (6 the drone's alone): the shedding lighter at every step, and at 8 three voices, as with
 * four (admin #685: his scene, every voice sounding, no dearer there) */
static const uint8_t LD_CAP[] = {LEAD_NV, LEAD_NV, LEAD_NV, LEAD_NV, LEAD_NV, 5u, 5u, 4u, 3u};
_Static_assert(LEAD_NV >= 5u, "LD_CAP: a step's voices, LEAD_NV at most");

typedef struct {
    int8_t note;                         /* -1: silent; else as played (OCTAVE not on it) */
    uint8_t gate, dec, set;
    uint8_t gliding;                     /* GLIDE's way to the note (MONO struck: at once) */
    uint8_t cut;                         /* let go by the shedding: out in some 15 ms */
    uint8_t ever;                        /* it has sounded: POLY glides from the note it played last */
    uint8_t fdec;                        /* (dec's, FILTER ENV's own envelope's) */
    uint8_t swap;                        /* taken while it sounded: the chunks left of its way out, its note after */
    float oct, oct_t;                    /* the pitch now and where it glides, octaves above 440 Hz */
    float vel, vel_t, prs, prs_t;
    float env, lvl;                      /* the envelope 0..1, the gain at the last chunk's end */
    float fenv;                          /* FILTER ENV's own envelope, 0..1 (admin #619: struck, let go with env) */
    float sw_vel;                        /* (swap's note's velocity) */
    uint32_t ph1, ph2;
    uint32_t age;                        /* when it was struck (POLY takes the oldest held) */
    float s1, s2, s3, s4;                /* the two filter stages' states */
    float n1, n2;                        /* WHISTLE's breath, its band-pass's */
    float flt, res;                      /* CUTOFF, RESONANCE gliding (an LFO's or a knob's steps smoothed) */
    float drift, drift_t;                /* the slow wander, cents */
    float scoop;                         /* semitones from the note, to 0 (WHISTLE's attack) */
} ldv_t;
static ldv_t vs[LEAD_NV];

static struct {
    int8_t octv;                         /* OCTAVE as it is now on the notes */
    uint8_t cap;                         /* the voices POLY may take now (the shedding's) */
    float vph;                           /* VIBRATO's phase (turns) */
    float mod, bend, bend_t;             /* the mod wheel, the pitch wheel (semitones) gliding to bend_t */
    float send;                          /* the cloud's share at its loudest, the last render (lead_view) */
    float echo[2];                       /* each side's echo at its loudest, the last render (lead_view: the tests') */
    float sm[8];                         /* DETUNE, WAVE, DRIVE, BREATH, FILTER ENV, MIX, SPACE, FEEDBACK gliding a chunk at
                                          * a time (an LFO's steps, 10 ms apart, smoothed: no zipper) */
    uint8_t sm_on;                       /* (from the first render: where they are, no glide from 0) */
    uint32_t rng, age;
    uint32_t ons, glides;
} L = {.cap = LEAD_NV, .rng = 0x6C8E9CF5u};

typedef struct {                         /* what the voices and the delay take from the settings gliding (shape) */
    float det, w, dgn, gm, nb, nw, na, dw, mix, sp, fb, fenv;
    uint32_t po;
} shape_t;

/* the settings gliding (L.sm) as the voices and the delay take them: DETUNE, WAVE (0 .. 0.5 a saw into a square: the
 * second saw half a turn on, growing; 0.5 .. 1 the pulse narrower), DRIVE (its gain, and the level made up after:
 * louder driven, not much), BREATH (WHISTLE's some always) */
static void shape(const lead_params_t *p, int lean, shape_t *s)
{
    float det = L.sm[0], wv = L.sm[1], dr = L.sm[2], b = br_clampf(L.sm[3], 0.0f, 1.0f), bw = b > 0.22f ? b : 0.22f;
    float pwo = wv < 0.5f ? 0.5f : 0.5f - 0.76f * (wv - 0.5f);
    s->det = fm_exp2f(0.25f * det * (1.0f / 12.0f));   /* 0 .. 25 cents */
    s->w = wv < 0.5f ? 2.0f * wv : 1.0f;
    s->po = (uint32_t)(pwo * 4294967296.0f);
    s->dgn = 0.9f * fm_exp2f(5.08f * (dr - LEAD_DRIVE_0));   /* (x0.37 .. x0.9 at its default .. x12.5) */
    s->gm = fm_exp2f(-1.2f * (dr - LEAD_DRIVE_0)) * (p->poly ? LEAD_PG : 1.0f) * (p->whistle ? 0.8f : 1.0f);
    s->nb = p->whistle ? 0.0f : 0.45f * b;   /* (WHISTLE's breath: some 20 dB under the sine at 0.3, 10 at 1) */
    s->nw = p->whistle ? 5.0f * LD_BPK * bw : 0.0f;   /* (into the band-pass, its peak 1/k: 5 bw at the note) */
    s->na = p->whistle ? 0.16f * bw : 0.0f;           /* (and some air, unfiltered) */
    s->dw = lean ? 0.0f : 0.35f * det;                /* WHISTLE's second sine, so much under the first */
    s->fenv = L.sm[4];
    s->mix = br_clampf(L.sm[5], 0.0f, 1.0f);
    s->sp = br_clampf(L.sm[6], 0.0f, 1.0f) * 0.7071f;
    s->fb = 0.88f * br_clampf(L.sm[7], 0.0f, 1.0f);   /* (GLINT's) */
}

static uint8_t ldl[LD_B] BR_POOL, ldr[LD_B] BR_POOL;   /* the delay's left line, its right (the pool's: the reverb's
                                                         * ring gave it up, nuage.c NU_RING) */
typedef struct {
    float b0, b1, b2, a1, a2;            /* a two-pole section (direct form, normalised) */
} bq_t;
static struct {
    uint32_t w;                          /* the next sample's place */
    uint32_t ds, dn;                     /* the full rate's samples to the next low one, a low sample's (4 at 44.1 kHz) */
    bq_t aa[2], up;                      /* the low-pass into the lines (two sections), each side's out */
    uint32_t hi;                         /* the lines' four last read: around this place (LD_N: none) */
    float hv[2][4];                      /* (each line's, decoded) */
    float az[2][2], uz[2][2];            /* their states (transposed direct form II) */
    float y0[2], y1[2], yi, ys[2];       /* each line's last two outputs, the ramp between them, its last low-passed */
    float lp[2], hp[2];                  /* the feedback's filters, a line each (GLINT's) */
    float dl;                            /* the delay now, in low samples (TIME glides to it) */
    float quiet;                         /* seconds the lines have had almost nothing in them */
    float sh[2], s1[2], s2[2];           /* the cloud's share, each side: its high-pass, its two low-passes */
    int16_t dc[LD_DC];                   /* the right side's share a little later (x 4096) */
    uint32_t dcw;
    uint8_t live, dirty;                 /* running; the lines hold something (cleared before they run again) */
} d = {.dn = 4u, .hi = LD_N};

/* an elliptic low-pass's section at LD_CUT (its analog prototype's, its edge at 1: a zero, a pole and its Q; through
 * the bilinear, prewarped), its gain 1 at DC */
static void bq_ell(bq_t *f, float wz, float wp, float q)
{
    float k = fm_tanf(3.14159265f * LD_CUT * BR_INV_FS), z = wz * wz * k * k, p = wp * wp * k * k, m = wp / q * k;
    float n = 1.0f / (1.0f + m + p), b0 = (1.0f + z) * n, b1 = (2.0f * z - 2.0f) * n, a1 = (2.0f * p - 2.0f) * n;
    float a2 = (1.0f - m + p) * n, g = (1.0f + a1 + a2) / (2.0f * b0 + b1);
    f->b0 = f->b2 = b0 * g;
    f->b1 = b1 * g;
    f->a1 = a1;
    f->a2 = a2;
}

void lead_init(void)
{
    uint32_t n = (uint32_t)(BR_FS / LD_RATE + 0.5f), i;
    d.dn = n ? n : 1u;
    d.ds = 0;
    bq_ell(&d.aa[0], 5.115081f, 0.6372412f, 0.7226430f);   /* (elliptic, four poles: 0.5 dB to 4 kHz, 38 dB under from
                                                          * 7 kHz, what folds back under 4 kHz; Butterworth's 22) */
    bq_ell(&d.aa[1], 2.218985f, 1.0308677f, 3.3830906f);
    bq_ell(&d.up, 2.646702f, 0.939085f, 1.257641f);   /* (two poles, 2 dB, its notch some 9 kHz) */
    for (i = 0; i < LEAD_NV; i++)
        vs[i].note = -1;
}

#if !BR_FS_CONST                         /* (frasil_reset's: not on the FM-1) */
static void ld_zero(void *p, uint32_t n)
{
    uint8_t *z = (uint8_t *)p;
    uint32_t i;
    for (i = 0; i < n; i++)
        z[i] = 0;
}
void lead_reset(void)                    /* (as the statics start: lead_init's after it) */
{
    ld_zero(vs, sizeof vs);
    ld_zero(&L, sizeof L);
    L.cap = LEAD_NV;
    L.rng = 0x6C8E9CF5u;
    ld_zero(&d, sizeof d);
    d.dn = 4u;
    d.hi = LD_N;
    ld_zero(ldl, sizeof ldl);
    ld_zero(ldr, sizeof ldr);
}
#endif

void lead_hush(void)
{
    uint32_t i;
    for (i = 0; i < LEAD_NV; i++) {
        vs[i].note = -1;
        vs[i].gate = vs[i].cut = vs[i].swap = 0;
        vs[i].env = vs[i].lvl = vs[i].fenv = 0.0f;
    }
    d.live = 0;
    d.dirty = 1;                         /* (its echoes gone: cleared before it runs again) */
}

static float note_oct(int note)          /* a note as played, OCTAVE on it: octaves above 440 Hz */
{
    int n = note + 12 * L.octv;
    n = n < 0 ? 0 : n > 127 ? 127 : n;
    return ((float)n - 69.0f) * (1.0f / 12.0f);
}

static void fresh(ldv_t *v, float vel)   /* from silence: the phases apart (the two not as one), the rest from nothing */
{
    v->ph1 = br_rand(&L.rng);
    v->ph2 = br_rand(&L.rng);
    v->env = v->lvl = v->fenv = 0.0f;
    v->s1 = v->s2 = v->s3 = v->s4 = v->n1 = v->n2 = 0.0f;
    v->vel = vel;
    v->prs = v->prs_t = 0.0f;
    v->set = 0;
}

static void strike(ldv_t *v, int note, float vel)
{
    v->note = (int8_t)note;
    v->vel_t = vel;
    v->gate = 1;
    v->dec = v->fdec = v->cut = 0;
    v->ever = 1;
    v->age = ++L.age;
    L.ons++;
}

void lead_on(int note, float vel, int legato)
{
    ldv_t *v = &vs[0];
    float o = note_oct(note);
    if (v->note < 0) {                   /* from silence: no glide from nowhere */
        v->oct = o;
        fresh(v, vel);
        legato = 0;
    }
    v->oct_t = o;
    v->gliding = (uint8_t)(legato != 0);
    if (!legato)
        v->oct = o;                      /* (struck: there at once, its envelope from where it is) */
    v->note = (int8_t)note;
    v->vel_t = vel;
    if (legato && v->gate) {             /* legato: the envelope goes on, the note glides */
        L.glides++;
        return;
    }
    v->scoop = legato ? 0.0f : -LD_SCOOP;
    strike(v, note, vel);
}

void lead_off(void)
{
    uint32_t i;
    for (i = 0; i < LEAD_NV; i++) {
        vs[i].gate = 0;
        vs[i].prs_t = 0.0f;
    }
}

void lead_poly_on(int note, float vel)
{
    ldv_t *x = 0;
    uint32_t i, n = L.cap;
    float o = note_oct(note);
    for (i = 0; i < LEAD_NV && !x; i++)  /* the same note again: its voice (one past the shedding's cap too: not a
                                          * second on it) */
        if (vs[i].note == note && !vs[i].cut)
            x = &vs[i];
    for (i = 0; i < n && !x; i++)        /* a free one */
        if (vs[i].note < 0)
            x = &vs[i];
    if (!x)                              /* the quietest let go (admin #685: the oldest let go was taken, its tail
                                          * loud still, a quieter one sounding on), else the oldest held */
        for (i = 0; i < n; i++)
            if (!vs[i].gate && (!x || vs[i].env < x->env))
                x = &vs[i];
    if (!x)
        for (i = 0; i < n; i++)
            if (!x || vs[i].age < x->age)
                x = &vs[i];
    if (!x->ever)                        /* (never sounded: no glide from nowhere) */
        x->oct = o;
    if (x->note >= 0 && x->note != note && (x->swap || x->env > LD_SWAP_ENV)) {
        x->note = (int8_t)note;          /* sounding: out first (LD_SWAP), its note struck after (the render's); one
                                          * the shedding let go too (on its way out in 15 ms, loud still) */
        x->sw_vel = vel;
        if (!x->swap)
            x->swap = (uint8_t)LD_SWAP;
        x->gate = 1;
        x->cut = 0;
        x->age = ++L.age;
        L.ons++;
        return;
    }
    if (x->note < 0)
        fresh(x, vel);
    x->oct_t = o;                        /* from the note it played last, GLIDE's way (0: there at once) */
    x->gliding = 1;
    x->scoop = -LD_SCOOP;
    strike(x, note, vel);
}

void lead_poly_off(int note)
{
    uint32_t i;
    for (i = 0; i < LEAD_NV; i++)
        if (vs[i].note == note && vs[i].gate) {
            vs[i].gate = 0;
            vs[i].prs_t = 0.0f;
        }
}

void lead_press(int note, float p)
{
    uint32_t i;
    p = br_clampf(p, 0.0f, 1.0f);
    if (note < 0)
        vs[0].prs_t = p;
    else
        for (i = 0; i < LEAD_NV; i++)
            if (vs[i].note == note && vs[i].gate)
                vs[i].prs_t = p;
}
void lead_mod(float m) { L.mod = br_clampf(m, 0.0f, 1.0f); }
void lead_bend(float semis) { L.bend_t = br_clampf(semis, -12.0f, 12.0f); }

/* an envelope a chunk on: held, ATTACK up (to 0.95, then DECAY), DECAY to SUSTAIN (all of it: ATTACK's way); let go,
 * RELEASE; let go by the shedding, out in some 15 ms (kq). The level's, and FILTER ENV's own (admin #619) */
static inline void env_step(float *e, uint8_t *dec, int gate, int cut, float ka, float kd, float kr, float kq, float sus)
{
    if (!gate) {
        *e += (0.0f - *e) * (cut ? kq : kr);
    } else if (!*dec) {
        *e += (1.0f - *e) * ka;
        *dec = *e >= 0.95f;
    } else {
        *e += (sus - *e) * (sus < 1.0f ? kd : ka);
    }
}

/* PolyBLEP: the step's correction at phase ph (2^32 a turn), dd a sample's increment: none but within a sample of the
 * step (one test, seldom true: ph + dd, round the turn, under 2 dd; two, each its own way, a division each, and the
 * compiler worked out both at every sample, keeping one) */
static inline float blep(uint32_t ph, uint32_t dd)
{
    if (__builtin_expect(ph + dd < dd + dd, 0)) {
        float t;
        if (ph < dd) {
            t = (float)ph / (float)dd;
            return t + t - t * t - 1.0f;
        }
        t = -(float)(0xFFFFFFFFu - ph + 1u) / (float)dd;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}
static inline float saw(uint32_t ph, uint32_t dd) { return (float)ph * (2.0f / 4294967296.0f) - 1.0f - blep(ph, dd); }

static inline float sine(uint32_t ph)    /* WHISTLE's: a parabola a half turn, made round */
{
    float t = (float)(int32_t)ph * (1.0f / 2147483648.0f), y = 4.0f * t * (1.0f - fm_fabsf(t));
    return y + 0.225f * (y * fm_fabsf(y) - y);
}

/* a line's sample, 12 bits: a sign, a 3-bit exponent, 8 bits of mantissa (from the second octave up its top bit
 * implied): x LD_Q, below 256 as it is, above it to the nearest step of its octave's 256 (some 54 dB under it) */
static inline float ld_at(const uint8_t *b, uint32_t i)   /* sample i of a line (two in three bytes) */
{
    const uint8_t *q = b + (i >> 1) * 3u;
    uint32_t v = i & 1u ? (uint32_t)(q[1] >> 4) | (uint32_t)q[2] << 4 : (uint32_t)q[0] | (uint32_t)(q[1] & 15u) << 8;
    uint32_t e = v >> 8 & 7u, m = v & 255u, a = e ? (m + 256u) << (e - 1u) : m;
    return (v & 0x800u ? -(float)a : (float)a) * (1.0f / LD_Q);
}
static inline float ld_herm(const float *y, float f)   /* Hermite through four, f past the second */
{
    return ((((0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2])) * f + (y[0] - 2.5f * y[1] + 2.0f * y[2] - 0.5f * y[3])) * f +
             0.5f * (y[2] - y[0])) * f) + y[1];
}
/* both lines, so many low samples behind the next write (3 .. LD_N - 4), Hermite through the four around it: each
 * sample decoded once (the read moves on a sample a step: three of the four kept; the write never among them) */
static inline void ld_read(float at, float *yl, float *yr)
{
    float p = (float)d.w - at, f;
    uint32_t i, k;
    p += p < 0.0f ? (float)LD_N : 0.0f;
    i = (uint32_t)p;
    f = p - (float)i;
    i = i >= LD_N ? i - LD_N : i;
    if (i != d.hi) {
        uint32_t n = d.hi + 1u >= LD_N ? 0u : d.hi + 1u, s = d.hi < LD_N && i == n ? 3u : 0u;
        for (k = 0; k < s; k++) {        /* (on by one: the three kept, the fourth anew) */
            d.hv[0][k] = d.hv[0][k + 1u];
            d.hv[1][k] = d.hv[1][k + 1u];
        }
        for (k = s; k < 4u; k++) {
            uint32_t j = i + k + LD_N - 1u;
            j = j >= LD_N ? j - LD_N : j;
            j = j >= LD_N ? j - LD_N : j;
            d.hv[0][k] = ld_at(ldl, j);
            d.hv[1][k] = ld_at(ldr, j);
        }
        d.hi = i;
    }
    *yl = ld_herm(d.hv[0], f);
    *yr = ld_herm(d.hv[1], f);
}
static inline void ld_write(uint8_t *line, float x)
{
    uint8_t *q = line + (d.w >> 1) * 3u;
    union { float f; uint32_t u; } v;
    uint32_t e = 0, m, u;
    v.f = br_clampf(fm_fabsf(x) * LD_Q, 0.0f, 32767.0f);
    if (v.f < 256.0f) {                  /* (under 256: as it is) */
        m = (uint32_t)v.f;
    } else {                             /* (over: the float's own octave and its 8 top bits, to the nearest: a carry
                                          * into the next octave there) */
        v.u += 1u << 14;
        e = (v.u >> 23) - 134u;
        m = v.u >> 15 & 255u;
        if (e > 7u) {
            e = 7u;
            m = 255u;
        }
    }
    u = (x < 0.0f ? 0x800u : 0u) | e << 8 | m;
    if (d.w & 1u) {
        q[1] = (uint8_t)((q[1] & 15u) | (u & 15u) << 4);
        q[2] = (uint8_t)(u >> 4);
    } else {
        q[0] = (uint8_t)u;
        q[1] = (uint8_t)((q[1] & 0xF0u) | u >> 8);
    }
}

int lead_render(float *o, float *o2, float *rl, float *rr, uint32_t n, const lead_params_t *p, int shed)
{
    const float ch_t = (float)CH * BR_INV_FS * br_tk;
    float kg, ka, kd, kr, kq, kf, kp, kb, ksc, ks, sus, dlt, fb, mix, sp, klp, khp, kdl, det, w, dgn, gm, nb, nw;
    float khs, kls;
    float na, dw, fe, fka, fkd, fkr, fsus;
    float tg[8];
    shape_t sh;
    float send = 0.0f;
    uint32_t c, i, j, po, on = 0;
    uint32_t cap = LD_CAP[shed < 0 ? 0 : shed < (int)sizeof LD_CAP ? shed : (int)sizeof LD_CAP - 1];   /* (the LEAD
                                         * shed last, the drone first: POLY all its voices to step 4, then 5 5 4 3;
                                         * its two oscillators always: admin #636, #639, #653, #685) */
    int lean = 0, any = 0;
    uint8_t live[LEAD_NV];
    if (L.octv != p->octave) {           /* OCTAVE turned: the notes sounding move there, gliding */
        L.octv = (int8_t)p->octave;
        for (j = 0; j < LEAD_NV; j++)
            if (vs[j].note >= 0) {
                vs[j].oct_t = note_oct(vs[j].note);
                vs[j].gliding = 1;
            }
    }
    L.cap = (uint8_t)cap;
    for (j = 0; j < LEAD_NV; j++) {
        live[j] = vs[j].note >= 0;
        any |= live[j];
        on += live[j] && !vs[j].cut;
    }
    while (on > cap) {                   /* the shedding: the quietest let go, else the oldest held, out quickly */
        ldv_t *x = 0;
        for (j = 0; j < LEAD_NV; j++)
            if (live[j] && !vs[j].cut && !vs[j].gate && (!x || vs[j].env < x->env))
                x = &vs[j];
        if (!x)
            for (j = 0; j < LEAD_NV; j++)
                if (live[j] && !vs[j].cut && (!x || vs[j].age < x->age))
                    x = &vs[j];
        x->gate = 0;
        x->cut = 1;
        on--;
    }
    L.send = L.echo[0] = L.echo[1] = 0.0f;
    if (!any && !d.live)
        return 0;
    if (d.dirty && any) {                /* (from silence: no old echoes) */
        for (i = 0; i < LD_B; i++)
            ldl[i] = ldr[i] = 0;
        for (i = 0; i < LD_DC; i++)
            d.dc[i] = 0;
        d.yi = 0.0f;
        d.hi = LD_N;
        for (j = 0; j < 2u; j++) {
            d.y0[j] = d.y1[j] = d.ys[j] = d.lp[j] = d.hp[j] = d.sh[j] = d.s1[j] = d.s2[j] = 0.0f;
            d.az[j][0] = d.az[j][1] = d.uz[j][0] = d.uz[j][1] = 0.0f;
        }
        d.dirty = 0;
        d.dl = 0.0f;
    }
    if (any)
        d.live = 1;
    {
        float tg = 2.0f * p->glide * p->glide;
        kg = tg < 0.002f ? 1.0f : 1.0f - fm_expf(-ch_t * 3.0f / tg);
    }
    ka = 1.0f - fm_expf(-ch_t * 3.0f / (0.005f * fm_powf(1000.0f, p->attack)));
    kr = 1.0f - fm_expf(-ch_t * 3.0f / (0.03f * fm_powf(500.0f, p->release)));
    kd = 1.0f - fm_expf(-ch_t * 3.0f / (0.01f * fm_powf(1000.0f, p->decay)));
    kq = 1.0f - fm_expf(-ch_t * 3.0f / 0.015f);
    ksc = fm_expf(-ch_t * (1.0f / 0.04f));
    sus = p->sustain >= 1.0f ? 1.0f : fm_sqrtf(p->sustain);
    fka = 1.0f - fm_expf(-ch_t * 3.0f / (0.005f * fm_powf(1000.0f, p->fattack)));   /* FILTER ENV's own, the same way */
    fkr = 1.0f - fm_expf(-ch_t * 3.0f / (0.03f * fm_powf(500.0f, p->frelease)));
    fkd = 1.0f - fm_expf(-ch_t * 3.0f / (0.01f * fm_powf(1000.0f, p->fdecay)));
    fsus = p->fsustain >= 1.0f ? 1.0f : fm_sqrtf(p->fsustain);
    kf = br_k(0.12f);
    kp = br_k(0.15f);
    kb = br_k(0.3f);
    /* the delay's TIME in low samples (the beat's, core.c; it glides there, some 30 ms as GLINT's); the rest as they
     * glide (shape) */
    dlt = br_clampf(p->dtime_s * LD_RATE, 3.0f, (float)LD_N - 4.0f);
    klp = 1.0f - fm_expf(-6.2831853f * 3500.0f / LD_RATE);    /* the repeats' low-pass, high-pass (low rate) */
    khp = 1.0f - fm_expf(-6.2831853f * 120.0f / LD_RATE);
    kdl = 1.0f - fm_expf(-1.0f / (0.03f * LD_RATE));
    khs = 1.0f - fm_expf(-6.2831853f * 150.0f * BR_INV_FS);   /* the cloud's share: its high-pass, its low-passes */
    kls = 1.0f - fm_expf(-6.2831853f * 4000.0f * BR_INV_FS);
    ks = br_k(0.3f);                                          /* (some 2 ms) */
    if (!d.dl)
        d.dl = dlt;
    tg[0] = p->detune;
    tg[1] = p->wave;
    tg[2] = p->drive;
    tg[3] = p->breath;
    tg[4] = p->fenv;
    tg[5] = p->dmix;
    tg[6] = p->space;
    tg[7] = p->dfb;
    if (!L.sm_on) {
        for (j = 0; j < 8u; j++)
            L.sm[j] = tg[j];
        L.sm_on = 1;
    }
    shape(p, lean, &sh);
    for (j = 0; j < LEAD_NV; j++)
        if (live[j] && !vs[j].set) {
            vs[j].flt = p->cutoff;
            vs[j].res = p->resonance;
            vs[j].set = 1;
        }
    for (c = 0; c < n; c += CH) {
        float nz[CH], vb = 0.0f, peak = 0.0f;
        int moved = 0;
        for (j = 0; j < 8u; j++)         /* (the settings gliding a chunk's step: their shape anew if one moved) */
            if (L.sm[j] != tg[j]) {
                float e = tg[j] - L.sm[j];
                L.sm[j] = e < 1e-5f && e > -1e-5f ? tg[j] : L.sm[j] + e * ks;
                moved = 1;
            }
        if (moved)
            shape(p, lean, &sh);
        det = sh.det;
        w = sh.w;
        po = sh.po;
        dgn = sh.dgn;
        gm = sh.gm;
        nb = sh.nb;
        nw = sh.nw;
        na = sh.na;
        dw = sh.dw;
        fe = sh.fenv;
        mix = sh.mix;
        sp = sh.sp;
        fb = sh.fb;
        for (i = 0; i < CH; i++)
            o[c + i] = 0.0f;
        if (any) {
            float vib;
            L.bend += (L.bend_t - L.bend) * kb;
            L.vph += 5.5f * ch_t;
            L.vph -= L.vph >= 1.0f ? 1.0f : 0.0f;
            vib = 0.5f * (p->vibrato > L.mod ? p->vibrato : L.mod);
            vib = vib * vib * 2.0f;      /* (gentle at first) */
            vb = vib * fm_sin_turns(L.vph);
            if (nb > 0.0f)               /* (the breath: one noise, the voices', BREATH's share in it already) */
                for (i = 0; i < CH; i++)
                    nz[i] = nb * br_randf(&L.rng);
            else if (nw > 0.0f)          /* (WHISTLE's: its two shares of it, below) */
                for (i = 0; i < CH; i++)
                    nz[i] = br_randf(&L.rng);
            else
                for (i = 0; i < CH; i++)
                    nz[i] = 0.0f;
        }
        for (j = 0; j < LEAD_NV; j++) {
            ldv_t *v = &vs[j];
            float g0 = v->lvl, g1, dg, oct, f, inc, fc, gf, k1, k2, a1, a2, a3, b1, b2, b3, trim;
            float z1 = v->s1, z2 = v->s2, z3 = v->s3, z4 = v->s4, m1 = v->n1, m2 = v->n2, *y0 = o + c;
            uint32_t d1, d2, ph1 = v->ph1, ph2 = v->ph2;
            if (!live[j])
                continue;
            v->oct += (v->oct_t - v->oct) * (v->gliding ? kg : 1.0f);
            env_step(&v->env, &v->dec, v->gate, v->cut, ka, kd, kr, kq, sus);
            env_step(&v->fenv, &v->fdec, v->gate, v->cut, fka, fkd, fkr, kq, fsus);   /* (its own, struck and let go
                                                                                      * with the level's) */
            v->prs += (v->prs_t - v->prs) * kp;
            v->vel += (v->vel_t - v->vel) * kp;
            v->flt += (p->cutoff - v->flt) * kf;
            v->res += (p->resonance - v->res) * kf;
            if (br_rand(&L.rng) < 0x08000000u)   /* the wander: a new aim now and then, some 3 cents at most */
                v->drift_t = 3.0f * br_randf(&L.rng);
            v->drift += (v->drift_t - v->drift) * 0.004f;
            v->scoop *= ksc;
            oct = v->oct + (L.bend + vb + v->drift * 0.01f + (p->whistle ? v->scoop : 0.0f)) * (1.0f / 12.0f);
            f = 440.0f * fm_exp2f(oct) * br_pk;
            inc = br_clampf(f * BR_INV_FS, 0.0f, 0.45f);
            d1 = (uint32_t)(int32_t)(inc * 4294967296.0f);
            d2 = (uint32_t)(int32_t)(br_clampf(inc * det, 0.0f, 0.45f) * 4294967296.0f);
            /* the filter: CUTOFF, the envelope, the key (half), the velocity and the pressure */
            fc = 60.0f * fm_powf(200.0f, v->flt) *
                 fm_exp2f(5.0f * fe * v->fenv + 0.5f * oct + 0.8f * v->vel + 1.5f * v->prs) * br_pk;
            fc = br_clampf(fc, 30.0f, 15000.0f);
            gf = fm_tanf(3.14159265f * fc * BR_INV_FS);
            k1 = 1.41f;                  /* the first stage plain, RESONANCE on the second */
            k2 = 1.9f - 1.75f * br_clampf(v->res, 0.0f, 1.0f);
            a1 = 1.0f / (1.0f + gf * (gf + k1));
            a2 = gf * a1;
            a3 = gf * a2;
            b1 = 1.0f / (1.0f + gf * (gf + k2));
            b2 = gf * b1;
            b3 = gf * b2;
            trim = 1.0f / (1.0f + 0.9f * br_clampf(v->res, 0.0f, 1.0f));
            g1 = LEAD_G * p->level * (0.7f + 0.3f * v->vel) * (1.0f + 0.4f * v->prs) * v->env * v->env * trim * gm;
            if (v->swap)                 /* (taken: on its way out, 0 at its last chunk's end) */
                g1 *= (float)(v->swap - 1u) * (1.0f / (float)LD_SWAP);
            dg = (g1 - g0) * (1.0f / (float)CH);
            if (!p->whistle && w == 0.0f) {   /* WAVE's saw: the two saws alone (the second pair, times 0 below, not
                                               * made: the same samples; admin #653, some 22 instructions a voice a
                                               * sample) */
                for (i = 0; i < CH; i++) {
                    float x, y, s;
                    x = saw(ph1, d1);
                    if (!lean)
                        x = 0.5f * (x + saw(ph2, d2));
                    ph1 += d1;
                    ph2 += d2;
                    x = br_soft((x + nz[i]) * dgn);
                    s = x - z2;
                    {
                        float q1 = a1 * z1 + a2 * s, q2 = z2 + a2 * z1 + a3 * s;
                        z1 = 2.0f * q1 - z1;
                        z2 = 2.0f * q2 - z2;
                        y = q2;
                    }
                    s = y - z4;
                    {
                        float q1 = b1 * z3 + b2 * s, q2 = z4 + b2 * z3 + b3 * s;
                        z3 = 2.0f * q1 - z3;
                        z4 = 2.0f * q2 - z4;
                        y = q2;
                    }
                    g0 += dg;
                    y0[i] += y * g0;
                }
            } else if (!p->whistle) {
                for (i = 0; i < CH; i++) {
                    float x, y, s;
                    x = saw(ph1, d1) - w * saw(ph1 + po, d1);
                    if (!lean)
                        x = 0.5f * (x + saw(ph2, d2) - w * saw(ph2 + po, d2));
                    ph1 += d1;
                    ph2 += d2;
                    x = br_soft((x + nz[i]) * dgn);   /* (the drive before the filter) */
                    s = x - z2;          /* the first stage, its low-pass out */
                    {
                        float q1 = a1 * z1 + a2 * s, q2 = z2 + a2 * z1 + a3 * s;
                        z1 = 2.0f * q1 - z1;
                        z2 = 2.0f * q2 - z2;
                        y = q2;
                    }
                    s = y - z4;          /* the second, resonant */
                    {
                        float q1 = b1 * z3 + b2 * s, q2 = z4 + b2 * z3 + b3 * s;
                        z3 = 2.0f * q1 - z3;
                        z4 = 2.0f * q2 - z4;
                        y = q2;
                    }
                    g0 += dg;
                    y0[i] += y * g0;
                }
            } else {                     /* WHISTLE: the sine (DETUNE's second under it), the breath through a band-pass
                                          * at the note, a little air, then as the others */
                float gb = fm_tanf(3.14159265f * inc), c1 = 1.0f / (1.0f + gb * (gb + LD_BPK)), c2 = gb * c1, c3 = gb * c2;
                for (i = 0; i < CH; i++) {
                    float x, y, s;
                    x = sine(ph1);
                    if (dw > 0.0f)
                        x += dw * sine(ph2);
                    ph1 += d1;
                    ph2 += d2;
                    s = nw * nz[i] - m2;
                    {
                        float q1 = c1 * m1 + c2 * s, q2 = m2 + c2 * m1 + c3 * s;
                        m1 = 2.0f * q1 - m1;
                        m2 = 2.0f * q2 - m2;
                        x += q1 + na * nz[i];
                    }
                    x = br_soft(x * dgn);
                    s = x - z2;
                    {
                        float q1 = a1 * z1 + a2 * s, q2 = z2 + a2 * z1 + a3 * s;
                        z1 = 2.0f * q1 - z1;
                        z2 = 2.0f * q2 - z2;
                        y = q2;
                    }
                    s = y - z4;
                    {
                        float q1 = b1 * z3 + b2 * s, q2 = z4 + b2 * z3 + b3 * s;
                        z3 = 2.0f * q1 - z3;
                        z4 = 2.0f * q2 - z4;
                        y = q2;
                    }
                    g0 += dg;
                    y0[i] += y * g0;
                }
            }
            v->ph1 = ph1;
            v->ph2 = ph2;
            v->s1 = fm_flush(z1);
            v->s2 = fm_flush(z2);
            v->s3 = fm_flush(z3);
            v->s4 = fm_flush(z4);
            v->n1 = fm_flush(m1);
            v->n2 = fm_flush(m2);
            v->lvl = g1;
            if (v->swap && !--v->swap) {
                if (v->cut) {            /* (let go by the shedding on its way out: silent there, its old note not
                                          * back for the cut's 15 ms) */
                    v->env = v->fenv = 0.0f;
                } else {                 /* out: its note from silence, held or let go since, gliding from where it
                                          * was; FILTER ENV on from where it was, as a voice taken always had it */
                    uint8_t held = v->gate;
                    float fe = v->fenv;
                    fresh(v, v->sw_vel);
                    v->fenv = fe;
                    v->oct_t = note_oct(v->note);
                    v->gliding = 1;
                    v->scoop = -LD_SCOOP;
                    v->vel_t = v->vel;
                    v->gate = held;
                    v->dec = v->fdec = 0;
                    v->ever = 1;
                }
            }
        }
        /* the delay: into the lines a sample in dn, the echoes back up to the rate, each over the dry on its side (x
         * 1.41: a side's echo as loud as the first LEAD's in the middle); the cloud's share, the dry and the echoes as
         * heard (MIX 0: the dry alone, SPACE as much). A stage at a time over the chunk, its state in hand (the
         * struct's, the outputs perhaps it, read and written again at every sample) */
        {
            float mg = mix * 1.4142136f, u[2][CH];
            {                            /* the dry into the lines' band */
                const bq_t f0 = d.aa[0], f1 = d.aa[1];
                float p0 = d.az[0][0], p1 = d.az[0][1], q0 = d.az[1][0], q1 = d.az[1][1];
                for (i = 0; i < CH; i++) {
                    float x = o[c + i], y = f0.b0 * x + p0;
                    p0 = f0.b1 * x - f0.a1 * y + p1;
                    p1 = f0.b2 * x - f0.a2 * y;
                    x = y;
                    y = f1.b0 * x + q0;
                    q0 = f1.b1 * x - f1.a1 * y + q1;
                    q1 = f1.b2 * x - f1.a2 * y;
                    u[0][i] = y;
                }
                d.az[0][0] = p0;
                d.az[0][1] = p1;
                d.az[1][0] = q0;
                d.az[1][1] = q1;
            }
            {                            /* a low sample in dn: read, feed back (left: the dry and the right's return;
                                          * right: the left's), write; each line's last two, the ramp between them */
                float yi = d.yi, l0 = d.y0[0], l1 = d.y1[0], r0 = d.y0[1], r1 = d.y1[1], st = 1.0f / (float)d.dn;
                uint32_t ds = d.ds;
                for (i = 0; i < CH; i++) {
                    if (++ds >= d.dn) {
                        float yl, yr;
                        ds = 0;
                        d.dl += (dlt - d.dl) * kdl;
                        ld_read(d.dl, &yl, &yr);
                        d.lp[0] += klp * (yr - d.lp[0]);
                        d.hp[0] += khp * (d.lp[0] - d.hp[0]);
                        d.lp[1] += klp * (yl - d.lp[1]);
                        d.hp[1] += khp * (d.lp[1] - d.hp[1]);
                        ld_write(ldl, u[0][i] + fb * (d.lp[0] - d.hp[0]));
                        ld_write(ldr, fb * (d.lp[1] - d.hp[1]));
                        d.w = d.w + 1u >= LD_N ? 0u : d.w + 1u;
                        l0 = l1;
                        r0 = r1;
                        l1 = yl;
                        r1 = yr;
                        yi = 0.0f;
                    }
                    yi += st;
                    u[0][i] = l0 + (l1 - l0) * yi;
                    u[1][i] = r0 + (r1 - r0) * yi;
                }
                d.yi = yi;
                d.y0[0] = l0;
                d.y1[0] = l1;
                d.y0[1] = r0;
                d.y1[1] = r1;
                d.ds = ds;
            }
            for (j = 0; j < 2u; j++) {   /* back up to the rate: each side its low-pass */
                const bq_t f = d.up;
                float z0 = d.uz[j][0], z1 = d.uz[j][1], *v = u[j];
                for (i = 0; i < CH; i++) {
                    float x = v[i], y = f.b0 * x + z0;
                    z0 = f.b1 * x - f.a1 * y + z1;
                    z1 = f.b2 * x - f.a2 * y;
                    v[i] = y;
                }
                d.uz[j][0] = z0;
                d.uz[j][1] = z1;
                d.ys[j] = v[CH - 1u];
            }
            {                            /* over the dry on its side; the cloud's share (150 Hz .. 4 kHz, the right
                                          * side's LD_DC later) */
                float h0 = d.sh[0], m0 = d.s1[0], n0 = d.s2[0], h1 = d.sh[1], m1 = d.s1[1], n1 = d.s2[1];
                uint32_t dcw = d.dcw;
                for (i = 0; i < CH; i++) {
                    float x = o[c + i], yl = u[0][i], yr = u[1][i], e;
                    e = x + mg * yl;
                    o[c + i] = e;
                    h0 += khs * (e - h0);
                    m0 += kls * (e - h0 - m0);
                    n0 += kls * (m0 - n0);
                    rl[c + i] += n0 * sp;
                    e = x + mg * yr;
                    o2[c + i] = e;
                    h1 += khs * (e - h1);
                    m1 += kls * (e - h1 - m1);
                    n1 += kls * (m1 - n1);
                    rr[c + i] += (float)d.dc[dcw] * (1.0f / 4096.0f) * sp;
                    d.dc[dcw] = (int16_t)br_clampf(n1 * 4096.0f, -32767.0f, 32767.0f);
                    dcw = dcw + 1u >= LD_DC ? 0u : dcw + 1u;
                    e = fm_fabsf(yl) + fm_fabsf(yr) + fm_fabsf(x);
                    peak = e > peak ? e : peak;
                }
                d.sh[0] = h0;
                d.s1[0] = m0;
                d.s2[0] = n0;
                d.sh[1] = h1;
                d.s1[1] = m1;
                d.s2[1] = n1;
                d.dcw = dcw;
            }
        }
        for (j = 0; j < 2u; j++) {
            float a = fm_fabsf(d.ys[j]) * mix;
            L.echo[j] = a > L.echo[j] ? a : L.echo[j];
            d.lp[j] = fm_flush(d.lp[j]);
            d.hp[j] = fm_flush(d.hp[j]);
            d.ys[j] = fm_flush(d.ys[j]);
            d.az[j][0] = fm_flush(d.az[j][0]);
            d.az[j][1] = fm_flush(d.az[j][1]);
            d.uz[j][0] = fm_flush(d.uz[j][0]);
            d.uz[j][1] = fm_flush(d.uz[j][1]);
            d.sh[j] = fm_flush(d.sh[j]);
            d.s1[j] = fm_flush(d.s1[j]);
            d.s2[j] = fm_flush(d.s2[j]);
        }
        send = peak * sp > send ? peak * sp : send;   /* (at most: the dry's and the echoes' peaks together) */
        if (peak < 2e-5f) {              /* nothing in it: once the line has gone round quiet, it rests */
            d.quiet += ch_t;
            if (!any && d.quiet > (float)LD_N / LD_RATE + 0.05f) {
                d.live = 0;
                d.dirty = 1;
            }
        } else {
            d.quiet = 0.0f;
        }
    }
    L.send = send;
    for (j = 0; j < LEAD_NV; j++)
        if (live[j] && !vs[j].gate && vs[j].env < 0.0005f) {
            vs[j].note = -1;
            vs[j].cut = vs[j].swap = 0;
        }
    return 1;
}

void lead_view(lead_view_t *o)
{
    const ldv_t *v = 0;
    uint32_t j;
    o->held = o->sounding = 0;
    for (j = 0; j < LEAD_NV; j++) {
        o->vnote[j] = vs[j].note < 0 ? -1 : (int8_t)(vs[j].note + 12 * L.octv);
        o->venv[j] = vs[j].note < 0 ? 0.0f : vs[j].env;
    }
    for (j = 0; j < LEAD_NV; j++)
        if (vs[j].note >= 0) {
            o->sounding++;
            o->held += vs[j].gate;
            if (!v || vs[j].age > v->age)
                v = &vs[j];
        }
    o->note = !v ? -1 : (int8_t)(v->note + 12 * L.octv);
    o->gate = v ? v->gate : 0;
    o->stage = !v || !v->gate ? 2u : v->dec ? 1u : 0u;
    o->level = v ? v->env : 0.0f;
    o->fenv = v ? v->fenv : 0.0f;
    o->cut = v ? v->flt : 0.0f;
    o->send = L.send;
    o->echol = L.echo[0];
    o->echor = L.echo[1];
    o->mod = L.mod;
    o->ons = L.ons;
    o->glides = L.glides;
}

/* Perform LIVE's (lead.h): a voice's flags, sounding (bit 0) and ever (1: POLY glides from the note it played last);
 * the pitch bit for bit (no beat against itself), the rest at half precision. A voice loaded starts nothing of its
 * own: what it was told is what it is (the delay's lines, its filters, the cloud's share: the engine's own) */
static void ld_lv_none(ldv_t *v)
{
    static const ldv_t none = {.note = -1};
    *v = none;
}
void lead_lv(br_live *c)
{
    uint32_t j, k;
    uint8_t o = (uint8_t)L.octv;
    c->u8(c, &o);
    L.octv = (int8_t)o;
    c->u8(c, &L.cap);
    c->u8(c, &L.sm_on);
    c->f16(c, &L.vph);
    c->f16(c, &L.mod);
    c->f16(c, &L.bend);
    c->f16(c, &L.bend_t);
    for (k = 0; k < 8u; k++)
        c->f16(c, &L.sm[k]);
    c->u32(c, &L.rng);
    c->u32(c, &L.age);
    c->u32(c, &L.ons);
    c->u32(c, &L.glides);
    c->f32(c, &d.dl);                    /* (TIME where it glides: the echoes as far apart as the FM-1's) */
    for (j = 0; j < LEAD_NV; j++) {
        ldv_t *v = &vs[j];
        uint8_t x = (uint8_t)((v->note >= 0) | v->ever << 1), nt = (uint8_t)v->note;
        c->u8(c, &x);
        if (!c->save) {
            ld_lv_none(v);
            v->ever = x >> 1 & 1u;
        }
        if (x & 2u)
            c->f32(c, &v->oct);
        if (!(x & 1u))
            continue;
        c->u8(c, &nt);
        v->note = (int8_t)(nt & 0x7Fu);
        c->u8(c, &v->gate);
        c->u8(c, &v->dec);
        c->u8(c, &v->fdec);
        c->u8(c, &v->set);
        c->u8(c, &v->gliding);
        c->u8(c, &v->cut);
        c->u8(c, &v->swap);
        c->f32(c, &v->oct_t);
        c->u32(c, &v->ph1);
        c->u32(c, &v->ph2);
        c->u32(c, &v->age);
        c->f16(c, &v->vel);
        c->f16(c, &v->vel_t);
        c->f16(c, &v->prs);
        c->f16(c, &v->prs_t);
        c->f16(c, &v->env);
        c->f16(c, &v->lvl);
        c->f16(c, &v->fenv);
        c->f16(c, &v->sw_vel);
        c->f16(c, &v->s1);
        c->f16(c, &v->s2);
        c->f16(c, &v->s3);
        c->f16(c, &v->s4);
        c->f16(c, &v->n1);
        c->f16(c, &v->n2);
        c->f16(c, &v->flt);
        c->f16(c, &v->res);
        c->f16(c, &v->drift);
        c->f16(c, &v->drift_t);
        c->f16(c, &v->scoop);
    }
}
#endif
