/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* stream.wasm: FRASIL's engine alone and a stream played on it (engine/frasil_stream.c's fst_air), for FRASIL STREAM's radio
 * (website/stream/); or a performance heard live (fst_live: Perform LIVE's listener, design/perform/listen/live-player.js).
 * Standalone WebAssembly, no imports it needs, about 200 KB. One engine an instance, a stream's or a live one's (the last
 * opened): the page makes as many as it plays at once (two across a change of tune, a third on the page for the picture).
 *   st_alloc(n), st_free(p)        bytes in its memory (a stream's: kept while it plays)
 *   st_open(p, n, rate)            fst_read, then a fresh engine at the stream's start: 0, else fst_read's code (-1 bad, -2 its
 *                                  CRC, -3 a newer version, -4 another layout: another engine's), -9 the engine would not open
 *   st_read(p, n)                  fst_read alone; st_info() then: ms, events, rate, flags, nparam, lost (u32s)
 *   st_skip(ms)                    to the stream's millisecond ms, nothing rendered (frs_skip_to)
 *   st_render(n)                   n frames (128 a quantum) into st_l(), st_r(): 256 at most
 *   st_ms()                        the stream's clock (live: the engine's milliseconds since it opened)
 *   st_live()                      frs_live's floats
 *   st_pcount(), st_param(p), st_pmax(p)   the settings, for the picture
 *   st_version()                   frs_version's string
 * Live (LIVE.md section 3):
 *   st_live_open(scene_p, state_p, state_n, moves_p, moves_n, tracks, rate)   a fresh engine from a start (fst_fresh,
 *                                  quiet): the scene's st_scene_bytes(), the state's state_n bytes, moves_n (setting,
 *                                  value) pairs, the tracks playing (bit 0 the drones, 1 GLINT, 2 the bass): 0, or -1
 *                                  (a state or moves too long, or the engine would not open)
 *   st_op(op, a_p, n)              an op now, its n args (fst_op): 0 done, 1 not known (ignored), -1 refused
 *   st_op_scene(op, scene_p)       FST_SCENE, FST_SCENE_NOW (FST_ANCHOR, FST_STORE) with the scene's bytes: the same
 *   st_live_counts()               done, missed, refused (u32s): the ops since st_live_open
 *   st_scene_bytes()               FRS_SCENE_BYTES: a start's scene of another length is another layout's
 * Live, each op in its millisecond (card #131: every listener's engine the same as the artist's, LIVE.md section 3):
 *   st_live_state(st_p, n, scene_p)   the running state that goes with the start just opened (frs_live_load, the start's
 *                                  scene): 0, or -1 not this engine's (then as the start alone opened it)
 *   st_now()                       the engine's clock, the millisecond it runs next (frs_ms; after a state, the artist's
 *                                  engine's)
 *   st_ms_after(n)                 st_now once n frames more are rendered (what a render of n runs: st_now up to it, less
 *                                  one)
 *   st_at(ms, op, a_p, n)          an op taken first in the engine's millisecond ms (at once if gone by): 0, -1 no room
 *   st_at_scene(ms, op, scene_p)   the same, a scene's op with its bytes
 *   st_at_state(ms, st_p, n, scene_p, cmp)   a snap's running state (and its start's scene) loaded at its place in ms;
 *                                  cmp: left when this engine's own is the very same. One waits at most: 0, -1
 *   st_state_ms()                  the millisecond a waiting state is taken in, 0xFFFFFFFF none
 *   st_cut()                       the queue from a waiting state on dropped (an engine copied to fade out goes on
 *                                  without it)
 *   st_live_skip(ms)               the brain alone to the millisecond ms, its ops taken there, nothing rendered (to
 *                                  catch up: frs_skip_to)
 *   st_live_save(out_p, max, scene_p)   frs_live_save: the running state's bytes, its length (0 none)
 *   st_live_more()                 late, loads, same, full (u32s): ops taken after their millisecond, states loaded,
 *                                  states left (the very same), ops or states with no room */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten/emscripten.h>
#include "frasil_engine.h"
#include "frasil_stream.h"

static fst_air air;
static fst_file file;
static fst_live lv;
static fst_start start;
static int on_air;                       /* (the engine is a stream's: fst_air's hook and clock; else a live one's) */
static uint32_t live_t0;
static float L[256], R[256], live[FRS_LIVE];
static uint32_t info[6], counts[3];

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
    on_air = 1;
    return fst_air_open(&air, &file, fs) ? -9 : 0;
}

EMSCRIPTEN_KEEPALIVE void st_skip(uint32_t ms)
{
    if (on_air)
        fst_air_skip(&air, ms);
}

EMSCRIPTEN_KEEPALIVE void st_render(uint32_t n)
{
    frs_render(L, R, n > 256u ? 256u : n);
    if (on_air)
        fst_air_after(&air);
}

EMSCRIPTEN_KEEPALIVE float *st_l(void) { return L; }
EMSCRIPTEN_KEEPALIVE float *st_r(void) { return R; }
EMSCRIPTEN_KEEPALIVE uint32_t st_ms(void) { return on_air ? fst_air_ms(&air) : frs_ms() - live_t0; }
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

/* ---- live */
EMSCRIPTEN_KEEPALIVE uint32_t st_scene_bytes(void) { return FRS_SCENE_BYTES; }

EMSCRIPTEN_KEEPALIVE int st_live_open(const uint8_t *scene, const uint8_t *state, uint32_t state_n, const uint8_t *moves,
                                      uint32_t moves_n, uint32_t tracks, float fs)
{
    if (state_n > FRS_STATE_BYTES || moves_n > FST_MOVES)
        return -1;
    memcpy(start.scene, scene, FRS_SCENE_BYTES);
    if (state_n)
        memcpy(start.state, state, state_n);
    start.state_n = state_n;
    if (moves_n)
        memcpy(start.moves, moves, 2u * moves_n);
    start.moves_n = (uint8_t)moves_n;
    start.tracks = (uint8_t)tracks;
    on_air = 0;
    if (fst_live_open(&lv, &start, fs))
        return -1;
    live_t0 = frs_ms();
    return 0;
}

EMSCRIPTEN_KEEPALIVE int st_op(uint32_t op, const uint8_t *a, uint32_t n)
{
    return op > 255u ? fst_op(&lv, 0, 0, 0, 0) : fst_op(&lv, (uint8_t)op, a, n, 0);   /* (no op 0: not known, counted) */
}
EMSCRIPTEN_KEEPALIVE int st_op_scene(uint32_t op, const uint8_t *scene)
{
    return op > 255u ? fst_op(&lv, 0, 0, 0, 0) : fst_op(&lv, (uint8_t)op, 0, 0, scene);
}
EMSCRIPTEN_KEEPALIVE uint32_t *st_live_counts(void)
{
    counts[0] = lv.done;
    counts[1] = lv.missed;
    counts[2] = lv.refused;
    return counts;
}

/* ---- live, each op in its millisecond (card #131) */
static uint32_t more[4];
EMSCRIPTEN_KEEPALIVE int st_live_state(const uint8_t *st, uint32_t n, const uint8_t *scene)
{
    if (on_air)
        return -1;
    if (fst_live_state(&lv, st, n, scene))
        return -1;
    live_t0 = frs_ms();
    return 0;
}
EMSCRIPTEN_KEEPALIVE uint32_t st_now(void) { return frs_ms(); }
EMSCRIPTEN_KEEPALIVE uint32_t st_ms_after(uint32_t n) { return frs_ms_after(n); }
EMSCRIPTEN_KEEPALIVE int st_at(uint32_t ms, uint32_t op, const uint8_t *a, uint32_t n)
{
    if (on_air || op == 0u || op > 255u || n > 255u)
        return -1;
    return fst_live_at(&lv, ms, (uint8_t)op, a, n, 0);
}
EMSCRIPTEN_KEEPALIVE int st_at_scene(uint32_t ms, uint32_t op, const uint8_t *scene)
{
    if (on_air || op == 0u || op > 255u)
        return -1;
    return fst_live_at(&lv, ms, (uint8_t)op, 0, 0, scene);
}
EMSCRIPTEN_KEEPALIVE int st_at_state(uint32_t ms, const uint8_t *st, uint32_t n, const uint8_t *scene, int cmp)
{
    return on_air ? -1 : fst_live_at_state(&lv, ms, st, n, scene, cmp);
}
EMSCRIPTEN_KEEPALIVE uint32_t st_state_ms(void) { return on_air ? 0xFFFFFFFFu : fst_live_state_ms(&lv); }
EMSCRIPTEN_KEEPALIVE void st_cut(void)
{
    if (!on_air)
        fst_live_cut(&lv);
}
EMSCRIPTEN_KEEPALIVE void st_live_skip(uint32_t ms)
{
    if (!on_air && ms > frs_ms())
        frs_skip_to(ms);
}
EMSCRIPTEN_KEEPALIVE uint32_t st_live_save(uint8_t *out, uint32_t max, const uint8_t *scene)
{
    return frs_live_save(out, max, scene);
}
EMSCRIPTEN_KEEPALIVE uint32_t *st_live_more(void)
{
    more[0] = lv.late;
    more[1] = lv.loads;
    more[2] = lv.same;
    more[3] = lv.full;
    return more;
}
