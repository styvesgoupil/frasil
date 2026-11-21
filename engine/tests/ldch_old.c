/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* LEAD CH (card #228) on 3.4's engine (c5398e68's, built from git by run.sh), what this one made (ldch make DIR):
 *   the state as at the start: its own, byte for byte; the state on 9 with CHORD (pair 250, not known there) read, its
 *   own then still as at the start (nothing else moved);
 *   the sound with nothing on the LEAD (ldch_sound.h: channels 4 .. 16 nobody's): bit for bit this engine's;
 *   the take with LEAD CH (its start on 9, FST_STATE's pair): read, opened, played to its end.
 *   ldch_old make DIR: a take of 3.4's with notes on channel 4 (GLINT CH 4 in its start, the LEAD's HONEY in its scene;
 *   notes, pressure, the wheels, CC 123 on 4, the drones on 1), journaled and played on air here: the take and its
 *   sound hashed into DIR (take-3.4.frstream, take-3.4.txt), for ldch make DIR to hear it the same, bit for bit.
 *   ldch_old [make] DIR */
#include "frasil_engine.h"
#include "frasil_stream.h"
#include "ldch_sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, oks;
static void check(int ok, const char *what)
{
    if (ok)
        oks++;
    else {
        fails++;
        printf("FAIL %s\n", what);
    }
}

static size_t get(const char *dir, const char *name, uint8_t *b, size_t max)
{
    char p[1024];
    FILE *f;
    size_t n;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "rb");
    if (!f)
        return 0;
    n = fread(b, 1, max, f);
    fclose(f);
    return n;
}

static void fresh(void)
{
    static uint8_t sc[FRS_SCENE_BYTES];
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_scene_factory(2, sc);
    frs_scene_take(sc);
}

/* 3.4's take with notes on channel 4: GLINT's there (GLINT CH 4), the LEAD's on none (3.4 has no LEAD CH) */
#define T34_END 4000u
static const struct {
    uint32_t ms;
    uint8_t s, d1, d2;
} T34[] = {
    {100, 0x90, 48, 100}, {300, 0x93, 60, 100}, {600, 0x93, 67, 90}, {900, 0xD3, 80, 0},   {1000, 0xE3, 0, 0x60},
    {1100, 0xB3, 1, 127}, {1500, 0x83, 67, 0},  {1800, 0x83, 60, 0}, {2000, 0x93, 64, 110}, {2300, 0xA3, 64, 90},
    {2600, 0xB3, 123, 0}, {2700, 0x83, 64, 0},  {3000, 0x80, 48, 0},
};
static int put(const char *dir, const char *name, const void *b, size_t n)
{
    char p[1024];
    FILE *f;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "wb");
    if (!f || fwrite(b, 1, n, f) != n) {
        if (f)
            fclose(f);
        return -1;
    }
    return fclose(f);
}
static int make(const char *dir)
{
    static uint8_t sc[FRS_SCENE_BYTES], jb[65536];
    static fst_start st0;
    static fst_file file;
    static fst_air air;
    uint64_t h = 1469598103934665603u;
    uint32_t last = 0, k, i, x;
    size_t jn, n = 0;
    uint8_t *s;
    fst_ev e;
    float l[256], r[256];
    char t[32];
    int rc;
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_scene_factory(1, sc);
    frs_scene_take(sc);
    frs_param_set(P_LD_PRESET, 9);       /* (HONEY: the LEAD heard, were anything on it) */
    frs_param_set(P_CH_GLINT, 4);
    fst_capture(&st0, 0);
    jn = fst_jhead(&st0, 44100, 1793000000, jb);
    for (k = 0; k < sizeof T34 / sizeof T34[0]; k++) {
        memset(&e, 0, sizeof e);
        e.ms = T34[k].ms;
        e.op = FST_MIDI;
        e.n = 4;
        e.a[0] = T34[k].s;
        e.a[1] = T34[k].d1;
        e.a[2] = T34[k].d2;
        jn += fst_jevent(&e, &last, 0, 0, jb + jn);
    }
    memset(&e, 0, sizeof e);
    e.ms = T34_END;
    e.op = FSTJ_END;
    jn += fst_jevent(&e, &last, 0, 0, jb + jn);
    s = fst_finish(jb, jn, &n);
    check(s && !fst_read(s, n, &file) && !fst_air_open(&air, &file, 44100.0f), "3.4's take, notes on channel 4: "
                                                                                "journaled, read, opened");
    while (s && fst_air_ms(&air) < T34_END) {
        frs_render(l, r, 256);
        fst_air_after(&air);
        for (i = 0; i < 256u; i++) {
            memcpy(&x, &l[i], 4);
            h = (h ^ x) * 1099511628211u;
            memcpy(&x, &r[i], 4);
            h = (h ^ x) * 1099511628211u;
        }
    }
    snprintf(t, sizeof t, "%016llx\n", (unsigned long long)h);
    rc = s ? put(dir, "take-3.4.frstream", s, n) | put(dir, "take-3.4.txt", t, strlen(t)) : -1;
    check(!rc, "... played on air here, it and its sound written");
    printf("ldch_old: 3.4's take made in %s, its sound %s", dir, t);
    free(s);
    printf("ldch_old make: %d ok, %d FAIL\n", oks, fails);
    return fails ? 1 : 0;
}

int main(int argc, char **argv)
{
    static uint8_t def[FRS_STATE_BYTES], s9[FRS_STATE_BYTES], own[FRS_STATE_BYTES], t[32], *take;
    static fst_file file;
    static fst_air air;
    size_t dn, n9, tn;
    uint32_t on;
    float l[256], r[256];
    char want[32];
    if (argc == 3 && !strcmp(argv[1], "make"))
        return make(argv[2]);
    if (argc != 2) {
        printf("ldch_old [make] DIR\n");
        return 2;
    }
    dn = get(argv[1], "state-def.bin", def, sizeof def);
    n9 = get(argv[1], "state-9c.bin", s9, sizeof s9);
    fresh();
    on = frs_state_save(own);
    check(dn && on == dn && !memcmp(own, def, dn), "the state as at the start: 3.4's own, byte for byte");
    check(n9 == dn + 2u && !frs_state_load(s9, (uint32_t)n9), "the state on 9 with CHORD: read");
    on = frs_state_save(own);
    check(on == dn && !memcmp(own, def, dn), "... its own then as at the start (pair 250 not taken, nothing moved)");
    memset(t, 0, sizeof t);
    get(argv[1], "sound.txt", t, sizeof t - 1u);
    snprintf(want, sizeof want, "%016llx\n", (unsigned long long)ldch_sound());
    check(t[0] && !strcmp((const char *)t, want), "nothing on the LEAD: the sound bit for bit this engine's");
    if (strcmp((const char *)t, want))
        printf("ldch_old: the sound %s, made %s", want, (const char *)t);
    take = malloc(1 << 20);
    tn = take ? get(argv[1], "take.frstream", take, 1 << 20) : 0u;
    check(tn && !fst_read(take, tn, &file) && !fst_air_open(&air, &file, 44100.0f), "the take with LEAD CH: read, opened");
    while (tn && fst_air_ms(&air) < file.ms + 500u) {
        frs_render(l, r, 256);
        fst_air_after(&air);
    }
    check(tn && fst_air_ms(&air) >= file.ms, "... played to its end");
    free(take);
    printf("ldch_old: %d ok, %d FAIL\n", oks, fails);
    return fails ? 1 : 0;
}
