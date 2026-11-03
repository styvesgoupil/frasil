/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The engine alone (frasil_engine.c), driven by a script of the FM-1's simulator (its commands without the FM-1's
 * panel: master, wait, midi, clock, param, wav, wavstop, renderhash, echo), its milliseconds and blocks as the simulator
 * runs them: at 44.1 kHz, the same samples as the FM-1's (each renderhash the simulator's). A Program Change takes the
 * slot's scene as an FM-1 with nothing saved has it (factory_get). At another rate, the same script (its WAV, to set
 * beside 44.1 kHz's: engine/tests/rates.py).
 *   engine/tests/run.sh, or: alone SCRIPT [RATE] [OUT.wav] */
#include <stdio.h>
#include <stdlib.h>
#include "../frasil_engine.c"

static uint64_t hash = 0xCBF29CE484222325u, frames;
static FILE *wav;
static uint32_t wav_frames;

static void wav_open(const char *path, uint32_t fs)
{
    static const uint8_t h[44] = {0};
    (void)fs;
    wav = fopen(path, "wb");
    if (wav)
        fwrite(h, 1, 44, wav);
    wav_frames = 0;
}

static void wav_close(uint32_t fs)
{
    uint32_t v, bytes = wav_frames * 4u;
    uint16_t fmt[2] = {1, 2}, al[2] = {4, 16};
    if (!wav)
        return;
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav);
    v = 36u + bytes;
    fwrite(&v, 4, 1, wav);
    fwrite("WAVEfmt ", 1, 8, wav);
    v = 16;
    fwrite(&v, 4, 1, wav);
    fwrite(fmt, 2, 2, wav);
    v = fs;
    fwrite(&v, 4, 1, wav);
    v = fs * 4u;
    fwrite(&v, 4, 1, wav);
    fwrite(al, 2, 2, wav);
    fwrite("data", 1, 4, wav);
    fwrite(&bytes, 4, 1, wav);
    fclose(wav);
    wav = 0;
}

/* a block out: hashed as app/app.c app_render hashes it, into the WAV as the simulator writes it */
static void out_block(const int32_t *lr)
{
    uint32_t i;
    for (i = 0; i < 2u * FRS_BLOCK; i++) {
        hash ^= (uint32_t)lr[i];
        hash *= 0x100000001B3u;
    }
    frames += FRS_BLOCK;
    for (i = 0; i < FRS_BLOCK; i++) {
        float l = (float)lr[2u * i] / 8388608.0f, r = (float)lr[2u * i + 1u] / 8388608.0f;
        int16_t s[2];
        s[0] = (int16_t)(l * 32767.0f);
        s[1] = (int16_t)(r * 32767.0f);
        if (wav) {
            fwrite(s, 2, 2, wav);
            wav_frames++;
        }
    }
}

/* n milliseconds, as the simulator's run_ms: at each, the blocks due, then the engine's tick */
static void run_ms(uint32_t n)
{
    while (n--) {
        while (due_ms <= (double)now64) {
            frasil_render(blk, FRS_BLOCK, out_gain);
            due_ms += block_ms;
            out_block(blk);
        }
        eng_tick();
        now64++;
    }
}

static void program(void *user, uint32_t n)   /* (as the FM-1's scene_go with nothing saved: the slot's factory scene) */
{
    scene_t sc;
    (void)user;
    if (n >= NSCENES)
        return;
    factory_get(n, &sc);
    taken_sc = sc;
    eng_scene_go(&sc);
}

static void say(void *user, const char *a, const char *b)
{
    (void)user;
    (void)a;
    (void)b;
}

int main(int argc, char **argv)
{
    char line[512], cmd[32], a[256], b[64], c[64], d[64];
    float fs = argc > 2 ? (float)atof(argv[2]) : 44100.0f;
    FILE *sc;
    if (argc < 2 || !(sc = fopen(argv[1], "r"))) {
        fprintf(stderr, "alone SCRIPT [RATE] [OUT.wav]\n");
        return 2;
    }
    if (frs_init(fs)) {
        fprintf(stderr, "alone: no engine at %g Hz\n", (double)fs);
        return 2;
    }
    frs_callbacks(say, program, 0);
    out_gain = (float)(2048u * 2048u >> 12) * (1.0f / 4096.0f);   /* (the simulator's MASTER at its start) */
    while (fgets(line, sizeof line, sc)) {
        memset(cmd, 0, sizeof cmd);
        a[0] = b[0] = c[0] = d[0] = 0;
        if (line[0] == '#' || sscanf(line, "%31s %255s %63s %63s %63s", cmd, a, b, c, d) < 1)
            continue;
        if (!strcmp(cmd, "wait")) {
            run_ms((uint32_t)atoi(a));
        } else if (!strcmp(cmd, "master")) {   /* (its gain as the simulator's: MASTER's square) */
            uint32_t m = (uint32_t)atoi(a);
            out_gain = (float)(m * m >> 12) * (1.0f / 4096.0f);
            run_ms(40);
        } else if (!strcmp(cmd, "midi")) {   /* (then 2 ms, as the simulator runs them) */
            frs_midi((uint8_t)strtoul(a, 0, 16), (uint8_t)strtoul(b, 0, 16), (uint8_t)strtoul(c, 0, 16));
            run_ms(2);
        } else if (!strcmp(cmd, "clock")) {
            int n = atoi(a), gap = b[0] ? atoi(b) : 21;
            for (; n > 0; n--) {
                frs_midi(0xF8, 0, 0);
                run_ms((uint32_t)gap);
            }
        } else if (!strcmp(cmd, "param")) {
            param_set(atoi(a) % NPARAM, atoi(b));
        } else if (!strcmp(cmd, "renderhash")) {
            printf("  renderhash %016llx %llu frames\n", (unsigned long long)hash, (unsigned long long)frames);
        } else if (!strcmp(cmd, "wav")) {
            if (argc > 3)
                wav_open(argv[3], (uint32_t)fs);
        } else if (!strcmp(cmd, "wavstop")) {
            wav_close((uint32_t)fs);
        } else if (!strcmp(cmd, "echo")) {
            printf("%s", line + 5);
        } else {
            printf("line: unknown command %s\n", cmd);
            return 2;
        }
    }
    wav_close((uint32_t)fs);
    return 0;
}
