/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* LEAD (card #144): a warm analog lead over everything, played from the keys alone (no arpeggio, no sequence). MODE:
 * MONO, one note at a time (legato: a key played while another is down glides there (GLIDE) without striking again),
 * or POLY, LEAD_NV voices, a free one taken, else the quietest let go, else the oldest held (each gliding from the note
 * it played last; one still sounding out first, in 2.9 ms, its note then from silence). A voice: two oscillators (WAVE:
 * a saw, through a square, to a narrow pulse, then WHISTLE, a sine and its breath through a band-pass at the note, a
 * scoop at its attack; DETUNE the second's), BREATH's noise, a soft drive (DRIVE), a 24 dB low-pass (two state variable
 * stages, RESONANCE on the second) its CUTOFF opened by the envelope (FILTER ENV), the key and the velocity; one ADSR for
 * the level and the filter; VIBRATO, and the SMK-37's mod wheel over it; its pitch wheel two semitones either way. Its
 * own delay, GLINT's (ping-pong, TIME on the beat, 0.5 s at most; its lines at a quarter of the rate, to some 4 kHz,
 * each echo a little darker); its share of the cloud (SPACE), the dry and the echoes as heard, whatever the delay's
 * MIX.
 * In the app's builds, the FM-1's and the SMK-37's (FRS_LEAD: app.h, fm1.toml's o2_cflags; admin #741), the engine's
 * own without it (the iPad, the energy tool: no lead.c there yet; STREAM's has it since card #152). The engine (frasil.c)
 * calls these from the render, with its queued events. In LIVE's state since card #152 (live.c's LEAD part, "FRld":
 * lead_lv); each scene's in the app's own object (app.c ldp_*), in a shard after its scene ("FRS~"). */
#pragma once
#include <stdint.h>
#include "brlive.h"

#ifndef FRS_LEAD
#define FRS_LEAD 0
#endif

#define LEAD_NV 6                        /* POLY's voices (MONO: the first alone; admin #685: four, RELEASE's tails took
                                          * them, a chord of three left one for every tail) */

typedef struct {
    float wave;                          /* 0 a saw .. 0.5 a square .. 1 a narrow pulse */
    int whistle;                         /* WAVE's WHISTLE: a sine, its breath at the note (wave aside) */
    int poly;                            /* MODE: 0 MONO, 1 POLY */
    float cutoff;                        /* 0..1: 60 Hz .. 12 kHz (before the envelope, the key, the velocity) */
    float resonance;                     /* 0..1: none .. near ringing */
    float detune;                        /* 0..1: the second oscillator 0 .. 25 cents up */
    float breath;                        /* 0..1: noise before the filter (WHISTLE's: a band-pass at the note, some always) */
    float drive;                         /* 0..1: the drive before the filter, DRIVE_0 as it always was */
    float glide;                         /* 0..1: 0 .. 2 s from one note to the next, legato (as BASS's GLIDE) */
    float fenv;                          /* 0..1: the envelope on the cutoff, 0 .. 5 octaves */
    float vibrato;                       /* 0..1: its depth at rest, 0 .. half a semitone (the mod wheel's, the larger) */
    int octave;                          /* -2 .. 2 octaves on the keys */
    float attack, decay, sustain, release;   /* 0..1, as BASS's: 5 ms .. 5 s, 10 ms .. 10 s, the level, 30 ms .. 15 s */
    float fattack, fdecay, fsustain, frelease;   /* FILTER ENV's own, the same way (each voice its own, struck with it) */
    float level;                         /* MIXER's LEAD: 1 as it is */
    float dtime_s;                       /* the delay (GLINT's: ping-pong), a repeat, seconds (core.c: TIME on the beat) */
    float dfb;                           /* 0..1: its feedback, 0 .. 0.88 (GLINT's) */
    float dmix;                          /* 0..1: its echoes over the dry */
    float space;                         /* 0..1: the lead's share of the cloud, its echoes as heard with it */
} lead_params_t;
#define LEAD_DRIVE_0 (32.0f / 127.0f)    /* DRIVE's default: the drive the first LEAD had */
#define LEAD_DELAY_MAX 0.5f              /* seconds a repeat at most (longer: halved, still on the beat, as GLINT's) */

void lead_init(void);
void lead_reset(void);                   /* everything as at power-on (frasil_reset: an engine opened again; not on the
                                          * FM-1) */
void lead_hush(void);                    /* silence now, the delay's echoes too (PANIC, CUT) */
void lead_on(int note, float vel, int legato);   /* MONO: to this note (OCTAVE on it): legato, gliding there, not struck */
void lead_off(void);                     /* every voice let go (MONO's key up; MODE turned) */
void lead_poly_on(int note, float vel);  /* POLY: a voice struck on this note (its own, a free one, else the quietest
                                          * let go, else the oldest held) */
void lead_poly_off(int note);
void lead_press(int note, float p);      /* 0..1: louder, the filter opens; note -1 MONO's voice, else POLY's on it */
void lead_mod(float m);                  /* 0..1: the mod wheel, VIBRATO's depth if more */
void lead_bend(float semis);             /* the pitch wheel, semitones */
/* n frames (a multiple of 32): the lead, its left into o, its right into o2 (set, not added: the dry the same in both,
 * each side's echoes on it), its share of the cloud added into rl, rr; shed the shedding's level: the LEAD shed last
 * (admin #636, #639: the drone gives first, frasil.c SHED_NP, SHED_NV), POLY all its voices to step 4, then 5, 5, 4, 3
 * (lead.c LD_CAP), its two oscillators always (DETUNE heard); MONO nothing. 0: nothing sounds (o, o2 untouched) */
int lead_render(float *o, float *o2, float *rl, float *rr, uint32_t n, const lead_params_t *p, int shed);

typedef struct {
    int8_t note;                         /* -1: silent; else the newest voice's, as it sounds (OCTAVE on it) */
    uint8_t gate;                        /* held */
    uint8_t stage;                       /* 0 ATTACK, 1 DECAY then SUSTAIN, 2 RELEASE */
    uint8_t held, sounding;              /* the voices held, sounding (held or fading) */
    float level;                         /* the envelope, 0..1 */
    float fenv;                          /* FILTER ENV's own envelope, 0..1 */
    float cut;                          /* its CUTOFF as it glides, an LFO's on it, 0..1 */
    float send;                          /* the cloud's share at its loudest, the last render */
    float echol, echor;                  /* each side's echo (MIX on it) at its loudest, the last render */
    float mod;                           /* the mod wheel as it came (lead_mod), 0..1 */
    uint32_t ons, glides;                /* the notes struck, the notes glided to (the tests) */
    int8_t vnote[LEAD_NV];               /* each voice's note as it sounds (OCTAVE on it), -1 silent (PERFORM's FRAME) */
    float venv[LEAD_NV];                 /* and its envelope, 0..1 */
} lead_view_t;
void lead_view(lead_view_t *v);
/* Perform LIVE's state (card #152, live.c's LEAD part): each voice (its note, stage and gate, the pitch where it glides,
 * its two envelopes, velocity, pressure, phases, its filter's states, its drift, WHISTLE's scoop; a silent one the
 * pitch it played last, POLY's glide), the wheels, the settings gliding, the generator, the delay's time where it
 * glides. Not the delay's lines nor their filters: each engine's own, as the cloud's room */
void lead_lv(br_live *c);
