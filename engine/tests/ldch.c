/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* LEAD CH (card #228), on the engine with the LEAD (FRS_LEAD, dsp/lead.c), as the FM-1 and stream.wasm build it:
 *   at the start OFF, no chords, channel 4's notes nobody's (as 3.4); frs_lead_midi_set, _get; the state's pair (250:
 *   the channel, 0x20 CHORD) only while not OFF (OFF, its chords or not: the state byte for byte as before it), a
 *   state without it OFF, past OMNI OFF; frs_panic.
 *   The LEAD on its channel: MONO's last on top and back to the one held (legato), POLY a voice a note, on CHORD (POLY)
 *   HARMONY's chord on each, MONO the note as it came; velocity, pressure (a key's, the channel's), the wheels; CC 123
 *   on it, CC 120 on any; the channel turned and MODE turned with notes held: none left hanging; OFF nothing, OMNI any.
 *   A take with it (its start on 9, FST_MIDI on it, FST_STATE's pair: CHORD, then channel 3, the bass's too) journaled
 *   and played on air: twice the same, every sample the take live op by op; the control without the pair, another.
 *   The running state with MIDI's chord held on the LEAD: a listener opened on it the same brain, the chord let go by
 *   its MIDI off there too.
 *   ldch make DIR: what engine/tests/ldch_old (3.4's engine, c5398e68) reads: the state as at the start and on 9 with
 *   CHORD, the take, the sound with nothing on the LEAD (ldch_sound.h) hashed; and 3.4's take with notes on channel 4
 *   (ldch_old make DIR's, when there) heard here bit for bit as 3.4 played it, with LEAD CH 4 another sound.
 *   engine/tests/run.sh, or: ldch [make DIR] */
#include "../frasil_engine.c"
#include "../frasil_stream.h"
#include "ldch_sound.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if !FRS_LEAD
#error "LEAD CH's test: built with -DFRS_LEAD=1 and dsp/lead.c (engine/tests/run.sh)"
#endif

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

static float L[256], R[256];
static uint64_t hash;
static void hash_block(uint32_t n)
{
    uint32_t i, x;
    for (i = 0; i < n; i++) {
        memcpy(&x, &L[i], 4);
        hash = (hash ^ x) * 1099511628211u;
        memcpy(&x, &R[i], 4);
        hash = (hash ^ x) * 1099511628211u;
    }
}
static void blocks(uint32_t n)
{
    while (n--)
        frs_render(L, R, 256);
}
static lead_view_t view(void)
{
    lead_view_t v;
    frasil_lead_view(&v);
    return v;
}
static uint32_t ldmidi(void)             /* MIDI's entries on the LEAD (a chord's each) */
{
    uint32_t i, n = 0;
    for (i = 0; i < ldk.n; i++)
        n += ldk.k[i] >= LDK_MIDI;
    return n;
}
static int pair_of(const uint8_t *s, uint32_t n, uint32_t p)   /* a state's pair p: its value, -1 none */
{
    uint32_t i;
    for (i = 5; i + 1u < n; i += 2u)
        if (s[i] == p)
            return s[i + 1u];
    return -1;
}
static void fresh(void)
{
    uint8_t sc[FRS_SCENE_BYTES];
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_scene_factory(2, sc);
    frs_scene_take(sc);
}

/* ------------------------------------------------------------------ the API, the state */
static void api(void)
{
    uint8_t s0[FRS_STATE_BYTES], s1[FRS_STATE_BYTES], s2[FRS_STATE_BYTES];
    uint32_t n0, n1, n2;
    fresh();
    check(frs_lead_midi_get() == FRS_LDCH_DEF && st.ldch == 0u && !st.ldchord, "at the start: LEAD CH OFF, no chords");
    n0 = frs_state_save(s0);
    check(pair_of(s0, n0, STATE_LDCH) < 0, "the state as at the start: no pair 250");
    frs_lead_midi_set(9, 1);
    check(frs_lead_midi_get() == (9u | FRS_LDCH_CHORD), "set 9, CHORD: get 9 | 0x20");
    n1 = frs_state_save(s1);
    check(n1 == n0 + 2u && pair_of(s1, n1, STATE_LDCH) == 0x29 && !memcmp(s0, s1, n0),
          "the state on 9 with CHORD: the same bytes, pair 250 (0x29) after them");
    frs_lead_midi_set(0, 0);
    n2 = frs_state_save(s2);
    check(n2 == n0 && !memcmp(s0, s2, n0), "back to OFF, no chords: the state byte for byte as at the start");
    frs_lead_midi_set(0, 1);
    n2 = frs_state_save(s2);
    check(frs_lead_midi_get() == FRS_LDCH_CHORD && n2 == n0 && !memcmp(s0, s2, n0),
          "OFF with CHORD (no MIDI note comes): no pair, the state byte for byte as at the start");
    frs_lead_midi_set(4, 0);
    check(pair_of(s2, frs_state_save(s2), STATE_LDCH) == 4, "4: the pair (4)");
    frs_lead_midi_set(17, 0);
    check(pair_of(s2, frs_state_save(s2), STATE_LDCH) == 17, "OMNI: the pair (17)");
    frs_lead_midi_set(18, 1);
    check(frs_lead_midi_get() == FRS_LDCH_CHORD, "past OMNI: OFF");
    check(!frs_state_load(s1, n1) && frs_lead_midi_get() == (9u | FRS_LDCH_CHORD), "a state with the pair: 9, CHORD");
    check(!frs_state_load(s0, n0) && frs_lead_midi_get() == FRS_LDCH_DEF, "a state without it: OFF, no chords");
    s1[n1 - 1u] = 30;
    check(!frs_state_load(s1, n1) && frs_lead_midi_get() == FRS_LDCH_DEF, "a pair past OMNI: OFF");
}

/* ------------------------------------------------------------------ MIDI on the LEAD */
static void midi(void)
{
    lead_view_t v;
    uint32_t g;
    fresh();
    frs_param_set(P_LD_MODE, 0);
    /* OFF at the start: channel 4's notes and wheels nobody's, as 3.4's */
    frs_midi(0x93, 60, 80);
    frs_midi(0xE3, 0, 0x60);
    frs_midi(0xB3, 1, 127);
    blocks(4);
    check(!view().gate && !ldmidi() && view().bend == 0.0f && view().mod == 0.0f,
          "OFF at the start: a note and the wheels on channel 4 not the LEAD's");
    frs_midi(0x83, 60, 0);
    frs_midi(0xE3, 0, 0x40);
    frs_midi(0xB3, 1, 0);
    frs_lead_midi_set(4, 0);
    /* on 4, MONO: the last on top, back to the one held (legato), velocity, pressure, the wheels */
    frs_midi(0x93, 60, 80);
    blocks(4);
    v = view();
    check(v.gate && ldmidi() == 1u && ldk.note[0] == 60 && fabsf(v.vel - 80.0f / 127.0f) < 1e-6f,
          "MONO: a note on channel 4 the LEAD's, as it came, its velocity");
    g = v.glides;
    frs_midi(0x93, 64, 127);
    blocks(4);
    check(view().note == v.note + 4 && view().glides == g + 1u, "MONO: the next on top, gliding there");
    frs_midi(0xD3, 64, 0);
    blocks(2);
    check(fabsf(view().prs - 64.0f / 127.0f) < 1e-6f, "channel pressure: the LEAD's PRESSURE");
    frs_midi(0xA3, 64, 127);
    blocks(2);
    check(view().prs == 1.0f, "a key's pressure on its note: the same");
    frs_midi(0xE3, 0, 0x60);
    frs_midi(0xB3, 1, 127);
    blocks(2);
    check(view().bend == 1.0f && view().mod == 1.0f, "the pitch wheel (+4096: a semitone) and the mod wheel");
    frs_midi(0xE3, 0, 0x40);
    frs_midi(0xB3, 1, 0);
    frs_midi(0x83, 64, 0);
    blocks(4);
    check(view().note == v.note && view().gate && view().glides == g + 2u, "the top let go: back to the one held");
    frs_midi(0x93, 60, 100);
    blocks(2);
    check(ldmidi() == 1u, "struck again without its off: one note");
    frs_midi(0x83, 60, 0);
    blocks(2);
    check(ldmidi() == 0u && !view().gate, "its one off lets it go");
    /* CC 123 on it, not on another; CC 120 on any; frs_panic */
    frs_midi(0x93, 60, 100);
    frs_midi(0x93, 64, 100);
    frs_midi(0xB0, 123, 0);
    blocks(2);
    check(ldmidi() == 2u && view().gate, "CC 123 on another channel: the LEAD's notes held");
    frs_midi(0xB3, 123, 0);
    blocks(2);
    check(ldmidi() == 0u && !view().gate, "CC 123 on LEAD CH: let go");
    frs_midi(0x93, 60, 100);
    frs_midi(0xBA, 120, 0);
    blocks(2);
    check(ldmidi() == 0u && !view().gate, "CC 120 on any: let go");
    frs_midi(0x93, 60, 100);
    frs_panic();
    blocks(2);
    check(ldmidi() == 0u && !view().held, "frs_panic: let go");
    /* the channel turned with a note held: let go, its off on the old channel nothing; OFF; OMNI */
    frs_midi(0x93, 60, 100);
    blocks(2);
    frs_lead_midi_set(5, 0);
    blocks(2);
    check(ldmidi() == 0u && !view().gate, "LEAD CH turned with a note held: let go");
    frs_midi(0x83, 60, 0);
    frs_midi(0x94, 62, 100);
    blocks(2);
    check(ldmidi() == 1u && view().gate, "on 5: channel 5's");
    frs_midi(0x84, 62, 0);
    frs_lead_midi_set(0, 0);
    frs_midi(0x93, 60, 100);
    frs_midi(0x94, 60, 100);
    blocks(2);
    check(ldmidi() == 0u && !view().gate, "OFF: none");
    frs_lead_midi_set(17, 0);
    frs_midi(0x9F, 60, 100);
    blocks(2);
    check(ldmidi() == 1u && view().gate, "OMNI: channel 16's too");
    frs_midi(0x8F, 60, 0);
    frs_lead_midi_set(4, 0);
    /* MODE turned with notes held: every voice let go, the notes kept; their offs leave none */
    frs_midi(0x93, 60, 100);
    blocks(2);
    frs_param_set(P_LD_MODE, 1);
    blocks(2);
    check(!view().held && ldmidi() == 1u, "MONO to POLY with a note held: its voice let go, the note kept");
    frs_midi(0x83, 60, 0);
    frs_midi(0x93, 60, 100);
    frs_midi(0x93, 67, 100);
    blocks(2);
    check(view().held == 2u, "POLY: a voice a note");
    frs_param_set(P_LD_MODE, 0);
    blocks(2);
    frs_midi(0x83, 67, 0);
    blocks(2);
    check(view().held == 1u && ldmidi() == 1u, "POLY to MONO: the top let go, back to the one held");
    frs_midi(0x83, 60, 0);
    blocks(2);
    check(!view().held && !ldmidi(), "none hanging");
    /* CHORD: on POLY HARMONY's chord on each note (C MAJOR, TRIAD, CLOSE: C#4 falls to C4, C E G); MONO the note */
    frs_param_set(P_ROOT, 0);
    frs_param_set(P_SCALE, 1);
    frs_param_set(P_CHORD, 2);
    frs_param_set(P_VOICING, 0);
    frs_param_set(P_LD_MODE, 1);
    frs_lead_midi_set(4, 1);
    frs_midi(0x93, 61, 100);
    blocks(2);
    check(view().held == 3u && ldmidi() == 3u && ldk.note[0] == 60 && ldk.note[1] == 64 && ldk.note[2] == 67,
          "CHORD, POLY: C#4 C E G");
    frs_midi(0x83, 61, 0);
    blocks(2);
    check(!view().held && !ldmidi(), "its off: the three let go");
    frs_param_set(P_LD_MODE, 0);
    frs_midi(0x93, 61, 100);
    blocks(2);
    check(ldmidi() == 1u && ldk.note[0] == 61, "CHORD, MONO: the note as it came");
    frs_midi(0x83, 61, 0);
}

/* ------------------------------------------------------------------ a take, on air and live */
#define END 4500u
#define SNAP 2000u
static const uint8_t ST_CHORD[] = {'F', 'R', 's', 't', STATE_V, STATE_ASHAPE, 0, STATE_LDCH, 9u | LDCH_CHORD};
static const uint8_t ST_THREE[] = {'F', 'R', 's', 't', STATE_V, STATE_ASHAPE, 0, STATE_LDCH, 3u};
typedef struct {
    uint32_t ms;
    uint8_t op, n, a[4];
    const uint8_t *st;                   /* (FST_STATE's bytes, n of them) */
} tk_t;
static tk_t TAKE[] = {
    {200, FST_MIDI, 4, {0x98, 60, 100, 0}, 0}, {500, FST_MIDI, 4, {0x98, 67, 90, 0}, 0},
    {800, FST_MIDI, 4, {0xD8, 80, 0, 0}, 0},   {900, FST_MIDI, 4, {0xE8, 0, 0x50, 0}, 0},
    {1200, FST_MIDI, 4, {0x88, 67, 0, 0}, 0},  {1400, FST_MIDI, 4, {0x88, 60, 0, 0}, 0},
    {1500, FST_PARAM16, 4, {P_LD_MODE & 0xFF, P_LD_MODE >> 8, 1, 0}, 0},
    {1600, FST_STATE, sizeof ST_CHORD, {0}, ST_CHORD},
    {1800, FST_MIDI, 4, {0x98, 61, 100, 0}, 0},   /* (the snap at 2000: its chord held) */
    {2400, FST_MIDI, 4, {0x88, 61, 0, 0}, 0},  {2600, FST_STATE, sizeof ST_THREE, {0}, ST_THREE},
    {2700, FST_MIDI, 4, {0x92, 40, 100, 0}, 0}, {3300, FST_MIDI, 4, {0x82, 40, 0, 0}, 0},
};
#define NTAKE (sizeof TAKE / sizeof TAKE[0])
static fst_start st0;
static void start0(void)                 /* the factory scene 1, HONEY's LEAD (9), LEAD CH 9; C MAJOR, TRIAD, CLOSE */
{
    uint8_t sc[FRS_SCENE_BYTES];
    frs_close();
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_scene_factory(1, sc);
    frs_scene_take(sc);
    frs_param_set(P_LD_PRESET, 9);
    frs_param_set(P_ROOT, 0);
    frs_param_set(P_SCALE, 1);
    frs_param_set(P_CHORD, 2);
    frs_param_set(P_VOICING, 0);
    frs_lead_midi_set(9, 0);
    fst_capture(&st0, 0);
}
static uint8_t *journal(size_t *out, int skip)   /* the take as a recorder journals it (skip: its kth op not) */
{
    static uint8_t jb[65536], room[FST_SCENE_BYTES];
    uint32_t last = 0, k;
    size_t jn = fst_jhead(&st0, 44100, 1793000000, jb);
    fst_ev e;
    for (k = 0; k < NTAKE; k++) {
        if ((int)k == skip)
            continue;
        memset(&e, 0, sizeof e);
        e.ms = TAKE[k].ms;
        if (TAKE[k].st) {
            e.op = FSTJ_SCENE;           /* (FST_STATE inline: its bytes, a[2] their length) */
            e.a[0] = FST_STATE;
            e.a[2] = TAKE[k].n;
            memcpy(room, TAKE[k].st, TAKE[k].n);
            jn += fst_jevent(&e, &last, room, 0, jb + jn);
        } else {
            e.op = TAKE[k].op;
            e.n = TAKE[k].n;
            memcpy(e.a, TAKE[k].a, 4);
            jn += fst_jevent(&e, &last, 0, 0, jb + jn);
        }
    }
    memset(&e, 0, sizeof e);
    e.ms = END;
    e.op = FSTJ_END;
    jn += fst_jevent(&e, &last, 0, 0, jb + jn);
    return fst_finish(jb, jn, out);
}
static fst_air air;
static fst_file file;
static uint32_t ons_end;
static uint64_t on_air(const uint8_t *s, size_t n, uint32_t *ch_at_open)
{
    if (fst_read(s, n, &file) || fst_air_open(&air, &file, 44100.0f))
        return 0;
    *ch_at_open = frs_lead_midi_get();
    hash = 1469598103934665603u;
    while (fst_air_ms(&air) < END) {
        frs_render(L, R, 256);
        fst_air_after(&air);
        hash_block(256);
    }
    ons_end = view().ons;
    return hash;
}
static fst_live lv;
static uint32_t ms0;
static void give(uint32_t from)
{
    uint32_t k;
    for (k = 0; k < NTAKE; k++)
        if (TAKE[k].ms > from)
            fst_live_at(&lv, ms0 + TAKE[k].ms, TAKE[k].op, TAKE[k].st ? TAKE[k].st : TAKE[k].a, TAKE[k].n, 0);
}
static void run_to(uint32_t ms, int hashing)
{
    while (frs_ms() < ms0 + ms) {
        frs_render(L, R, 256);
        if (hashing)
            hash_block(256);
    }
}
static uint32_t brain_of(uint8_t *out, const uint8_t *scene)   /* the brain, both parts (lead.c's) */
{
    static uint8_t ld[LIVE_LEAD_BYTES];
    uint32_t m = 0, n = live_save(out, FRS_LIVE_BYTES, scene, swell_held, alone_lv, 0, ld, sizeof ld, &m);
    if (n && m && n + m <= FRS_LIVE_BYTES) {
        memcpy(out + n, ld, m);
        n += m;
    }
    return n;
}

static uint8_t *take_made;
static size_t take_n;
static void streams(void)
{
    size_t n = 0, cn = 0;
    uint8_t *s, *c;
    uint64_t h1, h2, hl;
    uint32_t ch1 = 0, ch2 = 0, chc = 0;
    start0();
    s = journal(&n, -1);
    check(s && !fst_read(s, n, &file), "a take with LEAD CH journaled, finished, read");
    h1 = on_air(s, n, &ch1);
    h2 = on_air(s, n, &ch2);
    check(h1 && h1 == h2 && ch1 == 9u && ch2 == 9u, "on air: LEAD CH 9 at its start; twice, every sample the same");
    check(ons_end == 5u && frs_lead_midi_get() == 3u, "on air: the LEAD struck five (MONO's first, its second glided "
                                                      "to; the chord's three; the bass's note), LEAD CH 3 at its end");
    if (ons_end != 5u)
        printf("ldch: on air %u struck, LEAD CH %u\n", ons_end, frs_lead_midi_get());
    fst_live_open(&lv, &st0, 44100.0f);
    check(frs_lead_midi_get() == 9u, "live: LEAD CH 9 at its start");
    ms0 = frs_ms();
    give(0);
    hash = 1469598103934665603u;
    run_to(END, 1);
    hl = hash;
    check(hl == h1 && lv.refused == 0u && lv.missed == 0u, "live, op by op: every sample the file's on air");
    c = journal(&cn, 7);
    check(c && on_air(c, cn, &chc) != h1, "the control: without the pair (CHORD), another sound");
    free(c);
    take_made = s;                       /* (make's) */
    take_n = n;
}

/* ------------------------------------------------------------------ the running state */
static void snaps(void)
{
    static uint8_t snap_st[FRS_LIVE_BYTES], a[FRS_LIVE_BYTES], b[FRS_LIVE_BYTES];
    static fst_start snap_start;
    uint32_t snap_n, an, bn, m;
    start0();
    fst_live_open(&lv, &st0, 44100.0f);
    ms0 = frs_ms();
    give(0);
    run_to(SNAP, 0);
    fst_capture(&snap_start, 0);
    snap_n = frs_live_save(snap_st, sizeof snap_st, snap_start.scene);
    m = frs_live_ms(snap_st, snap_n);
    check(snap_n && ldmidi() == 3u && view().held == 3u && frs_lead_midi_get() == (9u | FRS_LDCH_CHORD),
          "A at the snap: MIDI's chord held on the LEAD, LEAD CH 9 with CHORD");
    an = brain_of(a, snap_start.scene);
    run_to(2500u, 0);
    check(!ldmidi() && !view().held, "A: its MIDI off lets the chord go");
    fst_live_open(&lv, &snap_start, 44100.0f);
    check(!fst_live_state(&lv, snap_st, snap_n, snap_start.scene) && frs_ms() == m,
          "B: opened on the snap, its running state loaded");
    bn = brain_of(b, snap_start.scene);
    check(an == bn && !memcmp(a, b, an) && ldmidi() == 3u && view().held == 3u &&
              frs_lead_midi_get() == (9u | FRS_LDCH_CHORD),
          "B: A's brain bit for bit, MIDI's chord on the LEAD, LEAD CH 9 with CHORD");
    give(SNAP);
    run_to(2500u, 0);
    check(!ldmidi() && !view().held, "B: the same MIDI off lets it go (none hanging)");
}

/* ------------------------------------------------------------------ for 3.4's engine (ldch_old) */
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
/* 3.4's take with notes on channel 4 (ldch_old make DIR: GLINT CH 4 in its start), heard here as 3.4 heard it: every
 * sample (LEAD CH OFF at the start, nothing on the LEAD); with LEAD CH 4 (set once opened), another sound */
static size_t got(const char *dir, const char *name, uint8_t *b, size_t max)
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
static uint64_t air34(const uint8_t *s, size_t n, uint32_t ch)
{
    if (fst_read(s, n, &file) || fst_air_open(&air, &file, 44100.0f))
        return 0;
    if (ch)
        frs_lead_midi_set(ch, 0);
    hash = 1469598103934665603u;
    while (fst_air_ms(&air) < 4000u) {   /* (ldch_old.c's T34_END) */
        frs_render(L, R, 256);
        fst_air_after(&air);
        hash_block(256);
    }
    return hash;
}
static void heard34(const char *dir)
{
    static uint8_t t34[1 << 16];
    char t[32], want[32];
    size_t n = got(dir, "take-3.4.frstream", t34, sizeof t34);
    uint64_t h;
    if (!n) {
        printf("ldch: no 3.4 take in %s (ldch_old make): not heard\n", dir);
        return;
    }
    memset(t, 0, sizeof t);
    got(dir, "take-3.4.txt", (uint8_t *)t, sizeof t - 1u);
    h = air34(t34, n, 0);
    snprintf(want, sizeof want, "%016llx\n", (unsigned long long)h);
    check(h && frs_lead_midi_get() == FRS_LDCH_DEF && !strcmp(t, want),
          "3.4's take with notes on channel 4: every sample as 3.4 played it, LEAD CH OFF");
    if (strcmp(t, want))
        printf("ldch: 3.4's take %s here, %s there", want, t);
    check(air34(t34, n, 4) != h && view().ons > 0u, "... with LEAD CH 4: the LEAD plays it, another sound");
}

static int make(const char *dir)
{
    uint8_t s[FRS_STATE_BYTES];
    uint32_t n;
    char t[32];
    int r = 0;
    heard34(dir);
    fresh();
    n = frs_state_save(s);
    r |= put(dir, "state-def.bin", s, n);
    frs_lead_midi_set(9, 1);
    n = frs_state_save(s);
    r |= put(dir, "state-9c.bin", s, n);
    r |= put(dir, "take.frstream", take_made, take_n);
    snprintf(t, sizeof t, "%016llx\n", (unsigned long long)ldch_sound());
    r |= put(dir, "sound.txt", t, strlen(t));
    printf("ldch: made in %s, the sound %s", dir, t);
    return r;
}

int main(int argc, char **argv)
{
    api();
    midi();
    streams();
    snaps();
    if (argc == 3 && !strcmp(argv[1], "make") && make(argv[2])) {
        printf("FAIL ldch: not written in %s\n", argv[2]);
        fails++;
    }
    printf("ldch: %d ok, %d FAIL\n", oks, fails);
    return fails ? 1 : 0;
}
