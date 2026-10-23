/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* NUAGE: FRASIL's reverb. A modulated 8-line feedback delay network with allpass diffusion in its loop, stereo
 * input diffusion and pre-delay, and a shimmer: a pitch shifter in the loop that sends the tail up an interval
 * each time round. All its lines share one 128 KiB buffer of 16-bit samples (the pool). */
#pragma once
#include <stdint.h>

typedef struct {
    float size;        /* 0..1: the space, 0.15x .. 1x of the longest lines (about 60 .. 140 ms) */
    float decay;       /* 0..1: RT60 0.4 s .. 60 s */
    float damp;        /* 0..1: how long the highs ring, 0.25 s .. 40 s (1: as long as the rest) */
    float diffusion;   /* 0..1: echoes into a cloud */
    float mod;         /* 0..1: the lines' wander (chorus in the tail) */
    float shimmer;     /* 0..1: the shifted tail fed back */
    float shimmer_tone; /* 0..1: its brightness, a low-pass on what is shifted, 700 Hz .. 8 kHz */
    float interval;    /* the shimmer's interval in semitones (12, 7, 5, 19, -12 ...) */
    float predelay;    /* 0..1: 0 .. 200 ms */
    float lowcut;      /* 0..1: 20 .. 500 Hz, on the way in */
    float width;       /* 0..1: the tail's stereo width */
    int freeze;        /* 1: the tail holds, nothing comes in */
} nuage_params_t;

void nuage_init(void);
void nuage_set(const nuage_params_t *p);    /* the targets; it glides to them (main loop or between blocks) */
/* the wet signal of n frames of stereo input (n a multiple of 16) */
void nuage_process(const float *in_l, const float *in_r, float *out_l, float *out_r, uint32_t n);
