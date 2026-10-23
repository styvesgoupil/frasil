/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* GLINT: FRASIL's icy glints, a small FM voice played over the drones. Never latched: a glint sounds while its
 * key is down and rings out as a bell does (or, with SUSTAIN, holds as a lead does). Two operators (a modulator at RATIO into a carrier), the index bright
 * at the strike and mellower after it, a soft onset; 4 at once.
 * The engine (frasil.c) calls these from the render, with its queued events. */
#pragma once
#include <stdint.h>

#define GLINT_NV 4

typedef struct {
    float ratio;                         /* the modulator's frequency over the carrier's */
    float bright;                        /* 0..1: the FM index, 0 .. 5 */
    float decay;                         /* 0..1: 80 ms .. 6 s to silence */
    float level;                         /* 0..1 */
    float space;                         /* 0..1: how much goes into the reverb */
    float scatter;                       /* 0..1: each glint's place in the field, and a little detune */
    float attack;                        /* 0..1: 2 ms .. 1.5 s (a pluck .. a bowed glass) */
    float ping;                          /* 0..1: how much brighter the strike than the ring (0 steady, an organ;
                                          * 1 a pluck whose ring is nearly pure) */
    float vibrato;                       /* 0..1: the pressure's wiggle on the pitch (a steady press stays in tune),
                                          * 0 off .. 25 cents either way */
    float sustain;                       /* 0..1: the level it settles at while the key is down (0: a bell, it fades
                                          * out; up: a lead, held) */
    /* the delay, on the glints alone */
    float delay_s;                       /* a repeat's time, seconds (the app syncs it to the tempo) */
    float delay_fb;                      /* 0..1: feedback */
    float delay_mix;                     /* 0..1: the echoes' level */
    int delay_pp;                        /* 1: ping-pong, 0: mono */
} glint_params_t;

void glint_init(void);
void glint_on(int note, float vel, const glint_params_t *p);      /* render side */
void glint_off(int note, const glint_params_t *p);
void glint_press(int note, float p);
/* n frames added into l, r (dry) and send_l, send_r (into the reverb) */
void glint_render(float *l, float *r, float *send_l, float *send_r, uint32_t n, const glint_params_t *p);
/* the delay: the glints' echoes added to l, r (which hold the glints alone) and to the sends, n even */
void glint_delay(float *l, float *r, float *send_l, float *send_r, uint32_t n, const glint_params_t *p);
#define GLINT_DELAY_MAX 0.92f            /* seconds a repeat at most (longer: halved, still on the beat) */

typedef struct {
    int8_t note;                         /* -1: silent */
    float level;                         /* 0..1 */
    uint32_t age;                        /* render blocks since the strike */
    float cents;                         /* VIBRATO's bend now */
} glint_view_t;
void glint_view(glint_view_t v[GLINT_NV]);
