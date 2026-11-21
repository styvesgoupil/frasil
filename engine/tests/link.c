/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* TRACKS LINK (card #212), on the engine as the app builds it (FRS_LEAD, FRS_MIDI_OUT): every note the brain sends the
 * voices logged (FRASIL_EV_LOG, frasil.c's ev_push), what sounds made from them.
 *   The offset: a note's degree of SCALE from ROOT, -n/2 .. n/2 (C minor's, a pentatonic's, CHROMATIC's); a track's,
 *   the degrees from its baseline to the note followed, the same way (F to G one up, not six down).
 *   The baseline (Styves, admin #1929): a track LINKED while the drones hold F (not ROOT) moves nothing, then by the
 *   degrees from F (G one up, C three down); nothing held as it was linked, the next note followed its baseline. Two
 *   linked at different notes keep their own offsets; linking or letting one go never moves another; FOLLOW turned,
 *   each moved keeps its offset (a baseline that puts the other's note there, lk_virtual, off the scale too; the other
 *   holding nothing, kept till its first note), one moved only now from that note, the drones' chord not struck again;
 *   told along (frs_state_load) the same, the state alone none kept; a scene taken, NEW SCENE, every one none and none
 *   kept, the scene's first note followed theirs; LOAD SND ONLY keeps them.
 *   FOLLOW DRONE, BASS LINKED: C minor, from the drones' C to F, the bass up three degrees (C2 out as F2, retuned), on G
 *   three down; the drones let go, the offset stays.
 *   FOLLOW DRONE, GLINT LINKED: its arpeggio's steps moved on F, the step ringing let go where it went; a GLINT key held
 *   let go where it went out, the offset changed meanwhile. MIDI LINKED: MIDI OUT's keys the same, a KEYS chord's
 *   each note (card #194 in the engine, #219). NOTES OUT (the engine's, the iPad's): the bass's arpeggio and GLINT's
 *   out as they sound, the bass's note out let go and out again where its retune took it (no_move), each let go.
 *   FOLLOW BASS, DRONE LINKED: the drones' chord struck again where the bass's note takes it (C to F up three, to G down
 *   three); SPARKLE on the drones as they sound. The one followed's LINKED kept, not moving it: FOLLOW turned and back,
 *   each as it was. ROOT, SCALE turned: the baselines kept, the degrees counted again (none moved: nothing jumps).
 *   Every note let go that was struck, the drones', the bass's, GLINT's: through the link turned on and off, FOLLOW
 *   turned, ROOT turned, RELEASE ALL, a scene taken (a scene's latched chords and bass struck at once, as played, the
 *   lowest root followed). SPARKLE beside GLINT LINKED: its glint let go while GLINT plays that note elsewhere (latched,
 *   held by MIDI), none left on. GLINT's notes onto one pitch (one let go twice, two played onto one): let go once, with
 *   the last. The most the drones hold (eight latched chords and an eight-note step, 40) moved at once: nothing dropped
 *   from the voices' queue (frasil_ev_room), every voice on one held.
 *   The state: links off, its bytes as before (no pair 252); on, 252, 251 and the note followed once one is, 246 .. 249
 *   each baseline; the most (three baselines, ATTACK SHAPE's and LEAD CH's) in FRS_STATE_BYTES; read back (each offset at
 *   once); a state without the baselines (before them), each none, the next note followed theirs; 246 .. 249 no setting
 *   a state holds (an engine before them leaves them). A stream's start (fst_fresh: the state, then its scene) moves
 *   the tracks as the state has them.
 *   The running state (live.c): its link, the baselines, the notes where they sound, back as they were.
 *   engine/tests/run.sh (-DFRASIL_EV_LOG=link_log), or: link */
#include "../frasil_engine.c"
#include "../frasil_stream.h"
#include <stdio.h>
#include <stdlib.h>

#if !FRS_LEAD || !FRS_MIDI_OUT
#error "TRACKS LINK's test: built with -DFRS_LEAD=1 -DFRS_MIDI_OUT=1, dsp/lead.c (engine/tests/run.sh)"
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

/* ------------------------------------------------- what the voices are sent --- */
enum { EV_ON = 1, EV_OFF, EV_ALLOFF, EV_PRS, EV_SPON, EV_SPOFF, EV_SPPRS, EV_BON, EV_BOFF, EV_BPRS, EV_BPUL, EV_CLICK,
       EV_BRET };                        /* (frasil.c's) */
#define NLOG 20000
static struct {
    uint8_t t;
    int8_t n;
} lg[NLOG];
static int nlog;
static uint8_t out_dr[128], out_gl[128];   /* the drones' notes out, GLINT's (as the voices were sent them) */
static int out_bs = -1;                  /* the bass's */
static int dr_twice, dr_stray, gl_stray, bs_stray;
static int drops, drop_offs;             /* events the voices' queue had no room for (frasil.c drops them), let gos */
void link_log(uint8_t type, int note, float v);
void link_log(uint8_t type, int note, float v)
{
    int n = note & 127;
    (void)v;
    if (frasil_ev_room() == 0u) {        /* (full: this one dropped, never heard) */
        drops++;
        drop_offs += type == EV_OFF || type == EV_ALLOFF || type == EV_SPOFF || type == EV_BOFF;
        return;
    }
    if (nlog < NLOG) {
        lg[nlog].t = type;
        lg[nlog].n = (int8_t)note;
        nlog++;
    }
    switch (type) {
    case EV_ON:
        dr_twice += out_dr[n];
        out_dr[n] = 1;
        break;
    case EV_OFF:
        dr_stray += !out_dr[n];
        out_dr[n] = 0;
        break;
    case EV_ALLOFF:
        memset(out_dr, 0, sizeof out_dr);
        break;
    case EV_SPON:
        out_gl[n] = 1;
        break;
    case EV_SPOFF:
        if (note < 0)
            memset(out_gl, 0, sizeof out_gl);
        else {
            gl_stray += !out_gl[n];
            out_gl[n] = 0;
        }
        break;
    case EV_BON:
        out_bs = note;
        break;
    case EV_BRET:
        bs_stray += out_bs < 0;
        out_bs = note;
        break;
    case EV_BOFF:
        out_bs = -1;
        break;
    }
}
static void out_reset(void)
{
    memset(out_dr, 0, sizeof out_dr);
    memset(out_gl, 0, sizeof out_gl);
    out_bs = -1;
    dr_twice = dr_stray = gl_stray = bs_stray = drops = drop_offs = 0;
    nlog = 0;
}
static int dr_are(const int *w, int n)   /* the drones out are these n notes, and no other */
{
    int i, k = 0;
    for (i = 0; i < 128; i++)
        k += out_dr[i];
    for (i = 0; i < n; i++)
        if (!out_dr[w[i]])
            return 0;
    return k == n;
}
static int gl_any(void)
{
    int i, k = 0;
    for (i = 0; i < 128; i++)
        k += out_gl[i];
    return k;
}
static int logged(int from, uint8_t t, int n)   /* events of kind t on note n (-128: any) since `from` */
{
    int i, k = 0;
    for (i = from; i < nlog; i++)
        k += lg[i].t == t && (n == -128 || lg[i].n == n);
    return k;
}

static int dof(uint32_t t) { return lk.d[lk_ix(t)]; }   /* track t's offset (LK_*) */
static int bof(uint32_t t) { return lk.base[lk_ix(t)]; }   /* its baseline (-1 none) */

static int has_pair(const uint8_t *b, uint32_t n, uint32_t p)
{
    uint32_t i;
    for (i = 5; i + 1u < n; i += 2u)
        if (b[i] == p)
            return 1;
    return 0;
}
static int pair_of(const uint8_t *b, uint32_t n, uint32_t p)   /* its value, -1 none */
{
    uint32_t i;
    for (i = 5; i + 1u < n; i += 2u)
        if (b[i] == p)
            return b[i + 1u];
    return -1;
}

/* --------------------------------------------------------------- the engine --- */
static float L[256], R[256];
static frs_mout_ev_t mev[4096];
static uint32_t nmev;
static void run_ms(uint32_t ms)
{
    uint32_t n = (uint32_t)((float)ms * 44.1f), k;
    while (n) {
        k = n < 256u ? n : 256u;
        frs_render(L, R, k);
        nmev += frs_mout_read(mev + nmev, 4096u - nmev);
        n -= k;
    }
}
/* a MIDI note on track t's channel (0 the drones', 1 GLINT's, 2 the bass's), on or off, then 10 ms */
static void note(uint32_t t, int n, int on)
{
    frs_midi((uint8_t)((on ? 0x90u : 0x80u) | t), (uint8_t)n, (uint8_t)(on ? 100 : 0));
    run_ms(10);
}
static uint8_t sc0[FRS_SCENE_BYTES];
static void fresh(void)                  /* NEW SHARD, C minor, TRIAD, each track its channel, nothing by itself */
{
    frs_close();
    if (frs_init(44100.0f))
        exit(2);
    frs_scene_new(sc0);
    ((scene_t *)sc0)->v[P_ROOT] = 0;
    ((scene_t *)sc0)->v[P_SCALE] = 2;
    ((scene_t *)sc0)->v[P_CHORD] = 2;
    ((scene_t *)sc0)->v[P_VOICING] = 0;
    ((scene_t *)sc0)->v[P_GX_SPARK] = 0;
    frs_scene_take(sc0);
    frs_param_set(P_CANAL, 1);
    frs_param_set(P_CH_GLINT, 2);
    frs_param_set(P_CH_BASS, 3);
    frs_param_set(P_BS_PULSE, 0);
    frs_param_set(P_SQ_TYPE, 0);
    frs_param_set(P_BQ_ON, 0);
    run_ms(50);
    out_reset();
    nmev = 0;
}

static void degrees(void)
{
    static const struct {
        uint8_t scale;
        int n, d;
    } D[] = {{2, 60, 0}, {2, 62, 1}, {2, 63, 2}, {2, 65, 3}, {2, 67, -3}, {2, 68, -2}, {2, 70, -1}, {2, 71, -1},
             {2, 66, 3}, {2, 41, 3}, {2, 31, -3}, {11, 67, -2}, {11, 65, 2}, {11, 63, 1}, {0, 66, 6}, {0, 67, -5},
             {0, 59, -1}, {1, 64, 2}, {1, 71, -1}};
    uint32_t i;
    int ok = 1;
    fresh();
    for (i = 0; i < sizeof D / sizeof D[0]; i++) {
        st.v[P_SCALE] = D[i].scale;
        if (lk_degree(D[i].n) != D[i].d) {
            printf("  degree of %d (scale %u): %d, not %d\n", D[i].n, D[i].scale, lk_degree(D[i].n), D[i].d);
            ok = 0;
        }
    }
    st.v[P_SCALE] = 2;
    check(ok, "the offset: a note's degree of SCALE from ROOT, the one at or below, -n/2 .. n/2");
    st.v[P_ROOT] = 2;                    /* D minor: F its third */
    check(lk_degree(65) == 2 && lk_degree(62) == 0 && lk_degree(60) == -1, "the offset: from ROOT (D minor)");
    st.v[P_ROOT] = 0;
    check(lk_steps(60, 65) == 3 && lk_steps(60, 67) == -3 && lk_steps(65, 67) == 1 && lk_steps(67, 65) == -1 &&
              lk_steps(65, 60) == -3 && lk_steps(41, 79) == 1 && lk_steps(65, 65) == 0,
          "a track's offset: the degrees from its baseline (C minor: C to F 3, F to G 1 not -6, G to F -1)");
    check(lk_steps(-1, 65) == 0 && lk_steps(65, -1) == 0, "a track's offset: no baseline or no note followed, none");
    st.v[P_SCALE] = 0;
    check(lk_steps(61, 67) == 6 && lk_steps(60, 67) == -5, "a track's offset: CHROMATIC's, -5 .. 6");
    st.v[P_SCALE] = 11;
    check(lk_steps(65, 60) == -2 && lk_steps(65, 67) == 1, "a track's offset: PENTA MIN's (F to C two down)");
    {                                    /* FOLLOW turned: the baseline that keeps an offset, off the scale too */
        static const uint8_t SC[3] = {2, 0, 11};
        int sc, n, d, ok = 1;
        for (sc = 0; sc < 3; sc++)
            for (st.v[P_SCALE] = SC[sc], n = 24; n <= 100; n++)
                for (d = -5; d <= 6; d++)
                    if (lk_wrap(d) == d && lk_steps(lk_virtual(n, d), n) != d) {   /* (an offset the scale has) */
                        if (ok)
                            printf("  scale %u: lk_virtual(%d, %d) %d, %d degrees\n", SC[sc], n, d, lk_virtual(n, d),
                                   lk_steps(lk_virtual(n, d), n));
                        ok = 0;
                    }
        check(ok, "the baseline that keeps an offset (lk_virtual): its degrees to the note what were kept, off the "
                  "scale too");
        st.v[P_SCALE] = 2;
        check(lk_virtual(36, 3) == 31 && lk_virtual(65, 2) == 62 && lk_virtual(37, 3) == 31 && lk_virtual(65, 0) == 65 &&
                  lk_virtual(1, 3) >= 0 && lk_virtual(126, -3) <= 127,
              "lk_virtual: C2 three on, G1; C#2's from C2; within 0 .. 127");
    }
    st.v[P_SCALE] = 2;
}

static void drone_leads(void)
{
    static const int F[3] = {65, 68, 72}, G[3] = {67, 70, 74};
    int at;
    fresh();
    frs_link_set(0, FRS_LINK_BASS | FRS_LINK_DRONE);
    check(frs_link_get() == (FRS_LINK_BASS | FRS_LINK_DRONE) && lk.on == LK_BASS,
          "FOLLOW DRONE: the drone's own LINKED kept, never moving it");
    check(bof(LK_BASS) == -1 && bof(LK_DRONE) == -1, "FOLLOW DRONE: nothing held as it was linked, no baseline yet");
    note(2, 36, 1);
    check(out_bs == 36, "FOLLOW DRONE: nothing followed since it was turned on, the bass as played (C2)");
    at = nlog;
    note(0, 60, 1);
    check(bof(LK_BASS) == 60 && dof(LK_BASS) == 0 && out_bs == 36 && logged(at, EV_BRET, -128) == 0,
          "FOLLOW DRONE: the drones' first note (C) the bass's baseline, nothing moved");
    note(0, 60, 0);
    at = nlog;
    note(0, 65, 1);
    check(dr_are(F, 3), "FOLLOW DRONE: the drones as played (F Ab C), never moved");
    check(dof(LK_BASS) == 3 && out_bs == 41 && logged(at, EV_BRET, 41) == 1 && logged(at, EV_BON, -128) == 0,
          "FOLLOW DRONE, C minor, the drones from C to F: the bass up three degrees, C2 retuned to F2");
    note(0, 65, 0);
    check(out_bs == 41 && dof(LK_BASS) == 3, "FOLLOW DRONE: the drones let go, the offset stays");
    at = nlog;
    note(0, 67, 1);
    check(dr_are(G, 3) && dof(LK_BASS) == -3 && out_bs == 31 && logged(at, EV_BRET, 31) == 1,
          "FOLLOW DRONE, on G: three degrees down from C, the bass on G1");
    note(0, 67, 0);
    note(2, 36, 0);
    check(out_bs == -1, "FOLLOW DRONE: the bass let go");
    at = nlog;
    note(2, 39, 1);
    check(out_bs == 34 && logged(at, EV_BON, 34) == 1, "FOLLOW DRONE: the bass's next note moved too (Eb2 as Bb1)");
    note(2, 39, 0);
    frs_link_set(0, 0);
    note(2, 36, 1);
    check(out_bs == 36 && dof(LK_BASS) == 0 && bof(LK_BASS) == -1, "FOLLOW DRONE: off, the bass as played, no baseline");
    note(2, 36, 0);
    frs_link_set(0, FRS_LINK_BASS);
    note(2, 36, 1);
    check(out_bs == 36 && bof(LK_BASS) == -1, "FOLLOW DRONE: on again, nothing held by the drones: no baseline");
    note(2, 36, 0);
    frs_link_set(0, 0);
    /* Styves, admin #1929: linked while the drones hold F, not ROOT: its baseline, nothing moved; then the chord changes,
     * the bass by the degrees from F */
    note(0, 65, 1);
    note(2, 36, 1);
    at = nlog;
    frs_link_set(0, FRS_LINK_BASS);
    run_ms(10);
    check(bof(LK_BASS) == 65 && dof(LK_BASS) == 0 && out_bs == 36 && logged(at, EV_BRET, -128) == 0 &&
              logged(at, EV_BON, -128) == 0,
          "BASS LINKED while the drones hold F (not ROOT): its baseline F, nothing moved (C2 as played)");
    at = nlog;
    note(0, 65, 0);
    note(0, 67, 1);
    check(dof(LK_BASS) == 1 && out_bs == 38 && logged(at, EV_BRET, 38) == 1,
          "  then the drones on G: the bass one degree up (F to G), C2 retuned to D2");
    note(0, 67, 0);
    at = nlog;
    note(0, 60, 1);
    check(dof(LK_BASS) == -3 && out_bs == 31 && logged(at, EV_BRET, 31) == 1,
          "  on C: three down (F to C), the bass on G1");
    note(0, 60, 0);
    at = nlog;
    note(0, 65, 1);
    check(dof(LK_BASS) == 0 && out_bs == 36 && logged(at, EV_BRET, 36) == 1, "  back on F, its baseline: as played");
    note(2, 36, 0);
    note(0, 65, 0);
    frs_link_set(0, 0);
    check(dr_are(0, 0) && dr_twice == 0 && dr_stray == 0 && bs_stray == 0, "FOLLOW DRONE: every note let go, once");
}

static void glint_follows(void)
{
    int at, i, ok = 1, seen[128] = {0};
    fresh();
    frs_param_set(P_SQ_TYPE, SQT_ARP);
    frs_param_set(P_ARP_RANGE, 0);
    frs_param_set(P_ARP_MODE, 0);
    frs_param_set(P_ARP_CHANCE, 127);
    frs_link_set(0, FRS_LINK_GLINT);
    note(1, 60, 1);
    note(1, 63, 1);
    note(1, 67, 1);
    run_ms(1500);
    for (i = 0; i < nlog; i++)
        if (lg[i].t == EV_SPON) {
            seen[lg[i].n & 127]++;
            ok &= lg[i].n == 60 || lg[i].n == 63 || lg[i].n == 67;
        }
    check(ok && seen[60] && seen[63] && seen[67], "GLINT's arpeggio: nothing followed yet, as played (C Eb G)");
    note(0, 60, 1);                      /* (the drones' C: its baseline) */
    note(0, 60, 0);
    note(0, 65, 1);                      /* the drones on F: three up */
    run_ms(400);
    at = nlog;
    memset(seen, 0, sizeof seen);
    ok = 1;
    run_ms(1500);
    for (i = at; i < nlog; i++)
        if (lg[i].t == EV_SPON) {
            seen[lg[i].n & 127]++;
            ok &= lg[i].n == 65 || lg[i].n == 68 || lg[i].n == 72;
        }
    check(ok && seen[65] && seen[68] && seen[72], "GLINT's arpeggio, the drones on F: its steps three degrees up (F Ab C)");
    check(arp.n == 3 && arp.held[0] == 60 && arp.held[1] == 63 && arp.held[2] == 67, "GLINT's arpeggio: its notes as played");
    note(0, 65, 0);
    note(1, 60, 0);
    note(1, 63, 0);
    note(1, 67, 0);
    frs_panic();
    run_ms(20);
    check(gl_stray == 0 && !gl_any(), "GLINT's arpeggio: each step let go where it went out");
    /* a key on KEYS held while the offset changes: let go where it went */
    fresh();
    frs_link_set(0, FRS_LINK_GLINT);
    note(0, 60, 1);                      /* (C: its baseline) */
    note(0, 60, 0);
    note(0, 65, 1);
    at = nlog;
    note(1, 60, 1);
    check(logged(at, EV_SPON, 65) == 1 && out_gl[65], "GLINT's keys, the drones on F: C out as F");
    note(0, 65, 0);
    note(0, 67, 1);                      /* (on G: three down) */
    check(out_gl[65] && logged(at, EV_SPOFF, -128) == 0, "GLINT: a glint ringing left as it is");
    at = nlog;
    note(1, 60, 0);
    check(logged(at, EV_SPOFF, 65) == 1 && !gl_any(), "GLINT: its key let go where it went out (F), the offset changed");
    at = nlog;
    note(1, 60, 1);
    check(logged(at, EV_SPON, 55) == 1, "GLINT: the next note moved (C as G)");
    note(1, 60, 0);
    note(0, 67, 0);
    check(gl_stray == 0 && !gl_any(), "GLINT: every glint let go");
}

static void midi_follows(void)
{
    uint32_t i, on = 0, off = 0;
    int ok;
    fresh();
    frs_mout_set(FRS_MO_CH, 4);
    frs_link_set(0, FRS_LINK_MIDI);
    note(0, 60, 1);                      /* (C: its baseline) */
    note(0, 60, 0);
    note(0, 65, 1);
    nmev = 0;
    frs_mout_key(5, 60, 90, 1);
    run_ms(5);
    check(nmev == 1 && mev[0].status == 0x94 && mev[0].d1 == 65, "MIDI LINKED, the drones on F: its key's C out as F");
    note(0, 65, 0);
    note(0, 67, 1);
    nmev = 0;
    frs_mout_key(5, 60, 0, 0);
    run_ms(5);
    check(nmev == 1 && mev[0].status == 0x84 && mev[0].d1 == 65, "MIDI: its note off where it went out, the offset changed");
    nmev = 0;
    frs_mout_key(6, 60, 90, 1);
    frs_mout_key(6, 60, 0, 0);
    run_ms(5);
    check(nmev == 2 && mev[0].d1 == 55 && mev[1].d1 == 55, "MIDI: the next note moved (C as G)");
    /* its arpeggio: each step out moved, let go where it went */
    frs_mout_set(FRS_MO_TYPE, 1);
    frs_mout_set(FRS_MO_RANGE, 0);
    frs_mout_set(FRS_MO_CHANCE, 127);
    frs_mout_set(FRS_MO_AGATE, 50);
    nmev = 0;
    frs_mout_key(7, 60, 90, 1);
    run_ms(1200);
    note(0, 67, 0);
    note(0, 65, 1);                      /* (F: three up) */
    run_ms(1200);
    frs_mout_key(7, 60, 0, 0);
    frs_release_all();
    run_ms(50);
    for (i = 0, ok = 1; i < nmev; i++) {
        on += (mev[i].status & 0xF0u) == 0x90u;
        off += (mev[i].status & 0xF0u) == 0x80u;
        ok &= (mev[i].status & 0xF0u) != 0x90u || mev[i].d1 == 55 || mev[i].d1 == 65;
    }
    check(ok && on >= 4u && on == off, "MIDI's arpeggio: its steps moved (G, then F), each let go");
}

/* the engine's own MIDI OUT (mout.c, the iPad's since card #219): a KEYS chord, NOTES OUT */
static int mo_count(uint32_t status, int n, uint32_t from)   /* messages `status` on note n (-1: any) since `from` */
{
    uint32_t i;
    int c = 0;
    for (i = from; i < nmev; i++)
        c += mev[i].status == status && (n < 0 || mev[i].d1 == n);
    return c;
}
static int mo_balanced(uint32_t ch)      /* on channel ch (0 ..): each note on once till its off, every one let go */
{
    int open[128] = {0}, n;
    uint32_t i;
    for (i = 0; i < nmev; i++)
        if ((mev[i].status & 15u) == ch) {
            n = mev[i].d1 & 127;
            if ((mev[i].status & 0xF0u) == 0x90u && open[n]++)
                return 0;
            if ((mev[i].status & 0xF0u) == 0x80u && !open[n]--)
                return 0;
        }
    for (n = 0; n < 128; n++)
        if (open[n])
            return 0;
    return 1;
}
static void midi_engine(void)
{
    uint8_t c[FRS_CHORD_MAX];
    uint32_t nc, j, at;
    int ok, b0, b1, g1;
    fresh();
    frs_mout_set(FRS_MO_CH, 4);
    frs_mout_set(FRS_MO_HARM, 1);
    frs_mout_set(FRS_MO_NOTES, 1);
    frs_link_set(0, FRS_LINK_MIDI);
    note(0, 60, 1);                      /* (C: its baseline) */
    note(0, 60, 0);
    note(0, 65, 1);                      /* (the drones on F: three degrees up) */
    nc = frs_key_notes(60, 1, 1, c);
    nmev = 0;
    frs_mout_key(5, 60, 90, 1);
    run_ms(5);
    for (ok = nc >= 3u, j = 0; j < nc; j++)
        ok &= mo_count(0x94, scale_shift(c[j], 3, st.v), 0) == 1;
    check(ok && mo_count(0x94, -1, 0) == (int)nc, "MIDI LINKED, a KEYS chord: each of its notes out three degrees up");
    note(0, 65, 0);
    note(0, 67, 1);                      /* (G: three down, the chord held) */
    nmev = 0;
    frs_mout_key(5, 60, 0, 0);
    run_ms(5);
    for (ok = 1, j = 0; j < nc; j++)
        ok &= mo_count(0x84, scale_shift(c[j], 3, st.v), 0) == 1;
    check(ok && mo_count(0x84, -1, 0) == (int)nc, "  each let go where it went out, the offset changed");
    note(0, 67, 0);
    frs_mout_set(FRS_MO_HARM, 0);
    frs_mout_set(FRS_MO_NOTES, 0);
    /* NOTES OUT, FOLLOW DRONE with the bass and GLINT: their arpeggios out as they sound */
    fresh();
    frs_mout_notes(1);
    frs_link_set(0, FRS_LINK_BASS | FRS_LINK_GLINT);
    frs_param_set(P_BQ_ON, 1);
    frs_param_set(P_SQ_TYPE, 1);
    frs_param_set(P_BA_RANGE, 0);        /* (one octave each: their note alone) */
    frs_param_set(P_ARP_RANGE, 0);
    note(2, 36, 1);
    note(1, 72, 1);
    for (j = 0; j < 2000u && no.out[GEN_BASS] < 0; j++)
        run_ms(1);
    b0 = bass_sounding(36);
    b1 = bass_sounding(41);
    check(no.out[GEN_BASS] == b0, "NOTES OUT, FOLLOW DRONE: nothing held by the drones, the bass's out as played");
    at = nmev;
    note(0, 60, 1);                      /* (C: their baselines, nothing moved) */
    note(0, 60, 0);
    check(no.out[GEN_BASS] == b0 && dof(LK_BASS) == 0 && dof(LK_GLINT) == 0 && bof(LK_GLINT) == 60,
          "  the drones' first note (C) their baseline: nothing moved");
    at = nmev;
    note(0, 65, 1);
    check(mo_count(0x8F, b0, at) == 1 && mo_count(0x9F, b1, at) == 1 && no.out[GEN_BASS] == b1,
          "  the drones on F: the bass's note out let go, out again three degrees up, as it sounds");
    at = nmev;
    run_ms(2500);
    g1 = scale_shift(72, 3, st.v) + 12 * (int)st.v[P_GL_OCTAVE];
    check(mo_count(0x9F, b1, at) >= 2 && mo_count(0x9F, -1, at) == mo_count(0x9F, b1, at),
          "  the bass's next steps out moved too");
    check(mo_count(0x9D, g1, at) >= 2 && mo_count(0x9D, -1, at) == mo_count(0x9D, g1, at),
          "  GLINT's steps out moved, as they sound");
    note(1, 72, 0);
    note(2, 36, 0);
    note(0, 65, 0);
    frs_release_all();
    run_ms(400);
    check(mo_balanced(13) && mo_balanced(15) && no.out[GEN_BASS] < 0, "  every note out let go, once");
    frs_mout_notes(0);
}

static void bass_leads(void)
{
    static const int C[3] = {60, 63, 67}, F[3] = {65, 68, 72}, G[3] = {55, 58, 62};
    int at, i, ok, n;
    fresh();
    frs_link_set(1, FRS_LINK_DRONE | FRS_LINK_BASS);
    check(frs_link_get() == (FRS_LINK_DRONE | FRS_LINK_BASS | FRS_LINK_FOLLOW_BASS) && lk.on == LK_DRONE,
          "FOLLOW BASS: the bass's own LINKED kept, never moving it");
    note(0, 60, 1);
    check(dr_are(C, 3), "FOLLOW BASS: nothing played on the bass, the drones as played (C Eb G)");
    at = nlog;
    note(2, 36, 1);
    check(dr_are(C, 3) && bof(LK_DRONE) == 36 && logged(at, EV_OFF, -128) == 0,
          "FOLLOW BASS: the bass's first note (C2) the drones' baseline, nothing struck again");
    note(2, 36, 0);
    at = nlog;
    note(2, 41, 1);
    check(out_bs == 41 && logged(at, EV_BON, 41) == 1, "FOLLOW BASS: the bass as played, never moved");
    check(dr_are(F, 3) && logged(at, EV_OFF, -128) == 3 && logged(at, EV_ON, -128) == 3,
          "FOLLOW BASS, the bass from C to F: the drones' chord struck again three degrees up (F Ab C)");
    note(2, 41, 0);
    check(dr_are(F, 3), "FOLLOW BASS: the bass let go, the offset stays");
    note(2, 43, 1);
    check(dr_are(G, 3), "FOLLOW BASS, on G: three down (G Bb D)");
    /* SPARKLE on the drones as they sound */
    frs_param_set(P_GX_SPARK, 127);
    at = nlog;
    run_ms(3000);
    frs_param_set(P_GX_SPARK, 0);
    for (i = at, ok = 1, n = 0; i < nlog; i++)
        if (lg[i].t == EV_SPON) {
            int pc = lg[i].n % 12;
            n++;
            ok &= pc == 7 || pc == 10 || pc == 2;
        }
    check(ok && n > 3, "SPARKLE: on the drones as they sound (G Bb D)");
    note(0, 60, 0);
    note(2, 43, 0);
    run_ms(400);
    check(dr_are(0, 0) && out_bs == -1 && dr_twice == 0 && dr_stray == 0, "FOLLOW BASS: every note let go, once");
    check(held_by[60] == 0 && dr_snd[55] == 0 && dr_snd[58] == 0 && dr_snd[62] == 0, "FOLLOW BASS: the counts back to 0");
}

static void balanced(void)
{
    static const int C[3] = {60, 63, 67}, F[3] = {65, 68, 72}, DFA[3] = {62, 65, 68}, GBD[3] = {55, 58, 62};
    scene_t *s = (scene_t *)sc0;
    int at;
    fresh();
    frs_link_set(1, FRS_LINK_DRONE | FRS_LINK_GLINT);
    note(2, 36, 1);                      /* (the bass's C: their baselines) */
    note(2, 36, 0);
    note(2, 41, 1);
    note(0, 60, 1);
    check(dr_are(F, 3), "balanced: the drones up three on the bass's F (from its C)");
    frs_link_set(1, 0);
    run_ms(10);
    check(dr_are(C, 3), "balanced: off, the drones back where they were played");
    at = nlog;
    frs_link_set(1, FRS_LINK_DRONE);
    run_ms(10);
    check(dr_are(C, 3) && bof(LK_DRONE) == 41 && dof(LK_DRONE) == 0 && logged(at, EV_ON, -128) == 0,
          "balanced: on again while the bass holds F: its baseline, nothing struck again");
    frs_link_set(0, FRS_LINK_BASS);      /* FOLLOW DRONE: the bass from the drones' C (0) */
    run_ms(10);
    check(dr_are(C, 3) && out_bs == 41 && dof(LK_BASS) == 0 && bof(LK_BASS) == 60,
          "balanced: FOLLOW turned to the drone: the bass's baseline the drones' C, as played");
    frs_link_set(1, FRS_LINK_DRONE);
    run_ms(10);
    check(dr_are(C, 3) && out_bs == 41, "balanced: FOLLOW turned back to the bass: each as played");
    frs_link_set(1, FRS_LINK_DRONE | FRS_LINK_BASS);   /* (each LINKED, FOLLOW turned: the one followed's kept) */
    run_ms(10);
    check(dr_are(C, 3) && out_bs == 41 && lk.on == LK_DRONE && bof(LK_DRONE) == 41,
          "balanced: BASS LINKED while followed: not moved, the drones' baseline kept");
    frs_link_set(0, FRS_LINK_DRONE | FRS_LINK_BASS);
    run_ms(10);
    check(dr_are(C, 3) && out_bs == 41 && lk.on == LK_BASS && dof(LK_BASS) == 0,
          "balanced: FOLLOW DRONE, BASS LINKED again (from the drones' C: none), DRONE's kept");
    frs_link_set(1, FRS_LINK_DRONE | FRS_LINK_BASS);
    run_ms(10);
    check(dr_are(C, 3) && out_bs == 41 && lk.on == LK_DRONE, "balanced: FOLLOW BASS again, DRONE LINKED again");
    frs_param_set(P_ROOT, 2);            /* (D minor, the bass on its baseline: none moved, nothing jumps) */
    run_ms(10);
    check(bof(LK_DRONE) == 41 && dof(LK_DRONE) == 0 && dr_are(C, 3),
          "balanced: ROOT turned, the bass on the drones' baseline: nothing moved");
    frs_param_set(P_ROOT, 0);
    run_ms(10);
    note(2, 43, 1);                      /* (G: one up from F) */
    check(dof(LK_DRONE) == 1 && dr_are(DFA, 3), "balanced: the bass on G, the drones one up from its F (D F Ab)");
    frs_param_set(P_ROOT, 2);            /* D minor: F to G one still, the drones one up along D minor */
    run_ms(10);
    check(bof(LK_DRONE) == 41 && dof(LK_DRONE) == 1 && dr_are((const int[3]){62, 65, 69}, 3),
          "balanced: ROOT turned (D minor): the baseline kept, its degrees counted again, the drones there (D F A)");
    frs_param_set(P_ROOT, 0);
    run_ms(10);
    check(dof(LK_DRONE) == 1 && dr_are(DFA, 3), "balanced: ROOT back");
    note(2, 36, 1);                      /* (C: three down from F) */
    check(dof(LK_DRONE) == -3 && dr_are(GBD, 3), "balanced: the bass on C, the drones three down from its F (G Bb D)");
    frs_param_set(P_SCALE, 11);          /* PENTA MIN: F to C two down (C Eb G as G Bb Eb) */
    run_ms(10);
    check(bof(LK_DRONE) == 41 && dof(LK_DRONE) == -2 && dr_are((const int[3]){55, 58, 63}, 3),
          "balanced: SCALE turned, the baseline kept, its degrees counted again");
    frs_param_set(P_SCALE, 2);
    run_ms(10);
    check(dr_are(GBD, 3), "balanced: SCALE back");
    frs_release_all();
    run_ms(20);
    check(dr_are(0, 0) && out_bs == -1, "balanced: RELEASE ALL, everything let go");
    note(0, 60, 0);
    note(2, 36, 0);
    note(2, 43, 0);
    note(2, 41, 0);
    note(0, 60, 1);
    check(dr_are(GBD, 3), "balanced: after RELEASE ALL the offset stays");
    note(0, 60, 0);
    check(dr_are(0, 0) && dr_twice == 0 && dr_stray == 0 && bs_stray == 0, "balanced: every note let go once, so far");
    /* a scene taken while the drones are held: let go; every baseline none, its bass the first note followed: its
     * latched chord and bass struck once, as played */
    note(0, 63, 1);
    memset(s->ch, -1, sizeof s->ch);
    s->nch = 1;
    s->ch[0][0] = 60;
    s->ch[0][1] = 60;
    s->ch[0][2] = 63;
    s->ch[0][3] = 67;
    s->bass = (int8_t)(43 + 12 * ((int)s->v[P_BS_OCTAVE] - 2));   /* (as it sounds) */
    at = nlog;
    frs_scene_take(sc0);
    run_ms(20);
    check(out_bs == 43 && bof(LK_DRONE) == 43 && dof(LK_DRONE) == 0 && dr_are(C, 3),
          "a scene, FOLLOW BASS: its bass on G the drones' baseline, its latched C chord as played (C Eb G)");
    check(logged(at, EV_ON, 60) == 1 && logged(at, EV_ON, 63) == 1 && logged(at, EV_ON, 67) == 1 &&
              logged(at, EV_ON, -128) == 3,
          "a scene, FOLLOW BASS: its drones struck once, where they go");
    note(0, 63, 0);
    frs_panic();
    run_ms(20);
    check(dr_are(0, 0) && out_bs == -1 && !gl_any(), "a scene: PANIC, everything let go");
    frs_link_set(0, FRS_LINK_BASS);      /* FOLLOW DRONE: the scene's lowest latched root */
    note(0, 60, 1);                      /* (a baseline before the scene: C, the bass three up on F) */
    note(0, 60, 0);
    note(0, 65, 1);
    note(0, 65, 0);
    check(bof(LK_BASS) == 60 && dof(LK_BASS) == 3, "a scene, FOLLOW DRONE: the bass three up before it (C to F)");
    s->nch = 2;
    s->ch[0][0] = 70;                    /* (its chords not lowest first: the lowest root followed all the same) */
    s->ch[0][1] = 70;
    s->ch[0][2] = 74;
    s->ch[0][3] = 77;
    s->ch[1][0] = 65;
    s->ch[1][1] = 65;
    s->ch[1][2] = 68;
    s->ch[1][3] = 72;
    s->bass = (int8_t)(36 + 12 * ((int)s->v[P_BS_OCTAVE] - 2));
    at = nlog;
    frs_scene_take(sc0);
    run_ms(20);
    check(lk.ref == 65 && bof(LK_BASS) == 65 && dof(LK_BASS) == 0 && out_bs == 36 && logged(at, EV_BON, 36) == 1 &&
              logged(at, EV_BRET, -128) == 0,
          "a scene, FOLLOW DRONE: its lowest latched root (F) the bass's baseline, its bass struck as played (C2)");
    frs_panic();
    run_ms(20);
    check(dr_are(0, 0) && out_bs == -1 && dr_twice == 0 && dr_stray == 0 && bs_stray == 0,
          "balanced: through it all, every note struck let go once");
}

/* glints on (as the voices were sent them) that neither a GLINT note struck (where it went) nor SPARKLE holds: stuck */
static int gl_unheld(void)
{
    int n, m, k = 0, own;
    for (n = 0; n < 128; n++) {
        if (!out_gl[n])
            continue;
        own = spk.note[0] == n || spk.note[1] == n;
        for (m = 0; m < 128 && !own; m++)
            own = bit_get(gl_lit, m) && m + gl_d[m] == n;
        k += !own;
    }
    return k;
}

/* SPARKLE beside GLINT LINKED (FOLLOW DRONE, C minor, the drones' C Eb G latched, its baseline, then F held: GLINT three
 * degrees up): its glint on C let go while GLINT's arpeggio plays its C (out on F), never left on */
static void sparkle_glint(void)
{
    scene_t *s = (scene_t *)sc0;
    int k, bad = 0;
    fresh();
    frs_link_set(0, FRS_LINK_GLINT);
    memset(s->ch, -1, sizeof s->ch);
    s->nch = 1;
    s->ch[0][0] = 60;
    s->ch[0][1] = 60;
    s->ch[0][2] = 63;
    s->ch[0][3] = 67;
    s->narp = 1;                         /* (GLINT's arpeggio latched on C5: played C, out on F) */
    s->arp[0] = 72;
    s->bass = -1;
    frs_scene_take(sc0);
    note(0, 65, 1);
    frs_param_set(P_SQ_TYPE, SQT_ARP);
    frs_param_set(P_ARP_RANGE, 0);
    frs_param_set(P_ARP_CHANCE, 127);
    for (k = 0; k < 3000 && !(arp.sounding == 72 && out_gl[77]); k++)   /* (its next step, after F) */
        run_ms(1);
    check(arp.sounding == 72 && out_gl[77] && dof(LK_GLINT) == 3, "SPARKLE, GLINT LINKED: its arpeggio's C out on F");
    spk.note[0] = 72;                    /* SPARKLE's glint on C, its time up now */
    spk.off[0] = eng_ms();
    frasil_glint_on(72, 0.5f);
    run_ms(6);                           /* (a block or two: the brain runs at each) */
    check(spk.note[0] < 0 && !out_gl[72] && out_gl[77],
          "SPARKLE: its glint on C let go, GLINT's arpeggio playing its C elsewhere (F)");
    frs_param_set(P_GX_SPARK, 127);      /* then SPARKLE as it plays: every glint on held by one or the other */
    for (k = 0; k < 800; k++) {
        run_ms(10);
        bad += gl_unheld() != 0;
    }
    frs_param_set(P_GX_SPARK, 0);
    run_ms(3000);
    check(bad == 0 && gl_unheld() == 0 && drops == 0, "SPARKLE and GLINT LINKED, 8 s: no glint left on");
    note(0, 65, 0);
    frs_panic();
    run_ms(20);
    /* GLINT's C held by MIDI into its arpeggio (C its player's all along): SPARKLE's C struck while the step sounds
     * (moved: F), its time up between two steps, let go (the step's let go is on F) */
    fresh();
    frs_link_set(0, FRS_LINK_GLINT);
    frs_param_set(P_SQ_TYPE, SQT_ARP);
    frs_param_set(P_ARP_RANGE, 0);
    frs_param_set(P_ARP_CHANCE, 127);
    note(0, 60, 1);                      /* (C: its baseline) */
    note(0, 60, 0);
    note(0, 65, 1);
    note(1, 72, 1);
    for (k = 0; k < 3000 && arp.sounding != 72; k++)
        run_ms(1);
    spk.note[0] = 72;
    spk.off[0] = eng_ms() + 100000u;
    frasil_glint_on(72, 0.5f);
    for (k = 0; k < 3000 && arp.sounding >= 0; k++)
        run_ms(1);
    check(arp.sounding < 0 && glint_taken(72), "SPARKLE, MIDI's C held into the arpeggio: between two steps");
    spk.off[0] = eng_ms();
    run_ms(6);
    check(spk.note[0] < 0 && !out_gl[72], "SPARKLE: its glint on C let go between the steps (the arpeggio's on F)");
    note(1, 72, 0);
    note(0, 65, 0);
    frs_panic();
    run_ms(20);
}

/* GLINT's notes onto one: a played note let go twice (two that played it), two played notes the link puts on one pitch */
static void glint_shared(void)
{
    int a, b = 0, s = -1, at;
    fresh();
    frs_link_set(0, FRS_LINK_GLINT);
    note(0, 60, 1);                      /* (C: its baseline) */
    note(0, 60, 0);
    note(0, 65, 1);                      /* (the drones on F: three up) */
    gl_on(60, 0.5f);                     /* (C twice: out on F) */
    gl_on(60, 0.5f);
    frasil_glint_on(60, 0.5f);           /* (another glint on C itself) */
    at = nlog;
    gl_off(60);
    gl_off(60);
    check(logged(at, EV_SPOFF, 65) == 1 && logged(at, EV_SPOFF, 60) == 0 && out_gl[60] && !out_gl[65],
          "GLINT: a played note let go twice, once, where it went; a glint on C itself left on");
    frasil_glint_off(60);
    for (a = 48; a < 84 && s < 0; a++)   /* (two the scale's shift puts on one: a chromatic one beside its neighbour) */
        for (b = a + 1; b < 84 && s < 0; b++)
            if (lk_note(LK_GLINT, a) == lk_note(LK_GLINT, b))
                s = lk_note(LK_GLINT, a);
    a--;
    b--;
    check(s >= 0, "GLINT: two played notes the link puts on one pitch (found)");
    if (s < 0)
        return;
    at = nlog;
    gl_on(a, 0.5f);
    gl_on(b, 0.5f);
    gl_off(a);
    check(out_gl[s] && logged(at, EV_SPOFF, s) == 0, "GLINT: two played on one pitch, the first let go: it rings on");
    gl_off(b);
    check(!out_gl[s] && logged(at, EV_SPOFF, s) == 1, "GLINT: the last let go: off, once");
    note(0, 65, 0);
    check(!gl_any(), "GLINT: every glint let go");
}

/* the most the drones hold moved at once (FOLLOW BASS, DRONE LINKED): a scene's eight latched chords and the drone's
 * sequence's eight-note step, 40 notes; none of their let go dropped, every voice on one held */
static int dr_true(void)                 /* the drones on (as the voices were sent them) are those held, where they went */
{
    int n;
    for (n = 0; n < 128; n++)
        if (out_dr[n] != (dr_snd[n] != 0))
            return 0;
    return 1;
}
static void queue_full(void)
{
    static const uint8_t ST[8] = {84, 86, 87, 89, 91, 92, 94, 96};
    scene_t *s = (scene_t *)sc0;
    uint32_t i, j, held = 0;
    int at;
    fresh();
    memset(s->ch, -1, sizeof s->ch);
    s->nch = SC_CHORDS;
    for (i = 0; i < SC_CHORDS; i++)      /* (36 .. 67, four each) */
        for (j = 0; j < 5u; j++)
            s->ch[i][j] = (int8_t)(36 + 4 * (int)i + (j ? (int)j - 1 : 0));
    s->narp = 0;
    s->bass = -1;
    frs_scene_take(sc0);
    frs_link_set(1, FRS_LINK_DRONE);
    for (i = 0; i < 4u; i++)
        frs_dseq_step(i, ST, 8);
    frs_param_set(P_TEMPO, 90);
    frs_param_set(P_DQ_RATE, 2);
    frs_param_set(P_DQ_LEN, 3);
    frs_param_set(P_DQ_ON, 1);
    frs_play_all(1);
    run_ms(150);
    for (i = 0; i < 128u; i++)
        held += held_by[i] != 0;
    check(held == 40u && dr_true(), "the most held: eight latched chords and the sequence's step, 40 notes");
    note(2, 36, 1);                      /* (the bass's C: their baseline) */
    note(2, 36, 0);
    at = nlog;
    note(2, 41, 1);                      /* (the bass on F: every one three up at once) */
    check(dof(LK_DRONE) == 3 && drops == 0 && logged(at, EV_ALLOFF, -128) == 1 && dr_true(),
          "40 drones moved at once: let go together, struck again where they go, nothing dropped");
    run_ms(1200);
    note(2, 43, 1);                      /* (G: three down, the sequence's steps going on) */
    frs_param_set(P_ROOT, 2);
    run_ms(1200);
    check(drops == 0 && dr_true(), "40 drones, the offset turned again, the steps on: every voice on held");
    frs_param_set(P_ROOT, 0);
    note(2, 43, 0);
    note(2, 41, 0);
    frs_play_all(0);
    frs_panic();
    run_ms(20);
    check(drop_offs == 0 && dr_are(0, 0), "40 drones: no let go dropped, all let go");
}

/* two tracks linked at different notes: each its own offset; linking or letting one go never moves another */
static void two_bases(void)
{
    int at;
    fresh();
    frs_mout_set(FRS_MO_CH, 4);
    frs_link_set(0, FRS_LINK_BASS);
    note(0, 60, 1);                      /* (C: the bass's baseline) */
    note(2, 36, 1);
    note(0, 60, 0);
    note(0, 65, 1);                      /* (F: the bass three up, F2) */
    at = nlog;
    frs_link_set(0, FRS_LINK_BASS | FRS_LINK_MIDI);
    run_ms(10);
    check(bof(LK_BASS) == 60 && bof(LK_MIDI) == 65 && dof(LK_BASS) == 3 && dof(LK_MIDI) == 0 && out_bs == 41 &&
              logged(at, EV_BRET, -128) == 0,
          "two linked: the bass from C (three up), MIDI LINKED on F (none): the bass not moved by it");
    nmev = 0;
    frs_mout_key(5, 60, 90, 1);
    frs_mout_key(5, 60, 0, 0);
    run_ms(5);
    check(nmev == 2 && mev[0].d1 == 60, "  MIDI's key C out as C");
    note(0, 65, 0);
    note(0, 67, 1);                      /* (G: the bass from C three down, MIDI from F one up) */
    check(dof(LK_BASS) == -3 && dof(LK_MIDI) == 1 && out_bs == 31,
          "  the drones on G: the bass three down from C (G1), MIDI one up from F");
    nmev = 0;
    frs_mout_key(5, 60, 90, 1);
    frs_mout_key(5, 60, 0, 0);
    run_ms(5);
    check(nmev == 2 && mev[0].d1 == 62 && mev[1].d1 == 62, "  MIDI's key C out as D");
    at = nlog;
    frs_link_set(0, FRS_LINK_MIDI | FRS_LINK_GLINT);   /* (the bass let go of, GLINT LINKED on G) */
    run_ms(10);
    check(out_bs == 36 && logged(at, EV_BRET, 36) == 1 && bof(LK_BASS) == -1 && bof(LK_MIDI) == 65 &&
              dof(LK_MIDI) == 1 && bof(LK_GLINT) == 67 && dof(LK_GLINT) == 0,
          "  the bass NO LINK: back as played; MIDI's own kept; GLINT LINKED on G, none");
    at = nlog;
    note(1, 60, 1);
    check(logged(at, EV_SPON, 60) == 1, "  GLINT's C out as C");
    note(1, 60, 0);
    note(0, 67, 0);
    note(0, 65, 1);                      /* (F: MIDI back on its baseline, GLINT one down) */
    check(dof(LK_MIDI) == 0 && dof(LK_GLINT) == -1, "  the drones on F: MIDI none (its F), GLINT one down (from G)");
    note(0, 65, 0);
    note(2, 36, 0);
    frs_link_set(0, 0);
    check(dr_are(0, 0) && out_bs == -1 && !gl_any() && bs_stray == 0 && gl_stray == 0, "two linked: every note let go");
}

/* FOLLOW turned: every track moved before keeps its offset (its baseline the note that puts the other's there), one
 * moved only now from the other's note; the other holding nothing, the offset kept till its first note. The one
 * followed now never moved */
static void follow_setup(uint32_t bits)  /* FOLLOW DRONE, C minor: from the drones' C to F, those moved three up */
{
    fresh();
    frs_link_set(0, bits);
    note(0, 60, 1);
    note(0, 60, 0);
}
static void follow_turned(void)
{
    static const int F[3] = {65, 68, 72};
    const uint32_t ALL3 = FRS_LINK_DRONE | FRS_LINK_BASS | FRS_LINK_GLINT;
    scene_t *s = (scene_t *)sc0;
    uint8_t b[FRS_STATE_BYTES];
    uint32_t n;
    int at, k;
    /* the bass holding C2 as FOLLOW turns to it */
    follow_setup(ALL3);
    note(2, 36, 1);
    note(0, 65, 1);
    check(out_bs == 41 && dof(LK_BASS) == 3 && dof(LK_GLINT) == 3, "FOLLOW DRONE: the bass and GLINT three up (C to F)");
    at = nlog;
    frs_link_set(1, ALL3);
    run_ms(10);
    check(lk.on == (LK_DRONE | LK_GLINT) && dof(LK_GLINT) == 3 && bof(LK_GLINT) == 31 && dof(LK_DRONE) == 0 &&
              bof(LK_DRONE) == 36 && dr_are(F, 3) && logged(at, EV_ON, -128) == 0 && logged(at, EV_OFF, -128) == 0,
          "FOLLOW turned to the bass (on C2): GLINT keeps its three up (its baseline G1), the drones from C2, not "
          "struck again");
    check(out_bs == 36, "  the bass followed now: as played (the one followed never moved)");
    at = nlog;
    note(1, 60, 1);
    check(logged(at, EV_SPON, 65) == 1, "  GLINT's C out as F, as before the turn");
    note(1, 60, 0);
    note(2, 34, 1);
    check(dof(LK_GLINT) == 2 && dof(LK_DRONE) == -1 && dr_are((const int[3]){63, 67, 70}, 3),
          "  then the bass on Bb1, one down: GLINT two up, the drones one down (Eb G Bb)");
    at = nlog;
    frs_link_set(0, ALL3);
    run_ms(10);
    check(lk.on == (LK_BASS | LK_GLINT) && dof(LK_GLINT) == 2 && bof(LK_GLINT) == 62 && dof(LK_BASS) == 0 &&
              bof(LK_BASS) == 65 && out_bs == 34 && logged(at, EV_BRET, -128) == 0,
          "FOLLOW turned back (the drones on F): GLINT keeps its two up (from D), the bass from F, not retuned (Bb1)");
    check(dr_are(F, 3), "  the drones followed now: as played");
    note(2, 34, 0);
    note(2, 36, 0);
    note(0, 65, 0);
    frs_link_set(0, 0);
    check(dr_are(0, 0) && out_bs == -1 && dr_twice == 0 && dr_stray == 0 && bs_stray == 0 && gl_stray == 0,
          "FOLLOW turned: every note let go, once");
    /* the bass holding nothing: the offset kept till its first note */
    follow_setup(ALL3);
    note(0, 65, 1);
    note(0, 65, 0);
    frs_link_set(1, ALL3);
    run_ms(10);
    check(bof(LK_GLINT) == -1 && lk.keep[2] == 3 && dof(LK_GLINT) == 3 && bof(LK_DRONE) == -1 && dof(LK_DRONE) == 0,
          "FOLLOW turned to the bass, holding nothing: GLINT keeps its three up, no baseline yet; the drones none");
    at = nlog;
    note(1, 60, 1);
    check(logged(at, EV_SPON, 65) == 1, "  GLINT's C out as F meanwhile");
    note(1, 60, 0);
    note(2, 36, 1);
    check(bof(LK_GLINT) == 31 && lk.keep[2] == 0 && dof(LK_GLINT) == 3 && bof(LK_DRONE) == 36 && dof(LK_DRONE) == 0,
          "  the bass's first note (C2): GLINT's baseline made from it (G1), its three up kept; the drones' C2");
    note(2, 34, 1);
    check(dof(LK_GLINT) == 2 && dof(LK_DRONE) == -1, "  then Bb1: GLINT two up, the drones one down");
    note(2, 34, 0);
    note(2, 36, 0);
    k = dof(LK_GLINT);
    frs_link_set(0, ALL3);               /* (back, the drones holding nothing: kept again) */
    run_ms(10);
    check(k != 0 && lk.keep[2] == k && dof(LK_GLINT) == k && bof(LK_GLINT) == -1 && bof(LK_BASS) == -1,
          "FOLLOW turned back, the drones holding nothing: GLINT's kept, the bass none");
    memset(s->ch, -1, sizeof s->ch);     /* (a scene taken: none kept) */
    s->nch = 0;
    s->narp = 0;
    s->bass = -1;
    frs_scene_take(sc0);
    run_ms(10);
    check(lk.keep[2] == 0 && dof(LK_GLINT) == 0 && bof(LK_GLINT) == -1, "a scene taken: no offset kept, no baseline");
    frs_link_set(0, 0);
    frs_panic();
    run_ms(20);
    /* told along (the FM-1's LIVE: the turn's state, frs_state_load), a listener keeps the offset as the FM-1 did; the
     * state alone (frs_state_link: a stream's start) keeps none */
    follow_setup(ALL3);
    note(0, 65, 1);
    note(0, 65, 0);
    frs_link_set(1, ALL3);
    n = frs_state_save(b);
    check(pair_of(b, n, STATE_LINK) == (int)(ALL3 | 16u) && pair_of(b, n, STATE_LKREF) < 0 &&
              pair_of(b, n, STATE_LKBASE + 2u) < 0,
          "told along: the turn's state, no note followed, no baseline (GLINT's offset kept)");
    follow_setup(ALL3);
    note(0, 65, 1);
    note(0, 65, 0);
    check(frs_state_load(b, n) == 0 && lk.follow == 1 && lk.keep[2] == 3 && dof(LK_GLINT) == 3,
          "  read along (frs_state_load): GLINT keeps its three up, as the engine that told it");
    note(2, 36, 1);
    check(bof(LK_GLINT) == 31 && dof(LK_GLINT) == 3, "  the bass's first note: its baseline made from it, as there");
    note(2, 36, 0);
    follow_setup(ALL3);
    note(0, 65, 1);
    note(0, 65, 0);
    frs_link_set(1, ALL3);
    check(frs_state_link(b, n) == 0 && lk.keep[2] == 0 && dof(LK_GLINT) == 0,
          "  the state alone (frs_state_link): none kept");
    frs_link_set(0, 0);
    frs_panic();
    run_ms(20);
}

/* a scene taken (LOAD SND+NOTES, NEW SCENE): every baseline none, the scene's first note followed theirs; LOAD SND ONLY
 * (what plays stays) keeps them */
static void scene_resets(void)
{
    scene_t *s = (scene_t *)sc0;
    uint8_t o[FRS_SCENE_BYTES];
    fresh();
    frs_link_set(0, FRS_LINK_BASS | FRS_LINK_GLINT);
    note(0, 60, 1);                      /* (C: their baseline) */
    note(0, 60, 0);
    note(0, 65, 1);
    note(0, 65, 0);
    check(dof(LK_BASS) == 3 && dof(LK_GLINT) == 3, "a scene: before it, the bass and GLINT three up (C to F)");
    memset(s->ch, -1, sizeof s->ch);     /* (one that plays nothing of its own) */
    s->nch = 0;
    s->narp = 0;
    s->bass = -1;
    frs_scene_take(sc0);
    run_ms(10);
    check(lk.on == (LK_BASS | LK_GLINT) && bof(LK_BASS) == -1 && bof(LK_GLINT) == -1 && lk.ref == -1 &&
              dof(LK_BASS) == 0 && dof(LK_GLINT) == 0,
          "a scene taken: each LINKED as it was, every baseline none, nothing followed");
    note(2, 36, 1);
    check(out_bs == 36, "  the bass as played");
    note(0, 67, 1);
    check(bof(LK_BASS) == 67 && bof(LK_GLINT) == 67 && dof(LK_BASS) == 0 && out_bs == 36,
          "  its first note followed (G) their baseline: nothing moved");
    note(0, 67, 0);
    note(0, 65, 1);
    check(dof(LK_BASS) == -1 && out_bs == 34, "  then F: one down (G to F), C2 as Bb1");
    note(0, 65, 0);
    note(2, 36, 0);
    frs_scene_new(o);                    /* NEW SCENE (as the FM-1's scene_go: frs_scene_go, CUT, SND+NOTES) */
    frs_scene_go(o);
    run_ms(10);
    check(bof(LK_BASS) == -1 && bof(LK_GLINT) == -1 && lk.ref == -1 && dof(LK_BASS) == 0, "NEW SCENE: every baseline none");
    note(0, 60, 1);
    note(0, 60, 0);
    note(0, 65, 1);
    note(0, 65, 0);
    frs_param_set(P_LOAD, 1);            /* LOAD SND ONLY: what plays stays, the baselines with it */
    frs_scene_go(sc0);
    run_ms(10);
    check(bof(LK_BASS) == bof(LK_GLINT) && bof(LK_BASS) >= 0 && dof(LK_BASS) == lk_steps(bof(LK_BASS), lk.ref) &&
              lk.ref >= 0,
          "LOAD SND ONLY: the baselines kept");
    frs_param_set(P_LOAD, 0);
    frs_link_set(0, 0);
    frs_panic();
    run_ms(20);
    check(dr_are(0, 0) && out_bs == -1 && bs_stray == 0, "a scene: every note let go");
}

static void state(void)
{
    uint8_t b0[FRS_STATE_BYTES], b1[FRS_STATE_BYTES], b2[FRS_STATE_BYTES];
    uint32_t n0, n1, n2, p, ns = 0, i, j;
    int ok;
    fresh();
    for (p = 0; p < NPARAM; p++)
        ns += param_kind(p) == PK_STATE;
    n0 = frs_state_save(b0);
    check(n0 == 5u + 2u * ns + 4u && !has_pair(b0, n0, STATE_LINK), "the state, none linked: its bytes as before (no 252)");
    for (p = STATE_LKBASE, ok = 1; p < STATE_LKBASE + LK_N; p++)
        ok &= p >= NPARAM || frs_param_kind(p) != FRS_P_STATE;
    check(ok && STATE_LKBASE == 246u, "the baselines' pairs, 246 .. 249: no setting a state holds (an engine before them "
                                      "leaves them, a LEAD setting there)");
    frs_link_set(1, FRS_LINK_DRONE | FRS_LINK_MIDI);
    n1 = frs_state_save(b1);
    check(n1 == n0 + 2u && !memcmp(b0, b1, n0) && b1[n0] == STATE_LINK && b1[n0 + 1u] == (1u | 8u | 16u),
          "the state, linked, nothing followed: the pair 252 last, its tracks and the bass followed");
    check(n1 <= FRS_STATE_BYTES, "the state fits FRS_STATE_BYTES");
    fresh();
    check(frs_link_get() == 0u, "an engine opened: none linked");
    check(frs_state_load(b1, n1) == 0 && frs_link_get() == (FRS_LINK_DRONE | FRS_LINK_MIDI | FRS_LINK_FOLLOW_BASS),
          "the state read back: the link as saved");
    check(lk.ref == -1 && dof(LK_DRONE) == 0 && bof(LK_DRONE) == -1 && bof(LK_MIDI) == -1,
          "the state without its note followed (251): from what the bass holds (nothing: none), no baseline");
    check(frs_state_load(b0, n0) == 0 && (frs_link_get() & 15u) == 0u, "a state without the pair: none linked");
    b1[n0 + 1u] = 4u;                    /* (GLINT, the drone followed) */
    check(frs_state_load(b1, n1) == 0 && frs_link_get() == FRS_LINK_GLINT, "the state: the drone followed");
    /* the one followed's LINKED alone: none moved, no pair */
    fresh();
    frs_link_set(1, FRS_LINK_BASS);
    check(frs_state_save(b1) == n0 && lk.on == 0u && frs_link_get() == (FRS_LINK_BASS | FRS_LINK_FOLLOW_BASS),
          "the state, the bass followed and its own LINKED alone: none moved, the bytes as before");
    /* the note followed (251) and each baseline (246 ..): an engine opened on the scene and the state moves the tracks
     * as this one */
    fresh();
    frs_link_set(1, FRS_LINK_DRONE | FRS_LINK_GLINT);
    note(2, 36, 1);                      /* (C2: their baselines) */
    note(2, 36, 0);
    note(2, 41, 1);                      /* (F: three up) */
    frs_link_set(1, FRS_LINK_DRONE | FRS_LINK_GLINT | FRS_LINK_MIDI);   /* (MIDI LINKED on F: none) */
    n1 = frs_state_save(b1);
    check(n1 == n0 + 10u && b1[n0] == STATE_LINK && b1[n0 + 1u] == (1u | 4u | 8u | 16u) && b1[n0 + 2u] == STATE_LKREF &&
              b1[n0 + 3u] == 41u && b1[n0 + 4u] == STATE_LKBASE && b1[n0 + 5u] == 36u &&
              b1[n0 + 6u] == STATE_LKBASE + 2u && b1[n0 + 7u] == 36u && b1[n0 + 8u] == STATE_LKBASE + 3u &&
              b1[n0 + 9u] == 41u,
          "the state, the bass on F followed: 251 its note after 252, then each baseline (the drone's, GLINT's: C2; MIDI's F)");
    frs_lead_midi_set(9, 1);             /* (the most: ATTACK SHAPE's and LEAD CH's pairs with them) */
    frs_ashape_set(1u);
    n2 = frs_state_save(b2);
    check(n2 == n1 + 4u && n2 <= FRS_STATE_BYTES && pair_of(b2, n2, STATE_LKBASE + 3u) == 41,
          "the state at its most (three baselines, ATTACK SHAPE's, LEAD CH's): in FRS_STATE_BYTES");
    note(2, 41, 0);
    fresh();
    check(frs_state_load(b1, n1) == 0 && lk.ref == 41 && bof(LK_DRONE) == 36 && bof(LK_GLINT) == 36 &&
              bof(LK_MIDI) == 41 && dof(LK_DRONE) == 3 && dof(LK_GLINT) == 3 && dof(LK_MIDI) == 0,
          "read back: the note followed, each baseline, each offset at once");
    note(0, 60, 1);
    check(dr_are((const int[3]){65, 68, 72}, 3), "read back: the drones moved as they were (C as F, nothing on the bass)");
    note(0, 60, 0);
    note(2, 43, 1);
    check(dof(LK_DRONE) == -3 && dof(LK_MIDI) == 1, "read back: then the bass on G, each from its own baseline");
    note(2, 43, 0);
    /* a state without the baselines (written before them): the link and its note, each baseline none, the next note
     * followed theirs */
    for (i = j = 5; i + 1u < n1; i += 2u)
        if (b1[i] < STATE_LKBASE || b1[i] >= STATE_LKBASE + LK_N) {
            b2[j] = b1[i];
            b2[j + 1u] = b1[i + 1u];
            j += 2u;
        }
    memcpy(b2, b1, 5);
    n2 = j;
    fresh();
    check(n2 == n0 + 4u && frs_state_load(b2, n2) == 0 &&
              frs_link_get() == (FRS_LINK_DRONE | FRS_LINK_GLINT | FRS_LINK_MIDI | FRS_LINK_FOLLOW_BASS) && lk.ref == 41 &&
              bof(LK_DRONE) == -1 && dof(LK_DRONE) == 0,
          "a state without the baselines: linked as saved, each baseline none, nothing moved");
    note(2, 43, 1);
    check(bof(LK_DRONE) == 43 && bof(LK_MIDI) == 43 && dof(LK_DRONE) == 0, "  the next note followed (G) their baseline");
    note(2, 43, 0);
    b1[n0 + 3u] = 200u;                  /* (no note: as without it) */
    b1[n0 + 5u] = 200u;                  /* (no baseline: as without it) */
    fresh();
    check(frs_state_load(b1, n1) == 0 && lk.ref == -1 && bof(LK_DRONE) == -1 && dof(LK_DRONE) == 0 &&
              bof(LK_MIDI) == 41,
          "a note followed or a baseline past 127: as none");
    check(frs_state_link(b0, 4) == -1 && frs_state_link(b0, n0) == 0 && (frs_link_get() & 15u) == 0u,
          "frs_state_link: not a state, -1; a state, its link alone");
}

/* a stream's start (fst_fresh: its state, then its scene, which leaves every baseline none): the tracks moved as the
 * state has them */
static void stream_start(void)
{
    static const int F[3] = {65, 68, 72};
    scene_t *s = (scene_t *)sc0;
    static fst_start fs0;
    fresh();
    memset(s->ch, -1, sizeof s->ch);
    s->nch = 1;
    s->ch[0][0] = 60;
    s->ch[0][1] = 60;
    s->ch[0][2] = 63;
    s->ch[0][3] = 67;
    s->narp = 0;
    s->bass = -1;
    frs_scene_take(sc0);                 /* (its C chord latched) */
    frs_link_set(1, FRS_LINK_DRONE);
    note(2, 36, 1);                      /* (C2: the drones' baseline) */
    note(2, 41, 1);                      /* (F: three up) */
    run_ms(20);
    check(dr_are(F, 3), "a stream's start: here, the drones' C chord out three up (F Ab C)");
    fst_capture(&fs0, 0);
    note(2, 41, 0);
    note(2, 36, 0);
    out_reset();
    check(fst_fresh(&fs0, 44100.0f) == 0, "a stream's start: an engine opened on it");
    run_ms(20);
    check(frs_link_get() == (FRS_LINK_DRONE | FRS_LINK_FOLLOW_BASS) && lk.ref == 41 && bof(LK_DRONE) == 36 &&
              dof(LK_DRONE) == 3 && dr_are(F, 3),
          "a stream's start: the state's baseline and note after its scene, the drones where they were (F Ab C)");
    frs_panic();
    run_ms(20);
}

static void running(void)
{
    static uint8_t buf[FRS_LIVE_BYTES];
    static const int F[3] = {65, 68, 72};
    uint32_t n;
    int8_t d[128];
    uint8_t snd[128], gl[16];
    fresh();
    frs_link_set(1, FRS_LINK_DRONE | FRS_LINK_GLINT);
    note(2, 36, 1);                      /* (C2: their baselines) */
    note(2, 36, 0);
    note(2, 41, 1);
    frs_link_set(1, FRS_LINK_DRONE | FRS_LINK_GLINT | FRS_LINK_MIDI);   /* (MIDI's on F) */
    note(0, 60, 1);
    note(1, 60, 1);
    memcpy(d, dr_d, sizeof d);
    memcpy(snd, dr_snd, sizeof snd);
    memcpy(gl, gl_lit, sizeof gl);
    n = frs_live_save(buf, sizeof buf, 0);
    check(n > 0u, "the running state saved");
    memset(lk.d, 0, sizeof lk.d);        /* (as another engine's) */
    memset(lk.base, -1, sizeof lk.base);
    memset(lk.keep, 0, sizeof lk.keep);
    memset(dr_d, 0, sizeof dr_d);
    memset(dr_snd, 0, sizeof dr_snd);
    memset(gl_lit, 0, sizeof gl_lit);
    check(frs_live_load(buf, n, 0) == 0, "the running state read back");
    check(lk.bits == (FRS_LINK_DRONE | FRS_LINK_GLINT | FRS_LINK_MIDI) && lk.follow == 1 && lk.ref == 41 &&
              dof(LK_DRONE) == 3 && dof(LK_GLINT) == 3 && dof(LK_MIDI) == 0 && bof(LK_DRONE) == 36 &&
              bof(LK_GLINT) == 36 && bof(LK_MIDI) == 41 && !memcmp(d, dr_d, sizeof d) &&
              !memcmp(snd, dr_snd, sizeof snd) && !memcmp(gl, gl_lit, sizeof gl),
          "the running state: the link, each baseline and offset, the drones where they sound, GLINT's where they went");
    run_ms(20);
    check(dr_are(F, 3), "the running state: nothing struck again");
    note(0, 60, 0);
    note(1, 60, 0);
    note(2, 41, 0);
    check(!out_dr[65] && !out_dr[68] && !out_dr[72] && !out_gl[65], "the running state: let go where they went");
}

int main(void)
{
    degrees();
    drone_leads();
    glint_follows();
    midi_follows();
    midi_engine();
    bass_leads();
    balanced();
    sparkle_glint();
    glint_shared();
    queue_full();
    two_bases();
    follow_turned();
    scene_resets();
    state();
    stream_start();
    running();
    frs_close();
    printf("link: %d ok, %d FAIL\n", oks, fails);
    return fails != 0;
}
