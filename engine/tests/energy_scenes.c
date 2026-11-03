/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* ENERGY's test scenes (engine/tools/test.sh): factory scene 0 with the drones alone (GLINT and the bass on KEYS, nothing
 * latched on them, no SPARK, the drone's sequence off), and the same with LFO 1 fast and deep on the drone's FILTER (what
 * ENERGY doesn't count: it must read as calm as the other).
 *   energy_scenes OUT_DIR: OUT_DIR/drones.frscene, OUT_DIR/drones-lfo.frscene */
#include <stdio.h>
#include "../frasil_engine.h"
#include "../frasil_params.h"

static int put(const char *dir, const char *name)
{
    static uint8_t s[FRS_SCENE_BYTES], from[FRS_SCENE_BYTES];
    char path[512];
    FILE *f;
    frs_scene_factory(0, from);
    frs_scene_store(s, from);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(f = fopen(path, "wb")) || fwrite(s, 1, sizeof s, f) != sizeof s)
        return 1;
    return fclose(f) != 0;
}

int main(int argc, char **argv)
{
    static uint8_t s[FRS_SCENE_BYTES];
    if (argc != 2 || frs_init(44100.0f))
        return 1;
    frs_scene_factory(0, s);
    frs_scene_take(s);
    frs_param_set(P_SQ_TYPE, 0);
    frs_param_set(P_BQ_ON, 0);
    frs_param_set(P_DQ_ON, 0);
    frs_param_set(P_GX_SPARK, 0);
    frs_clear(1);
    frs_clear(2);
    if (put(argv[1], "drones.frscene"))
        return 1;
    frs_param_set(P_L1_SHAPE, 0);
    frs_param_set(P_L1_RATE, 60);          /* (free: some 6 Hz) */
    frs_param_set(P_L1_DEPTH, 127);
    frs_param_set(P_L1_T1, 9);             /* (the drone's FILTER) */
    return put(argv[1], "drones-lfo.frscene");
}
