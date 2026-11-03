/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's harmony: a key (ROOT, SCALE) and what a played note becomes (CHORD, VOICING).
 * A note outside the scale falls to the scale's note below it. The diatonic chords are steps up the scale from
 * that note (a TRIAD in D minor: D F A on D, E G Bb on E ...); the fixed ones are the same intervals on every note.
 * CHROMATIC with SINGLE: each key its own note, as if there were no harmony.
 * Part of the unity build (params.c includes it). */

#define MAXCHORD 4                       /* notes a chord at most */

static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* the scales: bit k = the pitch class k semitones above the root is in */
static const char *const SCALE_NAME[] = {"CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN",
                                         "LOCRIAN", "HARMONIC", "MELODIC", "PENTA MAJ", "PENTA MIN"};
static const uint16_t SCALE_BITS[] = {
    0xFFFu,
    1u << 0 | 1u << 2 | 1u << 4 | 1u << 5 | 1u << 7 | 1u << 9 | 1u << 11,   /* major */
    1u << 0 | 1u << 2 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 8 | 1u << 10,   /* natural minor */
    1u << 0 | 1u << 2 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 9 | 1u << 10,   /* dorian */
    1u << 0 | 1u << 1 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 8 | 1u << 10,   /* phrygian */
    1u << 0 | 1u << 2 | 1u << 4 | 1u << 6 | 1u << 7 | 1u << 9 | 1u << 11,   /* lydian */
    1u << 0 | 1u << 2 | 1u << 4 | 1u << 5 | 1u << 7 | 1u << 9 | 1u << 10,   /* mixolydian */
    1u << 0 | 1u << 1 | 1u << 3 | 1u << 5 | 1u << 6 | 1u << 8 | 1u << 10,   /* locrian */
    1u << 0 | 1u << 2 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 8 | 1u << 11,   /* harmonic minor */
    1u << 0 | 1u << 2 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 9 | 1u << 11,   /* melodic minor */
    1u << 0 | 1u << 2 | 1u << 4 | 1u << 7 | 1u << 9,                       /* major pentatonic */
    1u << 0 | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 10,                      /* minor pentatonic */
};
#define NSCALES (sizeof SCALE_BITS / sizeof SCALE_BITS[0])

/* the chords: the diatonic ones in scale steps, the fixed ones in semitones; -1 ends a chord */
enum { CH_DIATONIC = 8 };                /* the first 8 are diatonic */
static const char *const CHORD_NAME[] = {"SINGLE", "FIFTH", "TRIAD", "SEVENTH", "SUS2", "SUS4", "ADD9", "QUARTAL",
                                         "MAJ", "MIN", "MAJ7", "MIN7", "DOM7", "AUG", "DIM"};
static const int8_t CHORD_SHAPE[][MAXCHORD] = {
    {0, -1, -1, -1}, {0, 4, -1, -1}, {0, 2, 4, -1}, {0, 2, 4, 6}, {0, 1, 4, -1}, {0, 3, 4, -1}, {0, 2, 4, 8},
    {0, 3, 6, -1},                                       /* steps */
    {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 4, 7, 10}, {0, 4, 8, -1}, {0, 3, 6, -1},   /* semitones */
};
#define NCHORDS (sizeof CHORD_SHAPE / sizeof CHORD_SHAPE[0])

typedef struct {
    int8_t n[MAXCHORD];
    uint8_t k;                           /* how many */
} chord_t;

static int in_scale(int note, int root, uint16_t bits) { return bits >> (((note - root) % 12 + 12) % 12) & 1u; }

/* the scale's note `steps` steps above `note` (which is in the scale) */
static int scale_up(int note, int steps, int root, uint16_t bits)
{
    while (steps > 0) {
        note++;
        if (in_scale(note, root, bits))
            steps--;
    }
    return note;
}

/* what a played note becomes, with the harmony in v (the parameters); its root (the note it fell to) in *root */
static void chord_build(int played, const uint8_t *v, chord_t *c, int *root)
{
    int r = v[P_ROOT] % 12, sc = v[P_SCALE] % NSCALES, ch = v[P_CHORD] % NCHORDS, vo = v[P_VOICING] % 3, i, base;
    uint16_t bits = SCALE_BITS[sc];
    int out[MAXCHORD], k = 0;
    base = played;
    while (!in_scale(base, r, bits))     /* outside the scale: the scale's note below */
        base--;
    for (i = 0; i < MAXCHORD && CHORD_SHAPE[ch][i] >= 0; i++)
        out[k++] = ch < CH_DIATONIC ? scale_up(base, CHORD_SHAPE[ch][i], r, bits) : base + CHORD_SHAPE[ch][i];
    /* the voicing: OPEN lifts the second note an octave (a triad becomes root, fifth, tenth); WIDE drops the root
     * an octave under the rest */
    if (vo == 1 && k >= 3)
        out[1] += 12;
    else if (vo == 2 && k >= 2)
        out[0] -= 12;
    c->k = 0;
    for (i = 0; i < k; i++)
        if (out[i] >= 0 && out[i] <= 127)
            c->n[c->k++] = (int8_t)out[i];
    *root = base;
}

/* a note moved d steps along the scale (the scale's note at or below it moved, and what it was above that kept) */
static int scale_shift(int note, int d, const uint8_t *v)
{
    int r = v[P_ROOT] % 12, base = note;
    uint16_t bits = SCALE_BITS[v[P_SCALE] % NSCALES];
    while (!in_scale(base, r, bits))
        base--;
    note -= base;
    while (d > 0) {
        base++;
        d -= in_scale(base, r, bits);
    }
    while (d < 0) {
        base--;
        d += in_scale(base, r, bits);
    }
    note += base;
    return note < 1 ? 1 : note > 126 ? 126 : note;
}

/* how many steps of the scale from note a up to note b (down: below 0), each taken at the scale's note at or below it */
static int scale_steps(int a, int b, const uint8_t *v)
{
    int r = v[P_ROOT] % 12, d = 0, lo, hi;
    uint16_t bits = SCALE_BITS[v[P_SCALE] % NSCALES];
    while (!in_scale(a, r, bits))
        a--;
    while (!in_scale(b, r, bits))
        b--;
    lo = a < b ? a : b;
    hi = a < b ? b : a;
    while (lo < hi) {
        lo++;
        d += in_scale(lo, r, bits);
    }
    return a <= b ? d : -d;
}

/* a note into the scale: the scale's note at or below it (GLINT plays single notes) */
static int scale_snap(int note, const uint8_t *v)
{
    int r = v[P_ROOT] % 12;
    uint16_t bits = SCALE_BITS[v[P_SCALE] % NSCALES];
    while (!in_scale(note, r, bits))
        note--;
    return note;
}
