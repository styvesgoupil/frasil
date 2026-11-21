/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* LEAD CH's (card #228) sound with nothing on the LEAD: the same calls on this engine and on 3.4's (engine/tests/ldch.c,
 * ldch_old.c, built on c5398e68's by run.sh), their samples hashed. The factory scene 2; MIDI on the tracks' channels (1,
 * 2, 3: the drones, GLINT, the bass) and on 4 .. 16, nobody's (LEAD CH OFF at the start, as 3.4 has none: notes,
 * pressure, the wheels, CC 123); the keys on the drones then on the LEAD (SELECT's P_KB 3), its own notes */
#ifndef LDCH_SOUND_H
#define LDCH_SOUND_H
#include <string.h>

static float lds_l[256], lds_r[256];
static uint64_t ldch_sound(void)
{
    static uint8_t sc[FRS_SCENE_BYTES];
    uint64_t h = 1469598103934665603u;
    uint32_t b, i, x, c;
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_scene_factory(2, sc);
    frs_scene_take(sc);
    for (b = 0; b < 1200u; b++) {        /* (7 s: 1200 blocks of 256) */
        if (b == 10u)
            frs_midi(0x90, 48, 100);     /* (the drones) */
        if (b == 40u)
            frs_midi(0x91, 72, 90);      /* (GLINT) */
        if (b == 70u)
            frs_midi(0x92, 36, 110);     /* (the bass) */
        if (b == 100u)
            for (c = 3u; c < 16u; c++) { /* (4 .. 16: nobody's) */
                frs_midi((uint8_t)(0x90u | c), (uint8_t)(50u + c), 100);
                frs_midi((uint8_t)(0xD0u | c), 90, 0);
                frs_midi((uint8_t)(0xE0u | c), 0, 0x60);
                frs_midi((uint8_t)(0xB0u | c), 1, 127);
            }
        if (b == 200u) {
            frs_key_down(5, 57, 90, 0);
            frs_key_down(9, 60, 80, 0);
        }
        if (b == 300u) {
            frs_key_up(5);
            frs_key_up(9);
            frs_param_set(P_KB, 3);      /* (the keys on the LEAD) */
            frs_key_down(7, 62, 100, 0);
        }
        if (b == 360u)
            frs_key_down(11, 66, 70, 0);
        if (b == 420u) {
            frs_key_up(11);
            frs_key_pressure(7, 100);
        }
        if (b == 500u) {
            frs_key_up(7);
            frs_midi(0x81, 72, 0);
            frs_midi(0x92, 41, 100);
            for (c = 3u; c < 16u; c++) {
                frs_midi((uint8_t)(0x80u | c), (uint8_t)(50u + c), 0);
                frs_midi((uint8_t)(0xB0u | c), 123, 0);
            }
        }
        if (b == 700u) {
            frs_midi(0x80, 48, 0);
            frs_midi(0x82, 36, 0);
            frs_midi(0x82, 41, 0);
        }
        frs_render(lds_l, lds_r, 256);
        for (i = 0; i < 256u; i++) {
            memcpy(&x, &lds_l[i], 4);
            h = (h ^ x) * 1099511628211u;
            memcpy(&x, &lds_r[i], 4);
            h = (h ^ x) * 1099511628211u;
        }
    }
    return h;
}
#endif
