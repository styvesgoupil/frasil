/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* stream.wasm: FRASIL's engine alone and a stream played on it (engine/frasil_stream.c's fst_air), for FRASIL STREAM's radio
 * (website/stream/). Standalone WebAssembly, no imports it needs, about 200 KB. One engine an instance: the page makes as many
 * as it plays at once (two across a change of tune, a third on the page for the picture).
 *   st_alloc(n), st_free(p)        bytes in its memory (a stream's: kept while it plays)
 *   st_open(p, n, rate)            fst_read, then a fresh engine at the stream's start: 0, else fst_read's code (-1 bad, -2 its
 *                                  CRC, -3 a newer version, -4 another layout: another engine's), -9 the engine would not open
 *   st_read(p, n)                  fst_read alone; st_info() then: ms, events, rate, flags, nparam, lost (u32s)
 *   st_skip(ms)                    to the stream's millisecond ms, nothing rendered (frs_skip_to)
 *   st_render(n)                   n frames (128 a quantum) into st_l(), st_r(): 256 at most
 *   st_ms()                        the stream's clock     st_live(): frs_live's floats
 *   st_pcount(), st_param(p), st_pmax(p)   the settings, for the picture
 *   st_version()                   frs_version's string */
#include <stdint.h>
#include <stdlib.h>
#include <emscripten/emscripten.h>
#include "frasil_engine.h"
#include "frasil_stream.h"

static fst_air air;
static fst_file file;
static float L[256], R[256], live[FRS_LIVE];
static uint32_t info[6];

EMSCRIPTEN_KEEPALIVE uint8_t *st_alloc(uint32_t n) { return malloc(n); }
EMSCRIPTEN_KEEPALIVE void st_free(uint8_t *p) { free(p); }

EMSCRIPTEN_KEEPALIVE int st_read(const uint8_t *b, uint32_t n)
{
    fst_file f;
    int r = fst_read(b, n, &f);
    info[0] = f.ms;
    info[1] = f.events;
    info[2] = f.rate;
    info[3] = f.flags;
    info[4] = f.nparam;
    info[5] = f.lost;
    return r;
}
EMSCRIPTEN_KEEPALIVE uint32_t *st_info(void) { return info; }

EMSCRIPTEN_KEEPALIVE int st_open(const uint8_t *b, uint32_t n, float fs)
{
    int r = fst_read(b, n, &file);
    if (r)
        return r;
    return fst_air_open(&air, &file, fs) ? -9 : 0;
}

EMSCRIPTEN_KEEPALIVE void st_skip(uint32_t ms) { fst_air_skip(&air, ms); }

EMSCRIPTEN_KEEPALIVE void st_render(uint32_t n)
{
    frs_render(L, R, n > 256u ? 256u : n);
    fst_air_after(&air);
}

EMSCRIPTEN_KEEPALIVE float *st_l(void) { return L; }
EMSCRIPTEN_KEEPALIVE float *st_r(void) { return R; }
EMSCRIPTEN_KEEPALIVE uint32_t st_ms(void) { return fst_air_ms(&air); }
EMSCRIPTEN_KEEPALIVE float *st_live(void)
{
    frs_live(live);
    return live;
}
EMSCRIPTEN_KEEPALIVE uint32_t st_pcount(void) { return frs_param_count(); }
EMSCRIPTEN_KEEPALIVE uint32_t st_param(uint32_t p) { return frs_param_get(p); }
EMSCRIPTEN_KEEPALIVE uint32_t st_pmax(uint32_t p)
{
    uint32_t m = 1;
    frs_param_info(p, 0, &m, 0);
    return m;
}
EMSCRIPTEN_KEEPALIVE const char *st_version(void) { return frs_version(); }
