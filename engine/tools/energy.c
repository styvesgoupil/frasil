/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* ENERGY, FRASIL STREAM's 0 .. 1000 (the admin plan's §7): a piece's brain run alone for its time on air (frs_skip_to,
 * nothing rendered), what it sends the sound counted by the DSP's FRASIL_EV_LOG hook:
 *   G  GLINT's strikes                          B  the bass's notes, retunes and pulses
 *   D  the drones' note-ons (chord changes)     H  a stream's gestures (its events: no MIDI clock, no note-off, a slider's
 *                                                  drag one gesture)
 * each per minute, a note its layer struck in its strike before (a repetition, a pulse) counted 1/2; a GLINT strike or a
 * bass note (or pulse) also as sharp as its layer's ATTACK is then, 1 / (1 + (ATTACK / 200 ms)^2): 1 to about 50 ms, a
 * half at 200 ms, near none from a second (a bowed GLINT swelling in, a bass fading up: heard as calm). BPM: frs_view's
 * tempo each second, averaged.
 *   changes = G + 0.8 B + 0.5 D;  raw = changes x BPM / 90 + 4 H;
 *   energy  = 1000 ln(1 + raw / r0) / ln(1 + r98 / r0), 0 .. 1000 (r0, r98: the catalogue's median and 98th percentile)
 * (Refit on the owner's listening, ROADMAP card #4: WHITEOUT above LUMIERE NOCTURNE, THE WITCHINGHOUR above LONG NIGHT;
 * NORTH RIVER and RESILIENCE close, BLACKHOLLOW just above WHITEOUT, THE WITCHINGHOUR above SUNRISE kept. Before it:
 * B 1.5, the tempo's square root, every strike 1.)
 * A piece: a scene (a shard: FST_SCENE_BYTES, "FRS~"'s, the scene then its LEAD; or an earlier layout frs_scene_over
 * knows, its LEAD MOSS: card #152), played alone as the radio plays it (programme.py's shard_stream: the scene, nothing
 * played on it), or a .frstream; "-" reads it from stdin.
 *   energy [--len MS] [--in MS] [--out MS] [--r0 X --r98 Y] [--w G,B,D,H] [--factory I] [--bins N] FILE|-
 *     --len  a shard's time on air (180000: SHARD LENGTH 3:00)      --in, --out  a stream's part on air (all of it)
 *     --r0, --r98  the calibration (energy-calibration.json's; built in below)   --w  the weights (1,0.8,0.5,4)
 *     --factory I  factory scene I as a shard (no FILE)
 *     --bins N  also "bins": what it played, N bins over [in, out): a GLINT strike or a bass note 1, a drone note-on 1.6 (a
 *               chord change, a taller mark), a repetition half (FRASIL STREAM's density strip for a shard: programme.py)
 * One JSON line on stdout. Exit 0; 1 the arguments; 2 not a piece (unreadable, damaged, a size it doesn't know); 3 a
 * stream of another scene layout (this engine can't play it); 4 the engine would not open.
 *   engine/tools/build.sh (cc, -DFRASIL_EV_LOG=energy_log, as engine/tests/run.sh builds skip) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../frasil_engine.h"
#include "../frasil_stream.h"

#define R0 141.22                         /* energy-calibration.json's (engine/tools/test.sh checks they agree) */
#define R98 455.81

/* engine/dsp/frasil.c's events (its enum, in that order) */
enum { EV_ON = 1, EV_SPON = 5, EV_BON = 8, EV_BPUL = 11, EV_BRET = 13 };

/* a layer's strikes: the notes of its strike before (a millisecond's), and the one now */
typedef struct { uint32_t prev[4], cur[4], ms; int any; double n; long raw; } layer_t;
static layer_t lay[3];                   /* 0 the drones, 1 GLINT, 2 the bass */
static int counting, bass_note;
#define MAXBINS 256
static double bins[MAXBINS];             /* (--bins: what it played, bin by bin) */
static int nbins;
static long bin_in, bin_len;

/* how sharp a strike starts: its layer's ATTACK now (engine/params.c's curves: GLINT 2 ms .. 1.5 s, the bass 5 ms .. 5 s).
 * The setting, not what is heard: in a stream's MORPH to a scene of another ATTACK, the one it goes to from the start (a
 * shard has no morph); an LFO on it not seen either. */
#define ONSET 0.2
static double sharp(uint32_t p)
{
    double u = (double)frs_param_get(p) / 127.0;
    double s = p == P_GL_ATTACK ? 0.002 * pow(750.0, u) : 0.005 * pow(1000.0, u);
    return 1.0 / (1.0 + (s / ONSET) * (s / ONSET));
}

static void strike(layer_t *l, int note, int repeat, double w)
{
    uint32_t ms = frs_ms(), b = (uint32_t)(note & 127);
    if (!l->any || ms != l->ms) {
        memcpy(l->prev, l->cur, sizeof l->cur);
        memset(l->cur, 0, sizeof l->cur);
        l->ms = ms;
        l->any = 1;
    }
    if (repeat < 0)
        repeat = (int)(l->prev[b >> 5] >> (b & 31u) & 1u);
    l->cur[b >> 5] |= 1u << (b & 31u);
    l->n += (repeat ? 0.5 : 1.0) * w;
    l->raw++;
    if (nbins && bin_len > 0) {
        long k = ((long)ms - bin_in) * nbins / bin_len;
        if (k >= 0 && k < nbins)
            bins[k] += (repeat ? 0.5 : 1.0) * (l == &lay[0] ? 1.6 : 1.0);
    }
}

void energy_log(uint8_t type, int note, float v);
void energy_log(uint8_t type, int note, float v)
{
    (void)v;
    if (!counting)
        return;
    if (type == EV_ON)
        strike(&lay[0], note, -1, 1.0);
    else if (type == EV_SPON)
        strike(&lay[1], note, -1, sharp(P_GL_ATTACK));
    else if (type == EV_BON)
        strike(&lay[2], bass_note = note, -1, sharp(P_BS_ATTACK));
    else if (type == EV_BRET)            /* (a legato step: the pitch moves, no attack) */
        strike(&lay[2], bass_note = note, -1, 1.0);
    else if (type == EV_BPUL)            /* (the note held, struck again) */
        strike(&lay[2], bass_note, 1, sharp(P_BS_ATTACK));
}

/* a stream's gestures in [in, out): a slider's (or a CC's) events less than GAP ms apart are one drag */
#define GAP 300u
static long gestures(const fst_file *f, uint32_t in, uint32_t out)
{
    fst_player p;
    fst_dec e;
    static uint32_t last[512];
    long n = 0;
    memset(last, 0xff, sizeof last);
    fst_begin(&p, f);
    while (fst_next(&p, out - 1u, &e)) {
        int k = -1, count = 1;
        switch (e.op) {
        case FST_PARAM: k = e.a[0]; break;
        case FST_MIDI:
            if (e.a[0] >= 0xF8u || (e.a[0] & 0xF0u) == 0x80u || ((e.a[0] & 0xF0u) == 0x90u && !e.a[2]))
                count = 0;               /* (clocks, Start, Stop; note-offs) */
            else if ((e.a[0] & 0xF0u) == 0xB0u)
                k = 256 + e.a[1];
            else if ((e.a[0] & 0xF0u) == 0xE0u || (e.a[0] & 0xF0u) == 0xD0u || (e.a[0] & 0xF0u) == 0xA0u)
                k = 384 + (e.a[0] >> 4 & 7);
            break;
        case FST_HOLD: count = e.a[2] != 0; break;
        case FST_KEY_UP: case FST_RECORDING: case FST_STATE: case FST_OPEN: case FST_STORE: case FST_CUT_STOP: count = 0; break;
        default: break;
        }
        if (k >= 0) {
            count = last[k] == 0xffffffffu || e.ms - last[k] > GAP;
            last[k] = e.ms;
        }
        if (count && e.ms >= in)
            n++;
    }
    return n;
}

static uint8_t *slurp(const char *path, size_t *n)
{
    FILE *fp = strcmp(path, "-") ? fopen(path, "rb") : stdin;
    size_t cap = 1u << 16, got = 0, r;
    uint8_t *b = malloc(cap);
    if (!fp || !b)
        return 0;
    while ((r = fread(b + got, 1, cap - got, fp)) > 0)
        if ((got += r) == cap && cap < (64u << 20))
            b = realloc(b, cap *= 2u);
    if (fp != stdin)
        fclose(fp);
    *n = got;
    return b;
}

static void put32(uint8_t *p, uint32_t x) { p[0] = (uint8_t)x; p[1] = (uint8_t)(x >> 8); p[2] = (uint8_t)(x >> 16); p[3] = (uint8_t)(x >> 24); }
static size_t chunk(uint8_t *o, const char *tag, const uint8_t *d, uint32_t n)
{
    memcpy(o, tag, 4);
    put32(o + 4, n);
    if (n)
        memcpy(o + 8, d, n);
    return 8u + n;
}

/* a shard alone on air, as a stream (tools/programme.py's shard_stream, byte for byte) */
static size_t shard_stream(const uint8_t *scene, uint32_t ms, uint8_t *o)
{
    uint8_t head[39], zero[64];
    size_t j = 8;
    memset(head, 0, sizeof head);
    memset(zero, 0, sizeof zero);
    put32(head, 44100u);
    put32(head + 4, ms);
    memcpy(head + 12, frs_version(), strlen(frs_version()) < 8 ? strlen(frs_version()) : 8);
    memcpy(head + 20, FST_SCENE_LAYOUT, 4);
    head[26] = 7;
    memcpy(o, "FSTR\x01\x01\x00\x00", 8);
    j += chunk(o + j, "HEAD", head, 39);
    j += chunk(o + j, "SCN0", scene, FST_SCENE_BYTES);
    j += chunk(o + j, "STAT", 0, 0);
    j += chunk(o + j, "MOVE", 0, 0);
    j += chunk(o + j, "DENS", zero, 64);
    j += chunk(o + j, "SCNS", zero, 2);
    j += chunk(o + j, "EVTS", 0, 0);
    memcpy(o + j, "CRC_", 4);
    put32(o + j + 4, 4);
    put32(o + j + 8, fst_crc(o, j));
    return j + 12u;
}

static void json_str(const char *s, size_t n)
{
    size_t i;
    putchar('"');
    for (i = 0; i < n && s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\')
            printf("\\%c", c);
        else if (c < 0x20 || c >= 0x7f)
            printf("\\u%04x", c);
        else
            putchar(c);
    }
    putchar('"');
}

static double arg_d(const char *s) { return strtod(s, 0); }

int main(int argc, char **argv)
{
    const char *path = 0;
    double r0 = R0, r98 = R98, wg = 1.0, wb = 0.8, wd = 0.5, wh = 4.0, bpm = 0.0, mins, G, B, D, H, changes, raw, energy;
    long len = 180000, in = 0, out = -1, factory = -1, h, secs = 0;
    int i, r, shard;
    size_t n = 0;
    uint8_t *b = 0, *piece, scene[FST_SCENE_BYTES];
    static uint8_t made[2048];
    static fst_file f;
    static fst_air air;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--len") && i + 1 < argc)
            len = atol(argv[++i]);
        else if (!strcmp(argv[i], "--in") && i + 1 < argc)
            in = atol(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc)
            out = atol(argv[++i]);
        else if (!strcmp(argv[i], "--r0") && i + 1 < argc)
            r0 = arg_d(argv[++i]);
        else if (!strcmp(argv[i], "--r98") && i + 1 < argc)
            r98 = arg_d(argv[++i]);
        else if (!strcmp(argv[i], "--bins") && i + 1 < argc)
            nbins = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--factory") && i + 1 < argc)
            factory = atol(argv[++i]);
        else if (!strcmp(argv[i], "--w") && i + 1 < argc) {
            if (sscanf(argv[++i], "%lf,%lf,%lf,%lf", &wg, &wb, &wd, &wh) != 4)
                return fprintf(stderr, "energy: --w G,B,D,H\n"), 1;
        } else if (argv[i][0] != '-' || !strcmp(argv[i], "-"))
            path = argv[i];
        else
            return fprintf(stderr, "energy: what is %s? (engine/tools/energy.c says)\n", argv[i]), 1;
    }
    if ((!path) == (factory < 0) || len < 1000 || len > 3600000 || in < 0 || r0 <= 0.0 || r98 <= r0 || nbins < 0 || nbins > MAXBINS)
        return fprintf(stderr, "energy [--len MS] [--in MS] [--out MS] [--r0 X --r98 Y] [--w G,B,D,H] [--factory I] [--bins N] FILE|-\n"), 1;
    if (frs_init(44100.0f))
        return 4;
    if (factory >= 0) {
        if (frs_scene_factory((uint32_t)factory, scene))
            return fprintf(stderr, "energy: no factory scene %ld\n", factory), 2;
        frs_lead_none(scene + FRS_SCENE_BYTES);
        shard = 1;
    } else {
        if (!(b = slurp(path, &n)) || !n)
            return fprintf(stderr, "energy: %s does not read\n", path), 2;
        shard = n < 4 || memcmp(b, "FSTR", 4);
        if (shard) {
            if (n > FRS_SCENE_BYTES && n != FST_SCENE_BYTES)
                return fprintf(stderr, "energy: not a scene or a stream (%zu bytes)\n", n), 2;
            memset(scene, 0, sizeof scene);
            memcpy(scene, b, n);
            if (n != FST_SCENE_BYTES && frs_scene_over(scene, (uint32_t)n))
                return fprintf(stderr, "energy: a scene of %zu bytes: a layout this engine doesn't know\n", n), 2;
            if (n != FST_SCENE_BYTES)
                frs_lead_none(scene + FRS_SCENE_BYTES);   /* (a scene alone: its LEAD MOSS) */
        }
    }
    if (shard) {
        n = shard_stream(scene, (uint32_t)len, made);
        piece = made;
        in = 0;
        out = len;
    } else {
        piece = b;
    }
    r = fst_read(piece, n, &f);
    if (r == FST_LAYOUT)
        return fprintf(stderr, "energy: a stream of scenes in %s, not %s\n", f.layout, FST_SCENE_LAYOUT), 3;
    if (r)
        return fprintf(stderr, "energy: a damaged stream (%d)\n", r), 2;
    if (out < 0 || out > (long)f.ms)
        out = (long)f.ms;
    if (out - in < 1000)
        return fprintf(stderr, "energy: under a second on air (in %ld, out %ld, the stream %u ms)\n", in, out, f.ms), 1;
    memset(lay, 0, sizeof lay);
    memset(bins, 0, sizeof bins);
    bin_in = in;
    bin_len = out - in;
    counting = in == 0;
    if (fst_air_open(&air, &f, 44100.0f))
        return 4;
    if (in)
        fst_air_skip(&air, (uint32_t)in);
    counting = 1;
    for (h = in; h < out;) {             /* a second at a time: the tempo seen each second */
        frs_view_t v;
        long to = h + 1000 < out ? h + 1000 : out;
        fst_air_skip(&air, (uint32_t)to);
        frs_view(&v);
        bpm += (double)v.tempo;
        secs++;
        h = to;
    }
    counting = 0;
    bpm /= (double)secs;
    mins = (double)(out - in) / 60000.0;
    G = lay[1].n / mins;
    B = lay[2].n / mins;
    D = lay[0].n / mins;
    H = shard ? 0.0 : (double)gestures(&f, (uint32_t)in, (uint32_t)out) / mins;
    changes = wg * G + wb * B + wd * D;
    raw = changes * (bpm > 1.0 ? bpm : 1.0) / 90.0 + wh * H;
    energy = 1000.0 * log1p(raw / r0) / log1p(r98 / r0);
    energy = energy < 0.0 ? 0.0 : energy > 1000.0 ? 1000.0 : energy;
    printf("{\"kind\":\"%s\",\"name\":", shard ? "shard" : "stream");
    json_str((const char *)f.scene, 16);
    printf(",\"engine\":");
    json_str(f.engine, 8);
    printf(",\"measured_on\":\"%s\",\"in\":%ld,\"out\":%ld,\"G\":%.2f,\"B\":%.2f,\"D\":%.2f,\"H\":%.2f,\"bpm\":%.1f,"
           "\"raw\":%.2f,\"energy\":%d,\"r0\":%g,\"r98\":%g,\"strikes\":{\"G\":%ld,\"B\":%ld,\"D\":%ld}}\n",
           frs_version(), in, out, G, B, D, H, bpm, raw, (int)lround(energy), r0, r98, lay[1].raw, lay[2].raw, lay[0].raw);
    if (nbins) {                         /* (the same line, "bins" after it: a second JSON line keeps the first as it was) */
        printf("{\"bins\":[");
        for (i = 0; i < nbins; i++)
            printf("%s%.1f", i ? "," : "", bins[i]);
        printf("]}\n");
    }
    free(b);
    frs_close();
    return 0;
}
