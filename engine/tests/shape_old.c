/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* ATTACK SHAPE's takes (card #122) on an engine of before it: engine/tests/line-streams/'s, made with LINE on the
 * engine of card #122 (engine/tests/shape.c `make`), each read and heard by the engine this file is built on, its hash
 * hashes.txt's CURVE one (the second). Built by engine/tests/run.sh on history/dev ef2cbb5's engine (git archive, with
 * the LEAD as STREAM's engines are): the state's pair it does not know left, every take read, its shapes CURVE.
 *   shape_old DIR                    each take of DIR/hashes.txt checked
 *   shape_old hash FILE...           each one's hash on this engine: NAME HASH */
#include "frasil_engine.h"
#include "frasil_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float L[256], R[256];

static uint64_t hear(const uint8_t *b, size_t n, uint32_t *nparam)
{
    static fst_file f;
    static fst_air a;
    uint64_t h = 0xcbf29ce484222325ull;
    uint32_t i, x;
    if (fst_read(b, n, &f) || fst_air_open(&a, &f, 44100.0f))
        return 0;
    if (nparam)
        *nparam = f.nparam;
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

static uint8_t *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long sz;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(sz > 0 ? (size_t)sz : 1u);
    *n = b && sz > 0 ? fread(b, 1, (size_t)sz, f) : 0u;
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    char path[1024], line[256], name[128];
    unsigned long long hl, hc;
    int oks = 0, fails = 0, i;
    FILE *f;
    if (argc >= 2 && !strcmp(argv[1], "hash")) {
        for (i = 2; i < argc; i++) {
            size_t n;
            uint8_t *b = slurp(argv[i], &n);
            printf("%s %016llx\n", argv[i], (unsigned long long)(b ? hear(b, n, 0) : 0u));
            free(b);
        }
        return 0;
    }
    if (argc < 2)
        return printf("shape_old DIR | hash FILE...\n"), 2;
    snprintf(path, sizeof path, "%s/hashes.txt", argv[1]);
    f = fopen(path, "r");
    if (!f)
        return printf("FAIL %s not read\nshape-old: 0 ok, 1 FAIL\n", path), 1;
    while (fgets(line, sizeof line, f)) {
        size_t n;
        uint8_t *b;
        uint32_t np = 0;
        uint64_t h;
        if (line[0] == '#' || sscanf(line, "%127s %llx %llx", name, &hl, &hc) != 3)
            continue;
        snprintf(path, sizeof path, "%s/%s", argv[1], name);
        b = slurp(path, &n);
        h = b ? hear(b, n, &np) : 0u;
        if (h && h == hc && np == frs_param_count())
            oks++;
        else {
            fails++;
            printf("FAIL %s on %s's engine: %s, heard %016llx (CURVE %016llx)\n", name, FRS_VERSION,
                   h ? "read" : "not read", (unsigned long long)h, hc);
        }
        free(b);
    }
    fclose(f);
    printf("shape-old: %d ok, %d FAIL (each take read and heard CURVE on the engine of before card #122)\n", oks, fails);
    return fails != 0 || oks != 2;
}
