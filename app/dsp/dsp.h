/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's DSP: what every unit shares (sample rate, the pool section, small helpers).
 * The DSP units are compiled apart at -O2 (fm1.toml o2) and run in the audio interrupt; they see fastmath.h but not
 * the platform. Float only: the FM-1's FPU is single precision, and inlined 64-bit integer multiplies beside 16-bit
 * loads corrupt registers on this CPU (felucca-bsp docs/CPU.md). */
#pragma once
#include <stdint.h>
#include "fastmath.h"

#define BR_FS 44100.0f
#define BR_INV_FS (1.0f / 44100.0f)

/* big zero-initialised buffers: the .pool RAM region on the device */
#ifdef PLAT_HOST
#define BR_POOL
#else
#define BR_POOL __attribute__((section(".pool")))
#endif

/* code run from RAM on the device (.ram_text, some 20 KB free: the flash's cache misses not paid there): a function
 * that calls nothing (the flash's code is out of a call's reach from there), called through a pointer for the same
 * reason (BR_HOT_CALL) */
#ifdef PLAT_HOST
#define BR_HOT
#else
#define BR_HOT __attribute__((section(".ram_text"), noinline))
#endif

static inline __attribute__((always_inline)) float br_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float br_lerp(float a, float b, float t) { return a + (b - a) * t; }

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
