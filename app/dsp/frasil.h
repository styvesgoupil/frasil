/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's sound engine: up to 8 drone voices, each three layers (CHOEUR: detuned saws or pulses through a
 * low-pass; PARTIELS: additive partials that drift on their own, a formant; SOUFFLE: noise or grains through
 * resonators tuned to the note), their slow drift (MOTION) and a tide on the timbre, then warmth, the NUAGE reverb
 * and a soft limit.
 *
 * Threads: the main loop calls frasil_set and the note functions (the notes are queued); the audio interrupt calls
 * frasil_render. Nothing here plays by itself: a voice sounds because a note was played and stops when it is
 * released. */
#pragma once
#include <stdint.h>
#include "nuage.h"
#include "glint.h"
#include "bass.h"

#define FR_NV 8

typedef struct {
    /* the layers */
    float choir, partials, souffle;      /* levels 0..1 */
    float timbre;                        /* brightness of all three, 0..1 (0.5 neutral) */
    float detune;                        /* CHOEUR: the saws' spread, 0 .. 40 cents */
    float cutoff;                        /* CHOEUR: low-pass, 0..1: 1 .. 40 harmonics */
    float tilt;                          /* PARTIELS: 0 bright .. 1 dark */
    float odd;                           /* PARTIELS: 0 all harmonics .. 1 odd ones only */
    float stretch;                       /* PARTIELS: 0 harmonic .. 1 bell-like */
    float resonance;                     /* SOUFFLE: 0 air .. 1 glass */
    float color;                         /* SOUFFLE: the resonators' ratios, harmonic .. odd .. bell */
    /* the voices' shape */
    float attack, release;               /* 0..1: 5 ms .. 30 s; 50 ms .. 60 s */
    float pressure;                      /* how much pressure swells and opens a voice, 0..1 */
    float velocity;                      /* how much velocity sets a voice's level, 0..1 */
    float spread;                        /* the voices across the stereo field, 0..1 */
    /* motion */
    float motion, motion_rate;           /* drift: depth, speed (0 slow .. 1 fast) */
    float tide, tide_rate;               /* the tide on the timbre: depth, speed (one turn 5 min .. 10 s) */
    /* out */
    float drive;                         /* warmth, 0..1 */
    float mix;                           /* reverb, 0 dry .. 1 wet */
    float tune;                          /* semitones, -1..1 */
    float pump_drone, pump_bass, pump_glint;   /* PUMP: how deep each track ducks on the beat, 0..1 */
    float pump_rel;                      /* the share of the period it takes to come back, 0.1 .. 1 */
    float pump_att;                      /* how long it takes to go down, seconds */
    float pump_off;                      /* where in the period it goes down, 0..1 */
    float comp_thr;                      /* MASTERING: the threshold, dB (-40 .. 0) */
    float comp_ratio;                    /* 1 (off) .. 20 */
    float comp_rel;                      /* its release, seconds */
    float comp_gain;                     /* the make-up gain, linear */
    float mx_drone, mx_bass, mx_glint, mx_cloud;   /* MIXER: each track's level (1 as it is), the cloud's return */
    /* the layers' second pages */
    float ch_shape;                      /* CHOEUR: each saw toward a pulse, 0 saw .. 0.5 square .. 1 a narrow pulse */
    float ch_interval;                   /* CHOEUR: the centre saw's frequency over the note's (1, 0.5, 1.5, 2) */
    float ch_reso;                       /* CHOEUR: the filter's peak, 0 none (Q 0.5) .. 1 (Q 20) */
    float pa_evolve;                     /* PARTIELS: their levels wandering, beyond MOTION's, 0..1 */
    float pa_beat;                       /* PARTIELS: each one pulsing at its own pace, 0 none .. 1 deep and quick */
    float pa_formant;                    /* PARTIELS: a formant's place, log2 of the harmonic, 0 .. 3 */
    float pa_peak;                       /* PARTIELS: how strong, 0 none .. 1 the harmonic there alone */
    float br_grain;                      /* SOUFFLE: the noise, 0 a hiss .. 1 sparse grains */
    float br_pitch;                      /* SOUFFLE: the resonators' frequencies over the note's (0.5 .. 4) */
    float br_tone;                       /* SOUFFLE: the noise's colour, -1 dark .. 0 white .. 1 bright */
    float br_air;                        /* SOUFFLE: the noise itself beside the resonators, 0..1 */
    float fullness;                      /* the whole drone, 1 as its settings are, down to thin (less detune, fewer
                                          * partials, little breath, narrower, less warmth, its body high-passed) */
    float sink;                          /* GENERATIVE FX's SINK: everything under, a low-pass on the whole mix,
                                          * 0 none .. 1 (some 120 Hz) */
    nuage_params_t rev;
    glint_params_t glint;
    bass_params_t bass;
    int ch_type;                         /* CHOEUR: the filter, 0 low-pass, 1 band-pass, 2 high-pass */
} frasil_params_t;

void frasil_init(void);
void frasil_set(const frasil_params_t *p);        /* main loop */
void frasil_note_on(int note, float vel);        /* main loop: vel 0..1 */
void frasil_note_off(int note);
void frasil_all_off(void);
void frasil_pressure(int note, float p);         /* 0..1; note -1: every voice (channel pressure) */
void frasil_swell(float s);
/* GLINT, the icy glints over the drones (glint.h): never latched */
void frasil_glint_on(int note, float vel);
void frasil_glint_off(int note);                 /* note -1: every glint */
void frasil_click(int accent);                   /* main loop: the metronome's tick (accent: the bar's first) */
void frasil_glint_pressure(int note, float p);
/* BASS, one deep note under the drones (bass.h): held until let go; a new note glides from the last */
void frasil_bass_on(int note, float vel);
void frasil_bass_off(void);
void frasil_bass_pressure(float p);
/* PUMP's beat, from the main loop as often as it likes: where in its period the beat is now (0..1) and how far it
 * moves a sample (0: no pump); the render follows it */
void frasil_pump(float phase, float inc);
void frasil_render(int32_t *lr, uint32_t n, float gain);   /* audio interrupt: n frames (a multiple of 64), 24-bit;
                                                           * gain 0..1 (MASTER) */
void frasil_panic(void);                         /* silence now */
/* the load: 0 everything; 1 .. 3 fewer partials, then fewer voices (the oldest released first). From the
 * platform's overload hook, and back to 0 as the load allows (the main loop) */
void frasil_shed(int level);
int frasil_shed_level(void);

/* for the screen */
typedef struct {
    int8_t note;                         /* -1: free */
    uint8_t gate;                        /* held (or latched) */
    float level;                         /* the envelope, 0..1 */
    float pressure;                      /* 0..1 */
} frasil_view_t;
void frasil_view(frasil_view_t v[FR_NV]);
float frasil_tide(void);
void frasil_glint_view(glint_view_t v[GLINT_NV]);                  /* the tide now, -1..1 */
void frasil_bass_view(bass_view_t *v);
void frasil_meters(float m[4]);           /* the tracks' recent peaks, 0..1: drones, bass, glints, the cloud */
float frasil_pump_gain(void);             /* the drones' pump gain now, 0..1 (1: no duck) */
float frasil_comp_gr(void);               /* the compressor's gain reduction now, dB (0 or less) */
float frasil_out_level(void);             /* the output's recent peak, 0..1 */
