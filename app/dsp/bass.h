/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* BASS: FRASIL's bass, one deep voice under the drones. One of eight MODELs (a sine, a triangle, a square, a saw,
 * REESE's two detuned saws, a sine with its octave below, an organ's three drawbars, a growling FM sine), DRIVE
 * into a soft clip, through a resonant low-pass (FILTER, RESONANCE), gliding from note to note (GLIDE). Held like a
 * drone: it sounds until it is let go, one note at a time. Dry: none of it goes into the reverb.
 * The engine (frasil.c) calls these from the render, with its queued events. */
#pragma once
#include <stdint.h>

#define BASS_NMODEL 8

typedef struct {
    int model;                           /* 0 .. BASS_NMODEL - 1 */
    float drive;                         /* 0..1: clean .. soft-clipped */
    float filter;                        /* 0..1: the low-pass, 40 Hz .. 5 kHz */
    float resonance;                     /* 0..1: its peak, Q 0.5 .. 6 */
    float glide;                         /* 0..1: 0 .. 2 s from one note to the next */
    float level;                         /* 0..1 */
    float attack, release;               /* 0..1: 5 ms .. 5 s; 30 ms .. 15 s */
} bass_params_t;

void bass_init(void);
void bass_on(int note, float vel);       /* render side: to this note, from where it is if it sounds */
void bass_off(void);
void bass_press(float p);                /* 0..1: louder, and its filter opens */
/* n frames (a multiple of 32) added into l, r */
void bass_render(float *l, float *r, uint32_t n, const bass_params_t *p);

typedef struct {
    int8_t note;                         /* -1: silent */
    uint8_t gate;                        /* held */
    float level;                         /* the envelope, 0..1 */
    float pressure;                      /* 0..1 */
} bass_view_t;
void bass_view(bass_view_t *v);
