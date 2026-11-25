/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* CHORD PADS (card #208, engine/chords.c) from the inside (frasil_engine.c's statics), built with FRS_CHORDS 1 and the
 * LEAD (FrasilVibe's engine), and without them (FRS_CHORDS 0, the iPad's and the tests'): a performance with no move
 * renders the same samples in both (its hash printed last: engine/tests/run.sh compares the two). With them: the
 * chords' words in a few keys; a move (the drones on its chord, their own notes silent and kept, GLINT's and the bass's
 * notes moved along the scale, an altered one); ON THE BEAT on the grid's beat; GLIDE voice by voice over MORPH's time;
 * RESET; FOLLOW; a hand on the drones meanwhile; the running state; a scene taken, RELEASE ALL.
 *   engine/tests/run.sh, or: chords */
#include <stdio.h>
#include "../frasil_engine.c"

static float L[4096], R[4096];
static int fails;
static uint64_t hash = 0xCBF29CE484222325u;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "  ok" : "FAIL", what);
    fails += !ok;
}

static void render(uint32_t ms)          /* ms of it, its samples into the hash */
{
    uint32_t n = (uint32_t)((uint64_t)ms * 44100u / 1000u), i;
    while (n) {
        uint32_t k = n < 256u ? n : 256u;
        frs_render(L, R, k);
        for (i = 0; i < k; i++) {
            uint32_t a, b;
            memcpy(&a, &L[i], 4);
            memcpy(&b, &R[i], 4);
            hash = (hash ^ a) * 0x100000001B3u;
            hash = (hash ^ b) * 0x100000001B3u;
        }
        n -= k;
    }
}

/* the scene: D minor, the drones on D3 F3 A3, GLINT's sequence on D4 every fourth sixteenth, the bass's on D2 */
static uint8_t scene[FRS_SCENE_BYTES];
static void scene_make(void)
{
    scene_t sc;
    uint32_t i;
    static const int8_t C0[5] = {50, 50, 53, 57, -1};
    frs_scene_new(scene);
    memcpy(&sc, scene, sizeof sc);
    sc.v[P_ROOT] = 2;
    sc.v[P_SCALE] = 2;
    sc.v[P_VOICING] = 0;
    sc.v[P_SQ_TYPE] = SQT_SEQ;
    sc.v[P_ARP_RATE] = 7;                /* (sixteenths) */
    sc.v[P_ARP_CHANCE] = 127;
    sc.v[P_SQ_LEN] = 15;
    for (i = 0; i < SC_SEQ; i++)
        sc.seq[i] = (uint8_t)(i % 4u ? SQ_REST : 62u);
    for (i = 0; i < SC_BSEQ; i++)
        sc.bnote[i] = (uint8_t)(i % 4u ? SQ_TIE : 38u);
    sc.bq_on = SQT_SEQ;
    sc.bq_steps = 15;
    sc.bq_len = 0;
    sc.nch = 1;
    memcpy(sc.ch[0], C0, 5);
    memcpy(scene, &sc, sizeof sc);
}

#if FRS_CHORDS
static void gated(uint32_t *set)         /* the drone voices the DSP holds (gate), by note */
{
    frasil_view_t v[FR_NV];
    uint32_t i;
    frasil_view(v);
    memset(set, 0, 16);
    for (i = 0; i < FR_NV; i++)
        if (v[i].note >= 0 && v[i].gate)
            bit_put(set, v[i].note, 1);
}

static int drones_are(const int *n)      /* exactly these notes held by the DSP (-1 ends them) */
{
    uint32_t a[4], b[4] = {0, 0, 0, 0};
    gated(a);
    for (; *n >= 0; n++)
        bit_put(b, *n, 1);
    return !memcmp(a, b, sizeof a);
}

static int glint_has(int note)           /* a GLINT voice on the note (as it sounds: GLINT's OCTAVE on it) */
{
    glint_view_t g[GLINT_NV];
    uint32_t i;
    frasil_glint_view(g);
    for (i = 0; i < GLINT_NV; i++)
        if (g[i].note == note)
            return 1;
    return 0;
}

static int bass_dsp(void)                /* the bass's note as the DSP sounds it (BASS's OCTAVE on it) */
{
    bass_view_t b;
    frasil_bass_view(&b);
    return b.note;
}

static void words(void)
{
    static const uint8_t D[11][2] = {{0, 3}, {2, 3}, {3, 2}, {5, 2}, {4, 2}, {6, 1}, {4, 9}, {1, 2}, {0, 0}, {2, 0},
                                     {3, 3}};
    static const char *const NAME[11] = {"Dm9", "Fmaj9", "Gm7", "Bbmaj7", "Am7", "C6", "A7", "Em7b5", "Dm", "F", "Gm9"};
    static const char *const NUM[11] = {"i9", "III9", "iv7", "VI7", "v7", "VII6", "V7", ("ii\xC2\xB0" "7"), "i", "III", "iv9"};
    char name[16], num[16], all[64];
    int8_t n[8];
    uint32_t i, ok = 1, k;
    for (i = 0; i < 11u; i++) {
        frs_chord_name(2, 2, D[i][0], D[i][1], name, num);
        if (strcmp(name, NAME[i]) || strcmp(num, NUM[i])) {
            printf("     %s %s (want %s %s)\n", name, num, NAME[i], NUM[i]);
            ok = 0;
        }
    }
    check(ok, "D minor's eleven pads: Dm9 i9, Fmaj9 III9, Gm7 iv7, Bbmaj7 VI7 ... A7 V7, Em7b5 ii\xC2\xB0" "7, Gm9 iv9");
    frs_chord_name(9, 1, 5, 0, name, num);
    frs_chord_name(9, 1, 2, 0, all, 0);
    check(!strcmp(name, "F#m") && !strcmp(num, "vi") && !strcmp(all, "C#m"), "A major: F#m vi, C#m (its sharps)");
    frs_chord_name(10, 2, 0, 0, name, num);
    frs_chord_name(10, 2, 5, 2, all, 0);
    check(!strcmp(name, "Bbm") && !strcmp(all, "Gbmaj7"), "Bb minor: Bbm, Gbmaj7 (its flats)");
    frs_chord_name(0, 1, 6, 2, name, num);
    check(!strcmp(name, "Bm7b5") && !strcmp(num, "vii\xC2\xB0" "7"), "C major: Bm7b5 vii\xC2\xB0" "7");
    frs_chord_name(0, 1, 3, 8, name, num);
    check(!strcmp(name, "Fm") && !strcmp(num, "iv"), "C major: IV MINOR, Fm iv (borrowed)");
    frs_chord_name(0, 10, 4, 2, name, num);
    check(!strcmp(name, "G7") && !strcmp(num, "V7"), "C PENTA MAJ: on MAJOR's seven, G7 V7");
    frs_chord_name(0, 1, 1, 4, name, num);
    frs_chord_name(0, 1, 4, 6, all, 0);
    check(!strcmp(name, "Dsus2") && !strcmp(num, "IIsus2") && !strcmp(all, "Gadd9"), "C major: Dsus2 IIsus2, Gadd9");
    k = frs_chord_notes(2, 2, 3, 3, n);
    check(k == 5u && n[0] == 67 && n[1] == 70 && n[2] == 74 && n[3] == 77 && n[4] == 81, "Gm9's notes: G4 Bb4 D5 F5 A5");
    check(frs_chord_notes(2, 2, 7, 0, n) == 0u && frs_chord_notes(2, 2, 0, FRS_CM_TYPES, n) == 0u, "no VIII, no 11th type");
}

static void moves(void)
{
    static const int HOME[] = {50, 53, 57, -1}, BB7[] = {46, 50, 53, 57, -1}, A7[] = {45, 49, 52, 55, -1},
                     DM9[] = {50, 53, 57, 60, 64, -1}, GM7[] = {55, 58, 62, 65, -1};
    static uint8_t live[FRS_LIVE_BYTES];
    frs_view_t v, w;
    uint32_t home_held[4], i, n;
    int beat, ok;
    frs_view(&v);
    memcpy(home_held, v.held, sizeof home_held);
    check(drones_are(HOME) && v.cm_on == 0u && v.cm_follow == FRS_CM_ALL, "no move: the scene's drones, every track following");

    frs_param_set(P_MORPH, 0);           /* (CUT: the drones together) */
    frs_chord_go(5, 2, 0);               /* VI7, Bbmaj7 */
    render(30);
    frs_view(&v);
    check(v.cm_on && v.cm_hold && v.cm_degree == 5u && v.cm_type == 2u && v.cm_n == 4u && v.cm_notes[0] == 46 &&
              v.cm_glide == 1.0f, "VI7: the move, its chord voiced from Bb2 (the drones' D3 nearest)");
    check(drones_are(BB7), "  the drones play Bb2 D3 F3 A3 (D3 F3 A3 never struck again)");
    check(!memcmp(v.held, home_held, sizeof home_held), "  their own notes kept, silent (a scene stored has them)");
    check(frs_chord_note(FRS_CM_GLINT, 62) == 58 && frs_chord_note(FRS_CM_BASS, 38) == 34 &&
              frs_chord_note(FRS_CM_LEAD, 69) == 65 && frs_chord_note(FRS_CM_MIDI, 74) == 70,
          "  GLINT, BASS, LEAD, MIDI: two steps down the scale (D4 Bb3, D2 Bb1, A4 F4)");
    render(1500);
    check(glint_has(58 + 12 * st.v[P_GL_OCTAVE]) && !glint_has(62 + 12 * st.v[P_GL_OCTAVE]),
          "  GLINT's sequence on Bb, not D");
    check(bass_dsp() == bass_sounding(34), "  the bass's on Bb");

    frs_chord_go(4, 9, 1);               /* V7, A7, ON THE BEAT */
    frs_view(&v);
    beat = cm_beat();
    check(v.cm_wait && v.cm_degree_next == 4u && v.cm_type_next == 9u && v.cm_degree == 5u && drones_are(BB7),
          "V7 ON THE BEAT: waiting, VI7 sounding");
    for (n = 0; n < 3000u && cm.wait; n++)
        render(1);
    frs_view(&v);
    check(!v.cm_wait && v.cm_degree == 4u && cm_beat() != beat && n < 2000u, "  there on the grid's next beat");
    render(20);
    check(drones_are(A7), "  the drones on A2 C#3 E3 G3");
    check(frs_chord_note(FRS_CM_GLINT, 65) == 61 && frs_chord_note(FRS_CM_GLINT, 62) == 57,
          "  F4 three steps down is C4, made C#4 (A7's own); D4 A3");

    frs_param_set(P_MORPH, 2);           /* (2 S) */
    frs_chord_go(0, 3, 0);               /* i9, Dm9: GLIDE */
    render(10);
    frs_view(&v);
    {
        uint32_t s[4];
        gated(s);
        ok = bit_get(s, 50) && !bit_get(s, 45) && bit_get(s, 49) && v.cm_glide > 0.0f && v.cm_glide < 0.05f;
    }
    check(ok, "i9 GLIDE over MORPH's 2 S: the lowest voice first (A2 to D3), the others still");
    render(1000);
    frs_view(&v);
    check(v.cm_glide > 0.45f && v.cm_glide < 0.6f && !drones_are(DM9), "  halfway, not there");
    render(1100);
    frs_view(&v);
    check(v.cm_glide == 1.0f && drones_are(DM9), "  there: D3 F3 A3 C4 E4");

    frs_chord_home();                    /* RESET */
    render(20);
    frs_view(&v);
    check(!v.cm_on && !v.cm_hold && drones_are(HOME) && frs_chord_note(FRS_CM_GLINT, 62) == 62,
          "RESET: the scene's drones again, GLINT's notes as they are");
    render(600);
    check(bass_dsp() == bass_sounding(38), "  the bass on D again");

    frs_param_set(P_MORPH, 0);
    frs_chord_follow(FRS_CM_ALL & ~FRS_CM_DRONE);
    frs_chord_go(3, 2, 0);               /* iv7, Gm7, the drones not following */
    render(20);
    check(drones_are(HOME) && frs_chord_note(FRS_CM_GLINT, 62) == 67, "FOLLOW without DRONE: the scene's drones, GLINT up to G4");
    frs_chord_follow(FRS_CM_ALL);
    render(20);
    check(drones_are(GM7), "  DRONE following again: G3 Bb3 D4 F4");

    frs_key_down(0, 45, 100, 0);         /* a hand on the drones: A2's chord latched, silent while the move holds them */
    render(20);
    frs_key_up(0);
    render(20);
    frs_view(&v);
    check(drones_are(GM7) && bit_get(v.latched, 45), "a chord latched meanwhile: silent, the move's sounding");

    n = frs_live_save(live, sizeof live, scene);
    frs_view(&v);
    frs_close();
    frs_init(44100.0f);
    frs_scene_take(scene);
    render(50);
    check(n && frs_live_load(live, n, scene) == 0, "the running state saved and loaded into an engine opened again");
    frs_view(&w);
    check(w.cm_on && w.cm_hold && w.cm_degree == 3u && w.cm_type == 2u && w.cm_n == v.cm_n &&
              !memcmp(w.cm_notes, v.cm_notes, sizeof v.cm_notes) && w.cm_follow == FRS_CM_ALL &&
              frs_chord_note(FRS_CM_GLINT, 62) == 67, "  the move in it: iv7, the drones on it, GLINT moved");
    render(30);
    check(drones_are(GM7), "  the drones on Gm7");

    frs_chord_home();
    render(20);
    frs_view(&v);
    for (i = 0, ok = 1; i < 128u; i++)
        if (bit_get(v.held, (int)i) != (held_by[i] != 0))
            ok = 0;
    {
        uint32_t s[4];
        gated(s);
        check(ok && bit_get(s, 45) && bit_get(s, 50), "RESET: the chord latched meanwhile sounds now, with the scene's");
    }

    frs_chord_go(6, 1, 0);
    render(20);
    frs_scene_take(scene);
    render(20);
    frs_view(&v);
    check(!v.cm_on && !v.cm_hold && drones_are(HOME) && frs_chord_note(FRS_CM_GLINT, 62) == 62,
          "a scene taken: no move, its drones");
    frs_chord_go(2, 0, 0);
    render(20);
    frs_release_all();
    render(20);
    frs_view(&v);
    check(!v.cm_on && !v.cm_hold, "RELEASE ALL: no move");
}
#endif

int main(void)
{
    frs_init(44100.0f);
    scene_make();
    frs_scene_take(scene);
    render(1000);                        /* the same performance, no move: the same samples with the pads or without */
    frs_key_down(1, 45, 90, 0);
    render(400);
    frs_key_up(1);
    frs_param_set(P_KB, 1);
    frs_key_down(2, 69, 100, 0);
    render(300);
    frs_key_up(2);
    frs_midi(0xB0, 74, 90);
    frs_param_set(P_KB, 2);
    frs_key_down(3, 41, 100, 0);
    render(500);
    frs_key_up(3);
    frs_param_set(P_KB, 0);
    frs_play();
    render(800);
    frs_play();
    render(1000);
    printf("samples without a move: %016llx\n", (unsigned long long)hash);
#if FRS_CHORDS
    frs_close();
    frs_init(44100.0f);
    frs_scene_take(scene);
    render(2000);
    words();
    moves();
#endif
    printf("chords: %d failed\n", fails);
    return fails != 0;
}
