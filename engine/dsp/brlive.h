/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The engine's running state, field by field (Perform LIVE, card #131: core.c's live.c, frs_live_save and frs_live_load):
 * one visitor saves it or loads it, so that each part names its fields once. A part's own visit (frasil_lv, glint_lv,
 * bass_lv, nuage_lv) is handed it: saving, each field goes into the stream; loading, each is written back from it, in
 * the same order. The DSP's fields go at half precision (f16): the FM-1's sound never runs bit for bit with an engine
 * alone (its render is the audio interrupt's, its blocks on their own clock), so what is told is where it is heard,
 * close enough to go on from */
#pragma once
#include <stdint.h>

typedef struct br_live br_live;
struct br_live {
    int save;                            /* 1: the fields into the stream; 0: from it */
    void (*u8)(br_live *, uint8_t *);
    void (*u32)(br_live *, uint32_t *);
    void (*f32)(br_live *, float *);     /* bit for bit */
    void (*f16)(br_live *, float *);     /* half precision */
};
