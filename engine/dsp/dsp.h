/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's DSP: what every unit shares (sample rate, the pool section, small helpers).
 * The DSP units are compiled apart at -O2 (fm1.toml o2) and run in the audio interrupt; they see fastmath.h but not
 * the platform. Float only: the FM-1's FPU is single precision, and inlined 64-bit integer multiplies beside 16-bit
 * loads corrupt registers on this CPU (felucca-bsp docs/CPU.md). */
#pragma once
#include <stdint.h>
#include "fastmath.h"

/* the sample rate. On the FM-1 (the AC79's compiler: __PI32V2__) it is 44.1 kHz alone, a constant: the DSP compiled
 * as it always was (the reverb's lengths immediate in its loop, nothing more in RAM). Elsewhere (the FM-1's simulator,
 * the browser, an iPad) it is the rate frasil_init was given, and what holds time is sized for BR_FS_MAX. A step made
 * for 44.1 kHz is made over for the rate: br_k, br_d, br_n, each exactly the one it was at 44.1 kHz (frasil.h
 * FRASIL_FS has the same condition) */
#if defined __PI32V2__ || defined FRASIL_FS_CONST
#define BR_FS_CONST 1
#define BR_FS 44100.0f
#define BR_INV_FS (1.0f / 44100.0f)
#define BR_R44 1.0f                      /* 44.1 kHz over the rate: a step here, in steps at 44.1 kHz */
#define BR_FS_MAX 44100
#else
#define BR_FS_CONST 0
extern float br_fs, br_inv_fs, br_r44;   /* (frasil.c, frasil_init) */
#define BR_FS br_fs
#define BR_INV_FS br_inv_fs
#define BR_R44 br_r44
#ifndef BR_FS_MAX
#define BR_FS_MAX 96000
#endif
#endif

/* big zero-initialised buffers: the .pool RAM region on the device */
#ifndef __PI32V2__
#define BR_POOL
#else
#define BR_POOL __attribute__((section(".pool")))
#endif

/* code run from RAM on the device (.ram_text, some 20 KB free: the flash's cache misses not paid there): a function
 * that calls nothing (the flash's code is out of a call's reach from there), called through a pointer for the same
 * reason (BR_HOT_CALL) */
#ifndef __PI32V2__
#define BR_HOT
#else
#define BR_HOT __attribute__((section(".ram_text"), noinline))
#endif

static inline __attribute__((always_inline)) float br_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float br_lerp(float a, float b, float t) { return a + (b - a) * t; }

/* the steps made for 44.1 kHz (a sample's, a block's of 256 frames, a chunk's), at the rate: a one-pole's coefficient
 * k (x += k (target - x): the same time to settle), a decay d a step (x *= d: the same fall), a count of steps n (the
 * same time; unrounded) */
static inline float br_k(float k) { return BR_R44 == 1.0f ? k : 1.0f - fm_powf(1.0f - k, BR_R44); }
static inline float br_d(float d) { return BR_R44 == 1.0f ? d : fm_powf(d, BR_R44); }
static inline float br_n(float n) { return BR_R44 == 1.0f ? n : n / BR_R44; }

/* soft clip: unity slope at 0, x - 4/27 x^3 up to |x| = 1.5, where it reaches +-1 with a flat slope */
static inline __attribute__((always_inline)) float br_soft(float x)
{
    x = br_clampf(x, -1.5f, 1.5f);
    return x - 0.148148148f * x * x * x;
}

/* 32-bit xorshift: noise and drift */
static inline uint32_t br_rand(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static inline float br_randf(uint32_t *s) { return (float)(int32_t)br_rand(s) * (1.0f / 2147483648.0f); }   /* -1..1 */
