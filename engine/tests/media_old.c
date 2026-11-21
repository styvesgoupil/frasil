/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* PHONE (LOFI TAPE's fifth MEDIA, card #218) on the engines before it: three takes of the engine with it
 * (engine/tests/media.c `make`: PHONE in its start's scene and turned back to by an FST_PARAM, the same take on AM, on
 * TAPE), each read and heard by the engine this file is built on. run.sh builds it on two engines from git:
 *   media_old with DIR     (history/dev 49bcaa8, VINYL and AM but not PHONE): PHONE heard as AM, bit for bit (a value
 *                          past its range: its range's last);
 *   media_old before DIR   (70c3ad0, before the media): PHONE and AM heard as TAPE, bit for bit.
 * Every take read (none refused), every one played to its end. */
#include "frasil_engine.h"
#include "frasil_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float L[256], R[256];
static int fails;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "  ok" : "FAIL", what);
    fails += !ok;
}

/* a stream from the top to its end and 2 s more, as the radio plays it: its hash (0: not read) */
static uint64_t hear(const uint8_t *b, size_t n)
{
    static fst_file f;
    static fst_air a;
    uint64_t h = 0xcbf29ce484222325ull;
    uint32_t i, x;
    if (fst_read(b, n, &f) || fst_air_open(&a, &f, 44100.0f))
        return 0;
    while (fst_air_ms(&a) < f.ms + 2000u) {
        frs_render(L, R, 256u);
        fst_air_after(&a);
        for (i = 0; i < 256u; i++) {
            memcpy(&x, &L[i], 4);
            h = (h ^ x) * 1099511628211u;
            memcpy(&x, &R[i], 4);
            h = (h ^ x) * 1099511628211u;
        }
    }
    return h;
}

static uint64_t hear_file(const char *dir, const char *name)
{
    char path[1024];
    FILE *f;
    uint8_t *b;
    long sz;
    size_t n;
    uint64_t h;
    snprintf(path, sizeof path, "%s/%s.frstream", dir, name);
    f = fopen(path, "rb");
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(sz > 0 ? (size_t)sz : 1u);
    n = b && sz > 0 ? fread(b, 1, (size_t)sz, f) : 0u;
    fclose(f);
    h = n ? hear(b, n) : 0u;
    free(b);
    return h;
}

int main(int argc, char **argv)
{
    uint64_t ph, am, tp;
    int with;
    if (argc < 3 || (strcmp(argv[1], "with") && strcmp(argv[1], "before")))
        return printf("media_old with|before DIR\n"), 2;
    with = !strcmp(argv[1], "with");
    ph = hear_file(argv[2], "phone");
    am = hear_file(argv[2], "am");
    tp = hear_file(argv[2], "tape");
    frs_close();
    printf("       PHONE %016llx, AM %016llx, TAPE %016llx (%s)\n", (unsigned long long)ph, (unsigned long long)am,
           (unsigned long long)tp, with ? "an engine with VINYL and AM, before PHONE" : "an engine before the media");
    check(ph && am && tp, "the three takes read and played to their end");
    if (with)
        check(ph == am && ph != tp, "PHONE heard as AM, bit for bit (AM the last of its MEDIA)");
    else
        check(ph == tp && am == tp, "PHONE and AM heard as TAPE, bit for bit (TAPE its ON)");
    printf("media_old %s: %s\n", argv[1], fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
