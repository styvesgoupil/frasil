/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The calls frasil_engine.h says any thread may make (frs_param_count, _info, _kind, _text but the NOTEs and the drone's NOTES, frs_scene_new,
 * frs_scene_factory, frs_snapshot, frs_snapshot_fx, frs_fx_response, frs_lfo_target(s), frs_tracks_bits, frs_version), from a
 * UI's thread while the audio thread renders and plays (THE KEYS, the tracks, the settings, the scenes, the effects).
 * Built under ThreadSanitizer by engine/tests/run.sh: any race is its report.
 *   engine/tests/run.sh, or: threads */
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "frasil_engine.h"

static volatile int done;
static unsigned long reads;
static volatile float sink;            /* (the curves' sum: kept) */

static void *ui(void *arg)
{
    char t[FRS_TEXT];
    uint8_t sc[FRS_SCENE_BYTES];
    frs_view_t v;
    float live[FRS_LIVE], fx[FRS_LIVE_FX], db = 0.0f;
    const char *name;
    uint32_t max, def, p, n = frs_param_count();
    (void)arg;
    while (!done) {
        for (p = 0; p < n; p++) {        /* every value of every setting (but the two NOTEs: the audio thread's) */
            uint32_t x;
            if (frs_param_info(p, &name, &max, &def) || frs_param_kind(p) == FRS_P_NONE || !strcmp(name, "NOTE") ||
                !strcmp(name, "NOTES"))
                continue;
            for (x = 0; x <= max; x++)
                frs_param_text(p, x, t);
        }
        frs_scene_new(sc);
        frs_scene_factory(reads % 16u, sc);
        frs_snapshot(&v, live);
        if (!frs_snapshot_fx(fx))
            for (p = 0; p < 64u; p++)   /* (the effects' curves, for a screen) */
                db += frs_fx_response(fx, (int)(p & 1u), 20.0f * (float)(p + 1u) * 300.0f / 64.0f, 48000.0f);
        for (p = 0; p <= frs_lfo_targets(); p++)
            db += (float)frs_lfo_target(p) + (float)frs_tracks_bits(p % 7u);
        db += (float)frs_version()[0];
        reads++;
    }
    sink = db;
    return 0;
}

int main(void)
{
    static float l[512], r[512];
    uint8_t sc[FRS_SCENE_BYTES];
    pthread_t th;
    uint32_t k;
    char t[FRS_TEXT];
    frs_param_text(0, 0, t);             /* (before any open: the tables alone) */
    if (frs_init(48000.0f))
        return 1;
    pthread_create(&th, 0, ui, 0);
    for (k = 0; k < 3000u; k++) {        /* the audio thread: a minute's blocks, playing */
        if (k % 300u == 0u) {
            frs_scene_factory(k / 300u, sc);
            frs_scene_take(sc);
        }
        if (k % 40u == 5u)
            frs_latch(0, 48u + k % 24u, (int)(k % 5u) - 1);
        if (k % 90u == 7u)
            frs_track_play(k % 3u, (int)(k / 90u % 2u));
        if (k % 250u == 11u)
            frs_play_all((int)(k / 250u % 2u));
        if (k % 120u == 3u) {            /* (the effects in and out: the tape, the filter, the EQ) */
            frs_param_set(P_TP_ON, k / 120u % 2u);
            frs_param_set(P_FL_TYPE, k / 120u % 3u);
            frs_param_set(P_FL_CUT, 40u + k % 80u);
            frs_param_set(P_EQ_PRESET, k / 120u % 10u);
        }
        frs_render(l, r, 512);
    }
    done = 1;
    pthread_join(th, 0);
    frs_close();
    printf("threads: ok (%lu passes of the UI's calls beside the renders)\n", reads);
    return 0;
}
