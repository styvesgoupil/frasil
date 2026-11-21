/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The engine alone from the inside (frasil_engine.c's statics): its clock past 49.7 days (the milliseconds wrap, the
 * render goes on), a note under the scale's lowest (no root: nothing latched, nothing written outside), the function
 * statics that carried time started again with it; the LFOs on the LFOs (card #199: xmod_test). Built under AddressSanitizer and UBSan too (engine/tests/run.sh).
 *   engine/tests/run.sh, or: inside */
#include <stdio.h>
#include <math.h>
#include "../frasil_engine.c"

static float L[4096], R[4096];
static int fails;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "  ok" : "FAIL", what);
    fails += !ok;
}

static double rms(uint32_t n)
{
    double e = 0.0;
    uint32_t done = 0, i;
    while (done < n) {
        uint32_t k = n - done < 4096u ? n - done : 4096u;
        frs_render(L, R, k);
        for (i = 0; i < k; i++)
            e += (double)L[i] * L[i];
        done += k;
    }
    return sqrt(e / n);
}

/* the swell held over a chord, from an open: its samples' hash */
static uint64_t swell_run(void)
{
    uint64_t h = 0xCBF29CE484222325u;
    int32_t lr[2 * 4096];
    uint32_t k, j;
    frs_init(44100.0f);
    frs_swell(1);
    frs_latch(0, 48, FRS_CHORD_HARMONY);
    for (k = 0; k < 40u; k++) {
        frs_render_i24(lr, 4096);
        for (j = 0; j < 8192u; j++) {
            h ^= (uint32_t)lr[j];
            h *= 0x100000001B3u;
        }
    }
    frs_close();
    return h;
}

/* the tracks (frs_track_play, frs_play_all): a moment, nothing edited; a sequence started again in phase with the
 * other's (the two the same: 16 sixteenths each, so in phase is the same step) */
static uint32_t bits4(const uint32_t *b)
{
    uint32_t n = 0, i;
    for (i = 0; i < 128u; i++)
        n += b[i >> 5] >> (i & 31u) & 1u;
    return n;
}

/* blocks rendered for `ms`: those where GLINT's step and the bass's both play and differ (*both: where both play) */
static uint32_t apart(uint32_t ms, uint32_t *both)
{
    frs_view_t v;
    uint32_t k, n = ms * 48u / 256u, bad = 0;
    *both = 0;
    for (k = 0; k < n; k++) {
        frs_render(L, R, 256);
        frs_view(&v);
        if (v.seq_pos >= 0 && v.bseq_pos >= 0) {
            (*both)++;
            bad += v.seq_pos != v.bseq_pos;
        }
    }
    return bad;
}

/* in phase whatever the loops' lengths: a run begun by frs_play_all, one track stopped at block `a` and played again
 * at `b`, against the same run never stopped, block by block once it plays again (each one's position: GLINT's step,
 * the clocks into the bass's loop) */
#define PH_N 2000u
static float ph_b[PH_N];
static int ph_g[PH_N];
static void ph_setup(uint32_t glen, uint32_t rate, uint32_t bsteps, uint32_t blenidx)
{
    uint32_t i;
    frs_close();
    frs_init(48000.0f);
    param_set(P_TEMPO, 90);              /* (120 BPM) */
    param_set(P_ARP_RATE, (int32_t)rate);
    param_set(P_ARP_CHANCE, 127);
    for (i = 0; i < SC_SEQ; i++)
        seq[i] = (uint8_t)(60u + i % 12u);
    param_set(P_SQ_LEN, (int32_t)glen - 1);
    param_set(P_SQ_TYPE, SQT_SEQ);
    for (i = 0; i < SC_BSEQ; i++)
        bnote[i] = (uint8_t)(36u + i);
    param_set(P_BQ_LEN, (int32_t)blenidx);
    param_set(P_BQ_STEPS, (int32_t)bsteps - 1);
    param_set(P_BQ_ON, (int32_t)SQT_SEQ);
    frs_play_all(0);
    frs_play_all(1);
}
static uint32_t ph_event;                 /* (a block where a scene is taken again, both runs: 0 none) */
static uint8_t ph_sc[FRS_SCENE_BYTES];
static uint32_t ph_run(uint32_t t, uint32_t a, uint32_t b, int ref)
{
    uint32_t k, bad = 0;
    for (k = 0; k < PH_N; k++) {
        if (ph_event && k == ph_event) {
            frs_scene_store(ph_sc, 0);
            frs_scene_take(ph_sc);       /* (both sequences from their tops again, between two milliseconds) */
        }
        if (!ref && k == a)
            frs_track_play(t, 0);
        if (!ref && k == b)
            frs_track_play(t, 1);
        frs_render(L, R, 256);
        if (ref) {
            ph_b[k] = bq.run && bq.pos >= 0 ? bseq_at() : -1.0f;
            ph_g[k] = sq.run ? sq.pos : -1;
        } else if (k > b + 400u) {       /* (once both play again: a bass step may be a bar) */
            float x = bq.run && bq.pos >= 0 ? bseq_at() : -1.0f;
            bad += fabsf(x - ph_b[k]) > 0.6f || (sq.run ? sq.pos : -1) != ph_g[k];
        }
    }
    return bad;
}
static int in_phase(uint32_t glen, uint32_t rate, uint32_t bsteps, uint32_t blenidx, uint32_t t, uint32_t a, uint32_t b)
{
    ph_setup(glen, rate, bsteps, blenidx);
    ph_run(0, 0, 0, 1);
    ph_setup(glen, rate, bsteps, blenidx);
    return ph_run(t, a, b, 0) == 0;
}

static void tracks(void)
{
    scene_t sc;
    uint8_t b[FRS_SCENE_BYTES], s2[FRS_SCENE_BYTES], st0[FRS_STATE_BYTES] = {0}, st1[FRS_STATE_BYTES] = {0};
    frs_view_t v;
    uint32_t i, both, n0, n1;
    int d0;
    frs_init(48000.0f);
    frs_scene_new(b);
    memcpy(&sc, b, sizeof sc);
    sc.v[P_SQ_TYPE] = SQT_SEQ;
    sc.v[P_ARP_RATE] = 7;                /* (sixteenths: ARP_TICKS[7]) */
    sc.v[P_ARP_CHANCE] = 127;
    sc.v[P_SQ_LEN] = 15;                 /* (16 steps) */
    for (i = 0; i < SC_SEQ; i++)
        sc.seq[i] = (uint8_t)(i % 4u ? SQ_REST : 72u);
    for (i = 0; i < SC_BSEQ; i++)
        sc.bnote[i] = (uint8_t)(i % 4u ? SQ_TIE : 36u);
    sc.bq_on = SQT_SEQ;
    sc.bq_steps = 15;
    sc.bq_len = 0;                       /* (a sixteenth: BQ_TICKS[0]) */
    sc.nch = 2;
    {
        static const int8_t C0[5] = {48, 48, 52, 55, -1}, C1[5] = {55, 55, 59, 62, -1};
        memcpy(sc.ch[0], C0, 5);
        memcpy(sc.ch[1], C1, 5);
    }
    memcpy(b, &sc, sizeof sc);
    frs_scene_take(b);
    frs_view(&v);
    check(v.playing == 7u && bits4(v.latched) == 2u, "a scene taken: every track playing, its two chords");
    check(apart(1000, &both) == 0 && both > 100u, "GLINT's sequence and the bass's: the same step");
    d0 = frs_dirty();
    frs_state_save(st0);

    frs_track_play(1, 0);                /* GLINT stopped, the bass on */
    frs_view(&v);
    check(!v.seq_run && v.bseq_run && v.playing == 5u, "GLINT stopped: its sequence alone");
    apart(370, &both);
    check(both == 0, "  it plays nothing meanwhile");
    frs_midi(0xFA, 0, 0);                /* a MIDI Start: the bass from its top, GLINT still stopped */
    apart(250, &both);
    frs_view(&v);
    check(!v.seq_run && v.bseq_run && v.bseq_pos >= 0, "  a MIDI Start: the bass from the top, GLINT stays stopped");
    frs_scene_store(s2, b);
    check(s2[offsetof(scene_t, v) + P_SQ_TYPE] == SQT_SEQ &&
              !memcmp(s2 + offsetof(scene_t, seq), b + offsetof(scene_t, seq), SC_SEQ) && frs_dirty() == 0,
          "  stored meanwhile: its sequence on, as it was");
    frs_scene_take(b);                   /* (as before: the scene, untouched) */
    apart(530, &both);
    frs_track_play(1, 0);
    apart(410, &both);
    frs_track_play(1, 1);
    check(apart(2000, &both) == 0 && both > 300u, "GLINT again: in phase with the bass's, at once");

    frs_track_play(2, 0);                /* the bass stopped, GLINT on */
    frs_view(&v);
    check(v.seq_run && !v.bseq_run && v.playing == 3u && v.bass_kept < 0, "the bass stopped: GLINT's alone");
    frs_host_tempo(120.0, 0.0, 1);       /* a host plays, then locates: the bass stays stopped */
    apart(500, &both);
    frs_host_tempo(120.0, 9.0, 1);
    apart(500, &both);
    frs_view(&v);
    check(!v.bseq_run && v.seq_run, "  a host's start and locate: the bass stays stopped");
    frs_track_play(2, 1);
    check(apart(2000, &both) == 0 && both > 300u, "the bass again: in phase with GLINT's");
    frs_host_tempo(0.0, 0.0, 0);

    frs_play_all(0);
    frs_view(&v);
    check(v.playing == 0u && !v.seq_run && !v.bseq_run && bits4(v.latched) == 0u && bits4(v.kept) == 2u,
          "everything stopped: the chords kept");
    n0 = frs_state_save(st1);
    check(frs_dirty() == d0 && !memcmp(st0, st1, n0), "  nothing edited, nothing in the state");
    frs_scene_store(s2, b);
    check(s2[offsetof(scene_t, nch)] == 2u && s2[offsetof(scene_t, ch)] == 48 && s2[offsetof(scene_t, ch) + 5u] == 55,
          "  stored meanwhile: its two chords");
    apart(700, &both);
    frs_play_all(1);
    frs_view(&v);
    check(v.playing == 7u && bits4(v.latched) == 2u && bits4(v.kept) == 0u, "everything again: the chords latched");
    for (i = 0; i < 40u; i++) {          /* the sequences from their first steps, together */
        frs_render(L, R, 256);
        frs_view(&v);
        if (v.seq_pos >= 0 || v.bseq_pos >= 0)
            break;
    }
    check(v.seq_pos == 0 && v.bseq_pos == 0, "  the sequences from their first steps together");
    check(apart(1500, &both) == 0, "  in phase");

    frs_track_play(0, 0);
    frs_latch(0, 60, FRS_CHORD_HARMONY);
    frs_view(&v);
    check(v.playing & 1u && bits4(v.latched) == 1u && bits4(v.kept) == 0u && (v.latched[1] >> (60 - 32) & 1u),
          "a chord latched on the stopped drones: they play with it alone");
    frs_track_play(0, 0);
    frs_clear(0);
    frs_track_play(0, 1);
    frs_view(&v);
    check(bits4(v.latched) == 0u && v.playing & 1u, "CLEAR on the stopped drones: nothing kept to play again");

    sc.bq_on = 0;                        /* the bass's note held */
    sc.bass = 40;
    memcpy(b, &sc, sizeof sc);
    frs_scene_take(b);
    frs_view(&v);
    n0 = (uint32_t)v.bass;
    frs_track_play(2, 0);
    frs_view(&v);
    check(v.bass < 0 && v.bass_kept == (int8_t)n0 && !(v.playing & 4u), "the bass's note stopped: kept");
    frs_scene_store(s2, b);
    check((int8_t)s2[offsetof(scene_t, bass)] == (int8_t)n0, "  stored meanwhile: its note");
    frs_track_play(2, 1);
    frs_view(&v);
    check(v.bass == (int8_t)n0 && v.bass_kept < 0 && v.playing & 4u, "  again: it plays");
    frs_track_play(1, 0);
    frs_track_play(2, 0);
    frs_release_all();
    frs_view(&v);
    check(v.playing & 1u && !bits4(v.kept) && v.bass_kept < 0 && !v.seq_run && !v.bseq_run,
          "RELEASE ALL: nothing kept, nothing stopped by the player (its sequences stopped, as PLAY held has them)");

    /* in phase whatever the loops: GLINT a bar of sixteenths, the bass two bars of eighths; GLINT 12 eighths (a bar and a
     * half), the bass a bar; the bass three dotted quarters */
    check(in_phase(16, 7, 16, 1, 2, 300, 700) && in_phase(16, 7, 16, 1, 2, 300, 1100),
          "in phase: GLINT a bar, the bass two, the bass again (twice)");
    check(in_phase(32, 7, 16, 0, 1, 300, 700) && in_phase(32, 7, 16, 0, 1, 300, 1100),
          "in phase: GLINT two bars, the bass one, GLINT again (twice)");
    check(in_phase(12, 5, 16, 0, 1, 300, 900) && in_phase(12, 5, 16, 0, 2, 300, 900),
          "in phase: GLINT a bar and a half, the bass a bar, either again");
    check(in_phase(16, 7, 9, 1, 2, 300, 900), "in phase: the bass nine eighths (108 clocks, GLINT's 96), again");
    ph_event = 375;                      /* (GLINT's third loop's top at 120 BPM: a bar a 375 blocks) */
    check(in_phase(16, 7, 16, 1, 2, 500, 900) && in_phase(16, 7, 16, 1, 1, 500, 900),
          "in phase after a scene taken again at GLINT's loop's top (both begun again there)");
    ph_event = 0;
    frs_close();
    frs_init(48000.0f);

    /* only what is latched is kept: a bass note MIDI holds, a key's in HOLD, sound on and come back never */
    frs_midi(0x90, 40, 100);
    rms(4800);
    frs_track_play(2, 0);
    frs_midi(0x80, 40, 0);
    rms(4800);
    frs_track_play(2, 1);
    rms(48000);
    frs_view(&v);
    check(v.bass < 0 && v.bass_kept < 0, "a MIDI note on the stopped bass: not kept, nothing stuck once let go");
    param_set(P_KB, 2);                  /* (the keys on the bass, in HOLD) */
    param_set(P_KEYS, 1);
    frs_key_down(0, 45, 100, 0);
    frs_track_play(2, 0);
    frs_key_up(0);
    frs_track_play(2, 1);
    rms(48000);
    frs_view(&v);
    check(v.bass < 0 && v.bass_kept < 0, "a key held in HOLD on the stopped bass: the same");
    frs_latch(2, 52, FRS_CHORD_HARMONY); /* (KEY MODE HOLD still: a note latched) */
    frs_view(&v);
    n0 = (uint32_t)v.bass;
    frs_track_play(2, 0);
    frs_view(&v);
    check(v.bass < 0 && v.bass_kept == (int8_t)n0, "in KEY MODE HOLD, a latched bass note: stopped, kept");
    frs_track_play(2, 1);
    param_set(P_KEYS, 0);
    param_set(P_KB, 0);
    frs_clear(2);
    frs_midi(0x92, 36, 100);             /* MIDI holds one (the bass's channel, 3), a note latched over it */
    rms(480);
    frs_latch(2, 55, FRS_CHORD_HARMONY);
    frs_view(&v);
    n0 = (uint32_t)v.bass;
    frs_track_play(2, 0);
    frs_view(&v);
    n1 = (uint32_t)v.bass;
    frs_track_play(2, 1);
    frs_view(&v);
    check(v.bass_kept < 0 && (int8_t)n1 != (int8_t)n0 && n1 < 128u && v.bass == (int8_t)n0,
          "over a MIDI note: the latched one stopped and kept (MIDI's sounds), then again");
    frs_midi(0x82, 36, 0);
    frs_clear(2);
    rms(480);

    /* GLINT's arpeggio stopped: a HOLD sounds its glint alone, the arpeggio waiting on */
    param_set(P_SQ_TYPE, SQT_ARP);
    frs_latch(1, 72, FRS_CHORD_HARMONY);
    frs_latch(1, 76, FRS_CHORD_HARMONY);
    rms(48000);
    frs_track_play(1, 0);
    n0 = arp.steps;
    frs_hold(1, 79, 1, FRS_CHORD_HARMONY);
    rms(24000);
    frs_hold(1, 79, 0, FRS_CHORD_HARMONY);
    rms(96000);
    frs_view(&v);
    check(arp.steps == n0 && arp.paused && !(v.playing & 2u) && v.narp == 2u,
          "a HOLD on GLINT's stopped arpeggio: it waits on, its two notes kept");
    param_set(P_GL_SUSTAIN, 100);
    frs_hold(1, 84, 1, FRS_CHORD_HARMONY);
    rms(4800);
    param_set(P_GL_OCTAVE, st.v[P_GL_OCTAVE] + 1u);   /* (OCTAVE turned while it is held) */
    frs_hold(1, 84, 0, FRS_CHORD_HARMONY);
    rms(4800);
    {
        uint32_t g;
        int on = 0;
        for (g = 0; g < 128u; g++)
            on |= hold_g[g] >= 0;
        check(!on, "  OCTAVE turned while held: let go all the same (none left held)");
    }

    /* All Sound Off, KEY MODE: what a stopped track keeps goes */
    frs_latch(0, 48, FRS_CHORD_HARMONY);
    frs_track_play(0, 0);
    frs_midi(0xB0, 120, 0);
    rms(480);
    frs_view(&v);
    check(!bits4(v.kept) && v.playing & 1u, "MIDI All Sound Off: nothing kept, nothing stopped");
    frs_latch(0, 48, FRS_CHORD_HARMONY);
    frs_track_play(0, 0);
    frs_param_set(P_KEYS, 1);
    frs_param_set(P_KEYS, 0);
    frs_track_play(0, 1);
    frs_view(&v);
    check(!bits4(v.kept) && !bits4(v.latched), "KEY MODE turned: the kept chords go, as the latched ones do");

    /* the chord a latch or a hold makes (CHORD itself as it is) */
    frs_scene_new(b);
    frs_scene_take(b);
    param_set(P_CHORD, 2);               /* TRIAD */
    d0 = frs_dirty();
    frs_latch(0, 60, 0);
    frs_view(&v);
    n0 = bits4(v.held);
    frs_latch(0, 64, FRS_CHORD_HARMONY);
    frs_view(&v);
    n1 = bits4(v.held);
    frs_latch(0, 67, 12);                /* DOM7 */
    frs_view(&v);
    check(n0 == 1u && n1 == 4u && bits4(v.held) == 8u, "latched: SINGLE one note, HARMONY's TRIAD three, DOM7 four");
    check(st.v[P_CHORD] == 2u && frs_dirty() == d0, "  CHORD as it was set, nothing edited");
    frs_hold(0, 96, 1, 0);
    frs_view(&v);
    n0 = bits4(v.held);
    frs_hold(0, 96, 0, 0);
    frs_view(&v);
    check(n0 == 9u && bits4(v.held) == 8u, "held: SINGLE, one note while down");
    frs_close();
}

/* THE KEYS' HOLD with several fingers: a latch, CLEAR, an edit let go what they act on alone; two fingers on a note;
 * the bass's fingers in their order; a scene stored without what is held; a scene taken forgets the fingers */
#define H FRS_CHORD_HARMONY
static void fingers_test(void)
{
    frs_view_t v;
    uint8_t b[FRS_SCENE_BYTES], s2[FRS_SCENE_BYTES];
    uint32_t n0;
    int oct;
    frs_init(48000.0f);
    frs_scene_new(b);
    frs_scene_take(b);
    param_set(P_CHORD, 2);               /* TRIAD */
    frs_hold(0, 72, 1, H);
    frs_view(&v);
    n0 = bits4(v.held);
    frs_latch(0, 48, H);
    frs_view(&v);
    check(n0 == 3u && bits4(v.held) == 6u && bits4(v.latched) == 1u,
          "a chord latched while a finger holds another: both sound");
    frs_clear(0);
    frs_view(&v);
    check(bits4(v.held) == 3u && !bits4(v.latched), "  CLEAR: the latched one goes, the held one sounds on");
    frs_param_edit(P_SQ_STEP, 3);        /* (an edit, as the River's editor makes it) */
    frs_view(&v);
    check(bits4(v.held) == 3u, "  an edit: it sounds on");
    frs_hold(0, 72, 1, H);               /* a second finger on it */
    frs_hold(0, 72, 0, H);
    frs_view(&v);
    check(bits4(v.held) == 3u, "two fingers on a note: the first up, it sounds on");
    frs_hold(0, 72, 0, H);
    frs_view(&v);
    check(bits4(v.held) == 0u, "  the last up: gone");

    param_set(P_SQ_TYPE, SQT_ARP);       /* GLINT's arpeggio */
    oct = 12 * (int)st.v[P_GL_OCTAVE];
    frs_hold(1, 72, 1, H);
    frs_latch(1, 76, H);
    frs_view(&v);
    check(v.narp == 2u, "GLINT: a note latched while a finger holds one: both in the arpeggio");
    frs_hold(1, 72, 0, H);
    frs_view(&v);
    check(v.narp == 1u && v.arp[0] == 76 - oct, "  the finger up: the latched one stays");
    frs_hold(1, 79, 1, H);
    check(frs_latch(1, 79, H) == 1, "  a note a finger holds, latched");
    frs_hold(1, 79, 0, H);
    frs_view(&v);
    check(v.narp == 2u, "  it stays once the finger lets go");
    frs_hold(1, 84, 1, H);
    frs_scene_store(s2, 0);
    frs_view(&v);
    check(s2[offsetof(scene_t, narp)] == 2u && v.narp == 3u, "a scene stored while a finger holds a note: the latched ones alone");
    frs_clear(1);
    frs_view(&v);
    check(v.narp == 1u && v.arp[0] == 84 - oct, "  CLEAR: the latched ones gone, the held one in the arpeggio");
    frs_hold(1, 84, 0, H);
    frs_view(&v);
    check(v.narp == 0u, "  its finger up: none");
    param_set(P_SQ_TYPE, 0);

    frs_hold(2, 40, 1, H);               /* the bass: three fingers */
    frs_hold(2, 43, 1, H);
    frs_hold(2, 47, 1, H);
    frs_view(&v);
    check(v.bass == 47, "the bass, three fingers: the last one's note");
    frs_hold(2, 47, 0, H);
    frs_view(&v);
    check(v.bass == 43, "  its finger up: the last still down plays");
    frs_hold(2, 40, 0, H);
    frs_view(&v);
    check(v.bass == 43, "  another's up: it plays on");
    frs_latch(2, 52, H);
    frs_view(&v);
    check(v.bass == 52, "  a note latched over them: it plays");
    frs_hold(2, 43, 0, H);
    frs_view(&v);
    check(v.bass == 52, "  the fingers up: the latched one plays on");
    frs_hold(2, 50, 1, H);
    frs_scene_store(s2, 0);
    check((int8_t)s2[offsetof(scene_t, bass)] == 52, "  stored while a finger holds another: the latched one");
    frs_hold(2, 50, 0, H);
    frs_view(&v);
    check(v.bass == 52, "  its finger up: back to the latched one");
    frs_hold(2, 55, 1, H);
    check(frs_latch(2, 55, H) == 1, "  the note a finger holds, latched");
    frs_hold(2, 55, 0, H);
    frs_view(&v);
    check(v.bass == 55, "  it stays once the finger lets go");
    frs_hold(2, 59, 1, H);
    frs_clear(2);
    frs_view(&v);
    check(v.bass == 59, "  CLEAR: the latched one gone, the held one sounds on");
    frs_hold(2, 59, 0, H);
    frs_view(&v);
    check(v.bass < 0, "  its finger up: silent");
    frs_hold(2, 40, 1, H);               /* a latch over the fingers' note, latched again: theirs sounds again */
    frs_latch(2, 47, H);
    check(frs_latch(2, 47, H) == 0, "  a note latched over them, latched again: let go");
    frs_view(&v);
    check(v.bass == 40, "  the fingers' note sounds again");
    frs_hold(2, 43, 1, H);
    frs_hold(2, 43, 0, H);
    frs_hold(2, 40, 0, H);
    frs_view(&v);
    check(v.bass < 0, "  the fingers up: nothing comes back (it was let go)");
    frs_latch(2, 36, H);                 /* the note under the fingers', latched again: let go */
    frs_hold(2, 41, 1, H);
    check(frs_latch(2, 36, H) == 0, "  the note under the fingers', latched again: let go");
    frs_view(&v);
    check(v.bass == 41, "  the fingers' note sounds on");
    frs_hold(2, 41, 0, H);
    frs_view(&v);
    check(v.bass < 0, "  the finger up: silent (nothing under it now)");
    frs_hold(2, 40, 1, H);
    frs_hold(2, 43, 1, H);
    frs_scene_take(b);
    frs_hold(2, 43, 0, H);
    frs_hold(2, 40, 0, H);
    frs_view(&v);
    check(v.bass < 0, "a scene taken: the fingers forgotten, their ups do nothing");
    frs_hold(0, 72, 1, H);
    frs_view(&v);
    check(bits4(v.held) > 0u, "  a finger again on a note held then: it sounds");
    frs_hold(0, 72, 0, H);

    /* KEY MODE turned while a finger holds a chord: forgotten there, no count off for the chords after */
    param_set(P_CHORD, 2);
    frs_hold(0, 55, 1, H);
    frs_param_set(P_KEYS, 1);
    frs_param_set(P_KEYS, 0);
    frs_latch(0, 48, H);                 /* (TRIAD at 48: 48, 52, 55, as the held one's 55) */
    frs_hold(0, 55, 0, H);
    frs_view(&v);
    check(v.held[1] >> (55 - 32) & 1u, "KEY MODE turned under a finger: its up cuts nothing of a chord latched after");
    frs_clear(0);
    frs_hold(0, 60, 1, H);
    frs_midi(0xB0 | (st.v[P_CANAL] - 1u), 123, 0);   /* (All Notes Off on the drones' channel) */
    rms(480);
    frs_latch(0, 60, H);
    frs_hold(0, 60, 0, H);
    frs_view(&v);
    check(bits4(v.held) == 3u, "  All Notes Off under a finger: the same");
    frs_clear(0);

    /* an up lost: frs_hold_off; OCTAVE turned between a down and its up */
    frs_hold(0, 64, 1, H);
    frs_hold(0, 67, 1, H);
    frs_hold(2, 40, 1, H);
    frs_hold(2, 45, 1, H);
    frs_hold_off(0);
    frs_hold_off(2);
    frs_view(&v);
    check(!bits4(v.held) && v.bass < 0, "frs_hold_off: every finger up, the drones' and the bass's");
    param_set(P_SQ_TYPE, SQT_ARP);
    frs_hold(1, 72, 1, H);
    param_set(P_GL_OCTAVE, st.v[P_GL_OCTAVE] ? st.v[P_GL_OCTAVE] - 1u : 1u);
    frs_hold(1, 72, 0, H);
    frs_view(&v);
    check(v.narp == 0u, "OCTAVE turned while a finger holds GLINT's note: its up lets it go");
    param_set(P_SQ_TYPE, 0);
    frs_hold(2, 40, 1, H);
    param_set(P_BS_OCTAVE, st.v[P_BS_OCTAVE] ? st.v[P_BS_OCTAVE] - 1u : 1u);
    frs_hold(2, 40, 0, H);
    frs_view(&v);
    check(v.bass < 0, "  the bass's: the same");

    /* SEQUENCE off under a finger: its note sounds on */
    {
        uint32_t i;
        for (i = 0; i < SC_BSEQ; i++)
            bnote[i] = (uint8_t)(i % 4u ? SQ_TIE : 36u);
    }
    param_set(P_BQ_ON, (int32_t)SQT_SEQ);
    rms(48000);
    frs_hold(2, 50, 1, H);
    frs_param_edit(P_BQ_ON, 0);
    frs_view(&v);
    check(v.bass >= 0, "SEQUENCE off under a finger: its note sounds on");
    frs_hold(2, 50, 0, H);
    frs_close();
}
#undef H

/* the metronome of its own (frs_metronome, the app's): PLAY from the grid's top with it on (the app's two messages a few
 * ms apart), its one ticks; then each beat one; turned on in the middle of a beat, nothing until the next */
static void metro_test(void)
{
    uint32_t n, k, beat;
    frs_init(44100.0f);
    rms(44100u);
    grid_top(0);                         /* (PLAY from the top) */
    frs_render(L, R, 256);
    frs_render(L, R, 256);
    frs_metronome(1);
    frs_render(L, R, 256);
    check(metro_n == 1u, "the metronome on with PLAY from the top, a few ms after: its one ticks");
    beat = (uint32_t)(44100.0f * 60.0f / (float)(st.v[P_TEMPO] + 30u));
    for (k = 0; k < 4u * beat / 256u; k++)
        frs_render(L, R, 256);
    check(metro_n == 4u || metro_n == 5u, "...then a tick a beat (four beats on: four or five)");
    frs_metronome(0);
    while (grid.tick - 24.0f * floorf(grid.tick / 24.0f) < 12.0f)
        frs_render(L, R, 256);
    n = metro_n;
    frs_metronome(1);
    frs_render(L, R, 256);
    check(metro_n == n, "turned on in the middle of a beat: no tick for it");
    for (k = 0; k < beat / 256u; k++)
        frs_render(L, R, 256);
    check(metro_n == n + 1u, "...the next beat's, on the grid");
    frs_metronome(0);
    frs_close();
}

/* REC from the iPad's keys (the app's: frs_hold, RECORD LIVE on a layer's SEQ), at 120 BPM: a beat 500 ms, a sixteenth
 * a step, sixteen of them a loop (2 s). Each layer's sequence full of notes to begin with */
static void ms_run(uint32_t ms)          /* (44.1 kHz: blocks of 256, some 5.8 ms) */
{
    uint32_t k;
    for (k = 0; k < ms * 441u / 2560u; k++)
        frs_render(L, R, 256);
}

static void rec_setup(int bass)
{
    uint32_t i;
    frs_init(44100.0f);
    param_set(P_TEMPO, 90);
    param_set(P_ARP_RATE, 7);            /* (1/16) */
    param_set(P_ARP_CHANCE, 127);
    param_set(P_BA_CHANCE, 127);
    param_set(P_SQ_LEN, 15);
    param_set(P_BQ_LEN, 0);              /* (1/16) */
    param_set(P_BQ_STEPS, 15);
    for (i = 0; i < SC_SEQ; i++)
        seq[i] = (uint8_t)(60u + i % 12u);
    for (i = 0; i < SC_BSEQ; i++)
        bnote[i] = (uint8_t)(36u + i % 12u);
    param_set(bass ? P_BQ_ON : P_SQ_TYPE, SQT_SEQ);
    frs_play_all(0);                     /* (stopped: as the app arms REC) */
    param_set(bass ? P_BQ_REC : P_SQ_REC, 2);
}

/* COUNT-IN: PLAY from stopped, a take armed, a bar of ticks first (the count's beats every 500 ms), nothing playing;
 * then everything on the downbeat after it (2 s after its one), the take with it. A stop during it: nothing starts.
 * Playing already, or a track started: none, at once */
static void count_test(void)
{
    frs_view_t v;
    uint32_t at[5], start, t0, k, quiet;
    int bass;
    for (bass = 0; bass < 2; bass++) {
        rec_setup(bass);
        frs_count_in(1);
        t0 = frs_ms();
        frs_play_all(1);
        memset(at, 0, sizeof at);
        start = 0;
        quiet = 1;
        for (k = 0; k < 3000u * 441u / 2560u && !start; k++) {
            frs_render(L, R, 256);
            frs_view(&v);
            if (v.counting && v.count_beat >= 1u && v.count_beat <= 4u && !at[v.count_beat])
                at[v.count_beat] = v.ms;
            if (!v.counting && (bass ? v.bseq_pos >= 0 : v.seq_pos >= 0))
                start = v.ms;
            else if (v.counting)         /* (its last millisecond and the first step may fall either side of a block) */
                quiet &= !v.playing && !sq.steps && !bq.steps;
        }
        check(at[1] && at[1] - t0 <= 12u, bass ? "the bass's take: the same" : "COUNT-IN: its one at once");
        check(at[2] - at[1] >= 494u && at[2] - at[1] <= 506u && at[3] - at[2] >= 494u && at[3] - at[2] <= 506u &&
                  at[4] - at[3] >= 494u && at[4] - at[3] <= 506u, "  its beats 500 ms apart (120 BPM)");
        check(quiet, "  nothing plays while it counts");
        check(start && start - at[1] >= 1994u && start - at[1] <= 2008u, "  the take on the downbeat after it, 2 s on");
        frs_close();
    }

    rec_setup(0);
    frs_count_in(1);
    frs_play_all(1);
    ms_run(1200);
    frs_play_all(0);
    frs_view(&v);
    check(!v.counting, "a stop during the count: no more count");
    ms_run(3000);
    frs_view(&v);
    check(!sq.run && !sq.steps && !v.playing, "  and nothing starts");

    frs_play_all(1);                     /* (counting again) */
    ms_run(300);
    frs_track_play(1, 1);
    ms_run(30);
    frs_view(&v);
    check(!v.counting && sq.run, "a track played during it: no more count, the track at once");
    frs_close();

    rec_setup(0);
    frs_count_in(1);
    param_set(P_BQ_ON, SQT_SEQ);
    frs_track_play(2, 1);                /* (the bass's sequence playing) */
    ms_run(100);
    frs_play_all(1);
    ms_run(30);
    frs_view(&v);
    check(!v.counting && sq.run, "playing already: no count, the take at once");
    frs_close();

    rec_setup(0);
    frs_count_in(0);
    frs_play_all(1);
    ms_run(30);
    frs_view(&v);
    check(!v.counting && sq.run, "COUNT-IN off: at once");
    frs_close();

    rec_setup(1);                        /* (the bass's take armed; GLINT's SEQ turned on during the count) */
    frs_count_in(1);
    frs_play_all(1);
    ms_run(600);
    param_set(P_SQ_TYPE, SQT_SEQ);
    ms_run(600);
    check(!sq.steps && !bq.steps, "a sequence turned on during the count: it waits for the downbeat");
    ms_run(1000);
    frs_view(&v);
    check(!v.counting && sq.run && bq.run && sq.steps && bq.steps, "  then both from it");
    frs_close();
}

/* the iPad's fingers into a take (frs_hold): GLINT's on the step nearest the playhead, no ACCENT; stopped, it sounds,
 * nothing written. OVERDUB off: the take's first turn replaces the sequence as the playhead comes to each step, what the
 * take wrote kept (one ahead of the playhead too), a turn on it adds; on, it adds from the start. The bass's the same,
 * its note as long as it was held */
static void take_test(void)
{
    uint8_t was[SC_SEQ];
    uint32_t i, notes, old, k;
    int b, p, q;

    rec_setup(0);
    frs_count_in(0);
    frs_overdub(1);
    frs_play_all(1);
    ms_run(300);                         /* (some 2.3 steps in) */
    q = (int)fm_floorf(seq_grid(grid_now()) / 6.0f - (float)sq.start + 0.5f) % 16;
    b = 50 - 12 * (int)st.v[P_GL_OCTAVE] + 24;
    frs_hold(1, (uint32_t)(b + 12 * (int)st.v[P_GL_OCTAVE]), 1, FRS_CHORD_HARMONY);
    check(seq[q] == (uint8_t)b, "a finger, REC LIVE on GLINT: its note on the step nearest the playhead, no ACCENT");
    frs_hold(1, (uint32_t)(b + 12 * (int)st.v[P_GL_OCTAVE]), 0, FRS_CHORD_HARMONY);
    ms_run(2000);
    for (i = old = 0; i < 16u; i++)
        old += (int)i != q && seq[i] == (uint8_t)(60u + i % 12u);
    check(old == 15u, "  OVERDUB: the rest as it was, a turn on");
    frs_play_all(0);
    memcpy(was, seq, sizeof was);
    frs_hold(1, 80, 1, FRS_CHORD_HARMONY);
    frs_hold(1, 80, 0, FRS_CHORD_HARMONY);
    check(!memcmp(was, seq, sizeof was), "  stopped: it sounds, nothing written");
    frs_close();

    rec_setup(0);                        /* (TRANSPOSE +1: written a step of the scale down, it plays as it was played) */
    frs_count_in(0);
    frs_overdub(1);
    param_set(P_SQ_TRANS, 15);
    frs_play_all(1);
    ms_run(300);
    q = (int)fm_floorf(seq_grid(grid_now()) / 6.0f - (float)sq.start + 0.5f) % 16;
    for (b = 74; !in_scale(b, st.v[P_ROOT] % 12, SCALE_BITS[st.v[P_SCALE] % NSCALES]); b--)
        ;
    frs_hold(1, (uint32_t)(b + 12 * (int)st.v[P_GL_OCTAVE]), 1, FRS_CHORD_HARMONY);
    frs_hold(1, (uint32_t)(b + 12 * (int)st.v[P_GL_OCTAVE]), 0, FRS_CHORD_HARMONY);
    check(scale_shift(seq[q] & 127, 1, st.v) == b && st.v[P_SQ_TRANS] == 15, "  TRANSPOSE +1: what it writes plays as it was played");
    frs_close();

    rec_setup(0);
    frs_count_in(0);
    frs_overdub(0);
    frs_play_all(1);
    for (k = 0; k < 78u; k++)            /* (some 450 ms: past the middle of the fourth step) */
        frs_render(L, R, 256);
    for (i = old = 0; i < 16u; i++)
        old += seq[i] == (uint8_t)(60u + i % 12u);
    check(!seq[0] && !seq[1] && !seq[2] && old >= 11u && old <= 13u,
          "OVERDUB off: the steps the playhead has come to gone, the others as they were");
    frs_hold(1, (uint32_t)(b + 12 * (int)st.v[P_GL_OCTAVE]), 1, FRS_CHORD_HARMONY);
    q = sq.skip;
    frs_hold(1, (uint32_t)(b + 12 * (int)st.v[P_GL_OCTAVE]), 0, FRS_CHORD_HARMONY);
    ms_run(1700);                        /* (the first turn over) */
    for (i = notes = 0; i < 16u; i++)
        notes += seq[i] != SQ_REST;
    check(q >= 0 && seq[q] == (uint8_t)b && notes == 1u, "  a turn on: rests, but for the take's note (ahead of the "
                                                          "playhead when played)");
    ms_run(1000);
    frs_hold(1, (uint32_t)(b + 2 + 12 * (int)st.v[P_GL_OCTAVE]), 1, FRS_CHORD_HARMONY);
    frs_hold(1, (uint32_t)(b + 2 + 12 * (int)st.v[P_GL_OCTAVE]), 0, FRS_CHORD_HARMONY);
    ms_run(2000);
    for (i = notes = 0; i < 16u; i++)
        notes += seq[i] != SQ_REST;
    check(notes == 2u && seq[q] == (uint8_t)b, "  then it adds: the second turn's note beside the first");
    frs_close();

    rec_setup(0);                        /* (OVERDUB turned on in the first turn; then off, and UNDO in it) */
    frs_count_in(0);
    frs_overdub(0);
    frs_play_all(1);
    ms_run(500);
    frs_overdub(1);
    ms_run(1600);
    for (i = old = 0; i < 16u; i++)
        old += seq[i] == (uint8_t)(60u + i % 12u);
    check(old >= 11u && old <= 13u, "OVERDUB on in the first turn: what the playhead hadn't come to stays");
    frs_overdub(0);
    frs_play_all(0);
    frs_play_all(1);
    ms_run(500);
    param_set(P_SQ_UNDO, 2);
    memcpy(was, seq, sizeof was);
    ms_run(1600);
    check(!memcmp(was, seq, sizeof was), "  UNDO in it: the sequence put back stays");
    frs_overdub(1);
    frs_close();

    rec_setup(1);
    frs_count_in(0);
    frs_overdub(0);
    frs_play_all(1);
    ms_run(450);
    p = bass_played(48);
    frs_hold(2, 48, 1, FRS_CHORD_HARMONY);
    ms_run(300);
    frs_hold(2, 48, 0, FRS_CHORD_HARMONY);
    ms_run(1500);                        /* (the first turn over) */
    for (i = notes = old = 0; i < 16u; i++) {
        uint32_t x = bnote[i] & 127u;
        notes += x != SQ_REST && x != SQ_TIE;
        old += x != SQ_REST && x != SQ_TIE && x != (uint32_t)p;
    }
    check(notes == 1u && !old, "the bass's take, OVERDUB off: a turn on, CUTs but for the finger's note");
    for (i = 0; i < 16u && (bnote[i] & 127u) != (uint32_t)p; i++)
        ;
    check(i < 16u && (bnote[(i + 1u) % 16u] & 127u) == SQ_TIE && (bnote[(i + 2u) % 16u] & 127u) == SQ_REST &&
              !(bnote[i] & SQ_ACC), "  held some 2.4 steps: its NOTE, a HOLD (to the step), no ACCENT");
    frs_close();

    rec_setup(1);
    frs_count_in(0);
    frs_overdub(1);
    frs_play_all(1);
    ms_run(450);
    frs_hold(2, 48, 1, FRS_CHORD_HARMONY);
    ms_run(300);
    frs_hold(2, 48, 0, FRS_CHORD_HARMONY);
    ms_run(1500);
    for (i = old = 0; i < 16u; i++)
        old += bnote[i] == (uint8_t)(36u + i % 12u);
    check(old == 14u, "  OVERDUB on: what was there on, the note's two steps laid over it");
    frs_close();

    rec_setup(1);                        /* (a finger down before REC, one on it in the take, let go: the first plays,
                                          * the take's note ended) */
    frs_count_in(0);
    param_set(P_BQ_REC, 0);
    frs_play_all(1);
    ms_run(300);
    frs_hold(2, 43, 1, FRS_CHORD_HARMONY);
    param_set(P_BQ_REC, 2);
    frs_hold(2, 48, 1, FRS_CHORD_HARMONY);
    ms_run(300);
    frs_hold(2, 48, 0, FRS_CHORD_HARMONY);
    check(bq.tn < 0 && bass_note == bass_played(43), "the bass: the take's finger let go over one from before: its "
                                                       "note ended, the other's again");
    frs_hold(2, 43, 0, FRS_CHORD_HARMONY);
    frs_overdub(1);
    frs_close();
}

/* the drone's sequence (3.0): each step the notes it holds, played as a chord latched is (every voice of the drone,
 * held_by's), at 120 BPM, a quarter a step (500 ms), four steps. A step's notes sound as they were set; a chord and
 * notes stacked on it, all of them; a rest none; a tie the step before's on; GATE lets them go in the step. RECORD LIVE:
 * a chord latched and a note stacked go into the step nearest the playhead, a note it has once, under TRANSPOSE +1
 * written a step down (it plays as played); a finger, a MIDI note too. STEP: a finger's into STEP's step, STEP on at the
 * last up. COUNT-IN for its take; CLEAR; a scene stored and taken with it; an earlier layout's read with none */
static int dq_has(const uint8_t *s, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < DQ_NOTES; i++)
        if (s[i] == n)
            return 1;
    return 0;
}

static int dq_sounds(const uint8_t *n, uint32_t k)   /* the sequence holds exactly these notes, each a drone voice */
{
    float live[FRS_LIVE];
    uint32_t i, j, found;
    if (dq.nsnd != k)
        return 0;
    eng_live(live);
    for (i = 0; i < k; i++) {
        for (j = 0; j < dq.nsnd && dq.snd[j] != n[i]; j++)
            ;
        for (found = 0, j = 0; j < 8u; j++)
            found |= live[j] == (float)n[i];
        if (j == dq.nsnd || !found || !held_by[n[i]])
            return 0;
    }
    return 1;
}

static void dq_setup(void)
{
    frs_init(44100.0f);
    param_set(P_TEMPO, 90);              /* (120 BPM) */
    param_set(P_DQ_RATE, 2);             /* (1/4: 500 ms a step) */
    param_set(P_DQ_LEN, 3);
    param_set(P_DQ_ON, 1);
    frs_play_all(0);                     /* (stopped, nothing kept) */
}

static void dseq_test(void)
{
    static const uint8_t CH[3] = {48, 52, 55}, ST[5] = {48, 52, 55, 62, 67}, TIE = FRS_DQ_TIE;
    uint8_t sc[FRS_SCENE_BYTES], o[FRS_SCENE_BYTES], d0[SC_DSEQ][DQ_NOTES];
    frs_view_t v;
    chord_t c, c1;
    uint8_t v1[NPARAM];
    int root, r, ok;
    uint32_t i, q, k, at[5], start, t0;

    dq_setup();
    frs_dseq_step(0, CH, 3);
    frs_dseq_step(1, ST, 5);
    frs_dseq_step(2, 0, 0);
    frs_dseq_step(3, &TIE, 1);
    frs_view(&v);
    check(!memcmp(v.dnote[0], CH, 3) && !v.dnote[0][3] && !memcmp(v.dnote[1], ST, 5) && !v.dnote[2][0] &&
              v.dnote[3][0] == DQ_TIE && v.dq_len == 3u && v.dq_rate == 2u,
          "the drone's sequence: its steps as set, the view has them");
    frs_play_all(1);
    ms_run(150);
    check(dq_sounds(CH, 3), "  a step's chord: its three notes on the drone");
    ms_run(500);
    check(dq_sounds(ST, 5), "  a chord and two notes stacked on it: the five");
    ms_run(500);
    check(!dq.nsnd && !held_by[48] && !held_by[67], "  a rest: none");
    ms_run(500);
    check(!dq.nsnd, "  a tie after a rest: none still");
    ms_run(500);
    check(dq_sounds(CH, 3) && dq.pos == 0, "  round again: the first step's");
    frs_play_all(0);
    check(!dq.nsnd && !held_by[48], "  stopped: let go");

    frs_dseq_step(1, &TIE, 1);           /* (a tie after the chord, then a rest; GATE at half) */
    param_set(P_DQ_GATE, 64);
    frs_play_all(1);
    ms_run(150);
    ok = dq_sounds(CH, 3);
    ms_run(500);
    check(ok && dq_sounds(CH, 3), "  a tie: the step before's notes on (GATE at half: the next one a tie, on)");
    ms_run(250);
    check(!dq.nsnd && !held_by[48], "  GATE: let go some half through the tie's step");
    frs_dseq_step(1, ST, 5);
    ms_run(1250);                        /* (round to the first step, 150 ms in) */
    ok = dq_sounds(CH, 3);
    ms_run(250);
    check(ok && !dq.nsnd, "  GATE on a step alone: let go in it");
    frs_close();

    /* LIVE, TRANSPOSE +1: a chord latched some 1.2 steps in, a note stacked, a note it has, a finger, MIDI */
    dq_setup();
    param_set(P_DQ_TRANS, 15);
    param_set(P_DQ_REC, 2);
    frs_count_in(0);
    frs_overdub(1);
    check(st.v[P_KB] == 0u && st.v[P_DQ_ON], "RECORD LIVE on the drone's sequence: the keys the drones'");
    frs_play_all(1);
    ms_run(600);
    q = (uint32_t)fm_floorf(dseq_at() / 24.0f + 0.5f) % 4u;
    r = frs_latch(0, 50, 2);             /* (a TRIAD on D3) */
    memcpy(v1, st.v, sizeof v1);
    v1[P_CHORD] = 2;
    chord_build(50, v1, &c, &root);
    v1[P_CHORD] = v1[P_VOICING] = 0;
    chord_build(69, v1, &c1, &root);
    frs_latch(0, 69, 0);                 /* (a note stacked: SINGLE) */
    frs_hold(0, (uint32_t)c.n[0], 1, 0); /* (a note the step has: once) */
    frs_hold(0, (uint32_t)c.n[0], 0, 0);
    for (i = 0, ok = q == 1u && r == 1 && c.k == 3u; i < c.k; i++)
        ok &= dq_has(dnote[q], (uint32_t)scale_shift(c.n[i], -1, st.v)) &&
              scale_shift(scale_shift(c.n[i], -1, st.v), 1, st.v) == c.n[i];
    for (i = k = 0; i < DQ_NOTES; i++)
        k += dnote[q][i] != 0u;
    check(ok && k == 4u && c1.k == 1u && dq_has(dnote[q], (uint32_t)scale_shift(c1.n[0], -1, st.v)),
          "  a chord latched and a note stacked: into the step nearest the playhead, a step of the scale down");
    check(!dnote[0][0] && !dnote[2][0] && !dnote[3][0], "  none twice, nowhere else");
    for (i = 1, ok = 1; i < k; i++)
        ok &= dnote[q][i - 1u] < dnote[q][i];
    check(ok, "  low to high");
    frs_latch(0, 50, 2);                 /* (let go: the sequence alone) */
    frs_latch(0, 69, 0);
    ms_run(2100);                        /* (its step again, 0.4 into it) */
    {
        uint8_t want[4] = {(uint8_t)c.n[0], (uint8_t)c.n[1], (uint8_t)c.n[2], (uint8_t)c1.n[0]};
        check(dq.pos == (int8_t)q && dq_sounds(want, 4), "  TRANSPOSE +1 on it: it plays as it was played");
    }
    ms_run(1000);                        /* (step 3, 0.4 in: nearest it) */
    frs_hold(0, 57, 1, 0);
    check(dq_has(dnote[3], (uint32_t)scale_shift(57, -1, st.v)), "  a finger (frs_hold): the same");
    frs_hold(0, 57, 0, 0);
    frs_midi(0x90, 62, 90);              /* (a MIDI note on the drones' channel) */
    ms_run(10);
    frs_midi(0x80, 62, 0);
    ms_run(10);
    check(dq_has(dnote[3], (uint32_t)scale_shift(62, -1, st.v)), "  a MIDI note: the same");
    frs_play_all(0);
    memcpy(d0, dnote, sizeof d0);
    frs_latch(0, 60, 0);
    frs_latch(0, 60, 0);
    check(!memcmp(d0, dnote, sizeof d0), "  stopped: it sounds, nothing written");
    param_set(P_DQ_CLEAR, 2);
    for (i = k = 0; i < SC_DSEQ; i++)
        k += dnote[i][0] != 0u;
    check(!k && !st.v[P_DQ_CLEAR] && !dq.nsnd, "  CLEAR: every step a rest");
    frs_close();

    /* STEP: two fingers into STEP's step, STEP on at the last up; a latch adds, STEP staying */
    dq_setup();
    param_set(P_DQ_REC, 1);
    param_set(P_DQ_STEP, 2);
    frs_hold(0, 48, 1, 0);
    frs_hold(0, 55, 1, 0);
    frs_hold(0, 48, 0, 0);
    ok = st.v[P_DQ_STEP] == 2u;
    frs_hold(0, 55, 0, 0);
    check(ok && dq_has(dnote[2], 48) && dq_has(dnote[2], 55) && st.v[P_DQ_STEP] == 3u,
          "RECORD STEP: two fingers into STEP's step, STEP on as the last lets go");
    frs_latch(0, 64, 0);
    frs_latch(0, 67, 0);
    check(dq_has(dnote[3], 64) && dq_has(dnote[3], 67) && st.v[P_DQ_STEP] == 3u, "  latches: into it, STEP staying");
    frs_close();

    /* STEP staying (frs_step_stays, the iPad's): fingers into STEP's step, STEP where it was; GLINT's and the bass's one
     * note a step, a new key's in its place; stopped, nothing starts */
    dq_setup();
    frs_step_stays(1);
    param_set(P_DQ_REC, 1);
    param_set(P_DQ_STEP, 1);
    frs_hold(0, 48, 1, 0);
    frs_hold(0, 52, 1, 0);
    frs_hold(0, 48, 0, 0);
    frs_hold(0, 52, 0, 0);
    frs_hold(0, 55, 1, 0);
    frs_hold(0, 55, 0, 0);
    frs_view(&v);
    check(dq_has(dnote[1], 48) && dq_has(dnote[1], 52) && dq_has(dnote[1], 55) && st.v[P_DQ_STEP] == 1u && !dq.run &&
          !(v.playing & 1u), "STEP staying: the drone's fingers into STEP's step, STEP where it was, nothing started");
    param_set(P_SQ_TYPE, SQT_SEQ);
    param_set(P_SQ_LEN, 7);
    frs_play_all(0);
    param_set(P_SQ_REC, 1);
    param_set(P_SQ_STEP, 5);
    frs_hold(1, 72, 1, 0);
    frs_hold(1, 72, 0, 0);
    k = seq[5] & 127u;
    frs_hold(1, 76, 1, 0);
    frs_hold(1, 76, 0, 0);
    frs_view(&v);
    check(k && (seq[5] & 127u) && (seq[5] & 127u) != k && (seq[6] & 127u) == SQ_REST && st.v[P_SQ_STEP] == 5u && !sq.run &&
          !(v.playing & 2u), "  GLINT's: its one note on STEP's step, a new key's in its place, STEP staying");
    param_set(P_BQ_ON, SQT_SEQ);
    param_set(P_BQ_STEPS, 7);
    frs_play_all(0);
    param_set(P_BQ_REC, 1);
    param_set(P_BQ_STEP, 3);
    frs_hold(2, 40, 1, 0);
    frs_hold(2, 40, 0, 0);
    k = bnote[3] & 127u;
    frs_hold(2, 43, 1, 0);
    frs_hold(2, 43, 0, 0);
    frs_view(&v);
    check(k && (bnote[3] & 127u) && (bnote[3] & 127u) != k && st.v[P_BQ_STEP] == 3u && !bq.run && !(v.playing & 4u),
          "  the bass's: the same");
    frs_step_stays(0);
    frs_close();

    /* COUNT-IN: the drone's take armed, a bar ticked, then on the downbeat */
    dq_setup();
    frs_dseq_step(0, CH, 3);
    param_set(P_DQ_REC, 2);
    frs_count_in(1);
    t0 = frs_ms();
    frs_play_all(1);
    memset(at, 0, sizeof at);
    start = 0;
    ok = 1;
    for (k = 0; k < 3000u * 441u / 2560u && !start; k++) {
        frs_render(L, R, 256);
        frs_view(&v);
        if (v.counting && v.count_beat >= 1u && v.count_beat <= 4u && !at[v.count_beat])
            at[v.count_beat] = v.ms;
        if (!v.counting && v.dseq_pos >= 0)
            start = v.ms;
        else
            ok &= !dq.steps && !dq.nsnd;
    }
    check(at[1] && at[1] - t0 <= 12u && at[4] - at[1] >= 1494u && at[4] - at[1] <= 1506u && ok,
          "COUNT-IN for the drone's take: a bar of ticks, nothing playing");
    check(start && start - at[1] >= 1994u && start - at[1] <= 2008u && dq_sounds(CH, 3), "  then from its first step, on the downbeat");
    frs_count_in(0);
    frs_close();

    /* a scene: stored with it, taken back; an earlier layout's, none */
    dq_setup();
    frs_dseq_step(0, CH, 3);
    frs_dseq_step(1, ST, 5);
    frs_dseq_step(2, &TIE, 1);
    param_set(P_DQ_GATE, 90);
    param_set(P_DQ_CHANCE, 100);
    param_set(P_DQ_TRANS, 16);
    check(frs_dirty(), "a scene: the drone's sequence edited, the scene changed");
    frs_scene_store(sc, 0);
    check(!frs_dirty() && sc[324] == 1u && sc[325] == 2u && sc[326] == 3u && sc[327] == 90u && sc[328] == 100u &&
              sc[329] == 16u && !memcmp(sc + 330, CH, 3) && !memcmp(sc + 338, ST, 5) && sc[346] == DQ_TIE &&
              offsetof(scene_t, dq_on) == 324u && offsetof(scene_t, dnote) == 330u,
          "  stored: its settings at 324 .. 329, its steps from 330, eight bytes each");
    frs_scene_new(o);
    frs_scene_take(o);
    ms_run(50);
    frs_view(&v);
    check(!st.v[P_DQ_ON] && !v.dnote[0][0] && !v.dnote[1][0] && st.v[P_DQ_RATE] == 4u && st.v[P_DQ_LEN] == 3u &&
              st.v[P_DQ_GATE] == 127u && st.v[P_DQ_CHANCE] == 127u && st.v[P_DQ_TRANS] == 14u && !dq.run,
          "  NEW SHARD: none (off, empty, 1 BAR, four steps)");
    frs_scene_take(sc);
    ms_run(50);
    frs_view(&v);
    check(st.v[P_DQ_ON] && !memcmp(v.dnote[0], CH, 3) && !memcmp(v.dnote[1], ST, 5) && v.dnote[2][0] == DQ_TIE &&
              st.v[P_DQ_GATE] == 90u && st.v[P_DQ_TRANS] == 16u && dq.run && !frs_dirty(),
          "  taken: its steps and settings, playing from the top");
    memset(o, 0xEE, sizeof o);
    memcpy(o, sc, 324);
    check(!frs_scene_over(o, 324) && !memcmp(o, sc, 324) && !o[324] && o[325] == 4u && o[326] == 3u && o[327] == 127u &&
              o[328] == 127u && o[329] == 14u && !o[330] && !o[585],
          "  FRSy's 324 bytes: as they were, the drone's sequence none");
    memset(o, 0xEE, sizeof o);
    memcpy(o, sc, 323);
    check(!frs_scene_over(o, 323) && !o[323] && !o[324] && o[325] == 4u && !o[585], "  FRSx's 323: the same");
    frs_scene_take(o);
    ms_run(50);
    check(!st.v[P_DQ_ON] && !dq.run && !dnote[0][0], "  taken: none plays");
    check(frs_scene_over(o, 400) == -1, "  a size it does not know: -1");
    frs_close();
}

/* 3.0's KEYS (the iPad's TOUCH, the FM-1's): each layer's HARMONY, NOTES, LATCH, STACK, kept with the settings, KEY MODE
 * read in as the drones' and the bass's LATCH; the keys honour them once frs_keys_touch (till then, as 2.x's) */
static void tap(uint32_t k, int note)
{
    frs_key_down(k, note, 100, 0);
    frs_key_up(k);
}

static uint32_t chords_lit(void) { return bits4(lit); }

static int arp_holds(const arp_t *a, int n)
{
    uint32_t i;
    for (i = 0; i < a->n; i++)
        if (a->held[i] == n)
            return 1;
    return 0;
}

static uint32_t glints_down(void)        /* GLINT's voices with their key down (the render's, after a block) */
{
    glint_view_t v[GLINT_NV];
    uint32_t i, n = 0;
    glint_view(v);
    for (i = 0; i < GLINT_NV; i++)
        n += v[i].note >= 0 && v[i].gate;
    return n;
}

static void keys_test(void)
{
    static const uint8_t V3[] = {'F', 'R', 's', 't', 3, 154, 1, 255, 1};   /* (a 2.6 .. 2.7 state: KEY MODE HOLD) */
    uint8_t state[FRS_STATE_BYTES], v1[NPARAM];
    uint32_t n, i, l, ok;
    int root;
    chord_t c;
    char t[FRS_TEXT];
    frs_init(44100.0f);
    for (l = 0, ok = 1; l < 3u; l++)
        ok &= frs_param_get(P_KY_DR_HARM + 4u * l) == 1u && frs_param_get(P_KY_DR_NOTES + 4u * l) == (l == 0u) &&
              frs_param_get(P_KY_DR_LATCH + 4u * l) == 1u && frs_param_get(P_KY_DR_STACK + 4u * l) == 0u &&
              frs_param_kind(P_KY_DR_HARM + 4u * l) == FRS_P_STATE && frs_param_kind(P_KY_DR_STACK + 4u * l) == FRS_P_STATE;
    frs_param_text(P_KY_GL_NOTES, 0, t);
    ok &= !strcmp(t, "SINGLE NOTE");
    check(ok && frs_param_kind(P_METRO) == FRS_P_NONE && frs_param_kind(P_GA_LATCH) == FRS_P_MOMENT,
          "KEYS: HARMONY ON, NOTES CHORD on the drone (SINGLE NOTE on GLINT and the bass), LATCH ON, STACK OFF; kept with the settings");
    frs_param_set(P_KY_GL_HARM, 0);
    frs_param_set(P_KY_BS_STACK, 1);
    frs_param_set(P_KY_DR_NOTES, 0);
    n = frs_state_save(state);
    frs_param_set(P_KY_GL_HARM, 1);
    frs_param_set(P_KY_BS_STACK, 0);
    frs_param_set(P_KY_DR_NOTES, 1);
    check(n <= FRS_STATE_BYTES && state[4] == 3u && frs_state_load(state, n) == 0 && !frs_param_get(P_KY_GL_HARM) &&
              frs_param_get(P_KY_BS_STACK) == 1u && !frs_param_get(P_KY_DR_NOTES),
          "  saved beside a scene (frs_state_save), back");
    frs_param_set(P_KY_GL_HARM, 1);
    frs_param_set(P_KY_BS_STACK, 0);
    frs_param_set(P_KY_DR_NOTES, 1);
    check(frs_state_load(V3, sizeof V3) == 0 && !frs_param_get(P_KY_DR_LATCH) && !frs_param_get(P_KY_BS_LATCH) &&
              frs_param_get(P_KY_GL_LATCH) == 1u && frs_param_get(P_KEYS) == 1u,
          "  a state before 3.0's, KEY MODE HOLD: read in as the drones' and the bass's LATCH OFF (GLINT's as it was)");
    frs_param_set(P_KY_DR_LATCH, 1);
    check(frs_param_get(P_KEYS) == 0u && !frs_param_get(P_KY_BS_LATCH), "  the drones' LATCH on: KEY MODE shows it");
    frs_param_set(P_KEYS, 0);
    frs_param_set(P_KEYS, 1);
    frs_param_set(P_KEYS, 0);
    check(frs_param_get(P_KY_DR_LATCH) == 1u && frs_param_get(P_KY_BS_LATCH) == 1u, "  KEY MODE turned: both LATCHes");

    /* till frs_keys_touch: the keys as 2.x's (GLINT's on KEYS sounding while down, whatever its LATCH) */
    param_set(P_ROOT, 2);                /* (D major, TRIAD) */
    param_set(P_SCALE, 1);
    param_set(P_CHORD, 2);
    param_set(P_VOICING, 0);             /* (CLOSE: D, F#, A) */
    param_set(P_SQ_TYPE, 0);
    param_set(P_KB, 1);
    frs_key_down(0, 63, 100, 0);
    check(key[0].kind == K_GLINT && key[0].root == 62 && key[0].chord.k == 1u && !arp.n,
          "keys as 2.x's: GLINT's on KEYS one glint in the scale, sounding while down");
    frs_key_up(0);
    frs_keys_touch(1);
    frs_param_set(P_KY_GL_NOTES, 1);     /* (CHORD; GLINT's SINGLE NOTE at the start) */

    /* GLINT on KEYS: its keys sound while down, whatever its LATCH (card #143: no ring of four from the keys) */
    frs_key_down(0, 63, 100, 0);
    ms_run(20);
    check(key[0].kind == K_GLINT && key[0].chord.k == 3u && !arp.n && glints_down() == 3u,
          "KEYS: GLINT's NOTES CHORD on KEYS, LATCH on: HARMONY's chord (a TRIAD on D), sounding while down, nothing "
          "latched");
    frs_key_up(0);
    ms_run(20);
    check(!arp.n && !rung.n && !glints_down(), "  let go: let go, nothing rings on (no ring of four)");
    tap(0, 63);
    frs_key_down(0, 63, 100, 0);
    ms_run(20);
    check(!arp.n && glints_down() == 3u, "  the same key again: it sounds again (the ring let it go one time in two)");
    frs_key_up(0);
    frs_param_set(P_KY_GL_HARM, 0);
    frs_key_down(0, 63, 100, 0);
    check(key[0].kind == K_GLINT && key[0].chord.k == 1u && key[0].chord.n[0] == 63 && !arp.n,
          "  HARMONY off: the key's own note (D#, off the scale), no chord, sounding while down");
    frs_key_up(0);
    frs_param_set(P_KY_GL_HARM, 1);
    frs_param_set(P_KY_GL_NOTES, 0);
    frs_key_down(0, 69, 100, 0);
    frs_key_down(1, 71, 100, 0);
    frs_key_up(0);
    ms_run(20);
    check(!arp.n && key[1].kind == K_GLINT && glints_down() == 1u, "  two keys: each its own, the one let go silent");
    frs_key_up(1);
    frs_param_set(P_KY_GL_STACK, 1);
    tap(0, 74);
    check(!arp.n, "  STACK on: nothing latched either");
    frs_param_set(P_KY_GL_STACK, 0);
    frs_param_set(P_KY_GL_LATCH, 0);
    frs_key_down(0, 66, 100, 0);
    check(key[0].kind == K_GLINT && !arp.n, "  LATCH off: the same");
    frs_key_up(0);
    frs_param_set(P_KY_GL_LATCH, 1);

    /* THE KEYS' LATCH (frs_latch: the River's, the editor's, the iPad's) still rings four on KEYS; a GLINT key there lets
     * them go first (its voice a free one, not a ringing note's), GLINT's LATCH turned off too */
    {
        static const uint8_t RN[4] = {62, 64, 66, 67};
        int all = 1;
        for (i = 0; i < 4u; i++)
            all &= frs_latch(1u, RN[i] + 12u * st.v[P_GL_OCTAVE], 0) == 1;
        ms_run(20);
        check(all && arp.n == 4u && rung.n == 4u && glints_down() == 4u, "THE KEYS' LATCH on GLINT, on KEYS: four ring");
    }
    frs_key_down(0, 71, 100, 0);
    ms_run(20);
    check(!arp.n && !rung.n && glints_down() == 1u, "  a GLINT key: the ring let go first, the key alone sounding");
    frs_key_up(0);
    ms_run(20);
    check(!glints_down(), "  let go: silence");
    frs_latch(1u, 62u + 12u * st.v[P_GL_OCTAVE], 0);
    ms_run(20);
    frs_key_down(0, 62, 100, 0);
    ms_run(20);
    check(!rung.n && glints_down() == 1u, "  a key on a ringing note: one voice, the key's");
    frs_key_up(0);
    ms_run(20);
    check(!glints_down(), "  let go: silence");
    check(frs_latch(1u, 62u + 12u * st.v[P_GL_OCTAVE], 0) == 1 && arp.n == 1u, "  latched again");
    arp.paused = 1;                      /* (PLAY tapped: it waits, silent) */
    ms_run(20);
    frs_key_down(0, 71, 100, 0);
    ms_run(20);
    check(!rung.n && arp.n == 1u && glints_down() == 1u, "  waiting (PLAY tapped): a key sounds, the ring's note kept");
    frs_key_up(0);
    arp.paused = 0;
    ms_run(20);
    check(rung.n == 1u && glints_down() == 1u, "  PLAY again: it rings");
    frs_param_set(P_GA_LATCH, 0);
    check(arp.n == 1u, "  ARP II's LATCH off on KEYS: the ring stays");
    frs_param_set(P_GA_LATCH, 1);
    frs_param_set(P_KY_GL_LATCH, 0);
    check(!arp.n, "  GLINT's LATCH turned off: the ring let go");

    /* GLINT on ARP: NOTES CHORD, its chord into the arpeggio; SWAP on its latch */
    param_set(P_SQ_TYPE, SQT_ARP);
    frs_param_set(P_KY_GL_NOTES, 1);
    frs_param_set(P_KY_GL_STACK, 0);
    tap(0, 62);
    check(arp.latch && arp.n == 3u && arp_holds(&arp, 62) && arp_holds(&arp, 66) && arp_holds(&arp, 69),
          "GLINT on ARP, NOTES CHORD: the chord's notes into the arpeggio, latched");
    tap(0, 67);
    check(arp.n == 3u && arp_holds(&arp, 67) && !arp_holds(&arp, 62), "  SWAP: a new hand's chord in their place");
    frs_param_set(P_KY_GL_STACK, 1);
    tap(0, 62);
    check(arp.n == 6u, "  STACK on: added");
    tap(0, 62);
    check(arp.n == 3u && !arp_holds(&arp, 62), "  a tap on a chord there: it goes");

    /* ARP II's LATCH: a setting over the arpeggio's latch */
    frs_param_set(P_GA_LATCH, 0);
    check(!arp.latch && !arp.n, "ARP II's LATCH off: the latch let go, only what is held goes on (nothing)");
    frs_key_down(0, 62, 100, 0);
    check(arp.n == 3u, "  a key down: its chord while held");
    frs_key_up(0);
    check(!arp.n, "  let go: gone");
    arp.latch = 1;                       /* (ARP held, the FM-1's: the latch itself) */
    ms_run(10);
    check(frs_param_get(P_GA_LATCH) == 1u, "  ARP held turns it: the setting shows it");
    param_set(P_BQ_ON, SQT_ARP);
    frs_param_set(P_BA_LATCH, 0);
    check(!barp.latch && frs_param_get(P_BA_LATCH) == 0u, "  the bass's the same (P_BA_LATCH)");
    frs_param_set(P_BA_LATCH, 1);

    /* the bass: on ARP, HARMONY's chord into its arpeggio; on KEYS one note, LATCH or HOLD */
    param_set(P_KB, 2);
    frs_param_set(P_KY_BS_NOTES, 1);
    tap(0, 50);
    check(barp.n == 3u, "the bass on ARP, NOTES CHORD: the chord's notes into its arpeggio");
    frs_param_set(P_KY_BS_NOTES, 0);
    frs_param_set(P_BA_LATCH, 0);
    param_set(P_BQ_ON, 0);
    tap(0, 50);
    check(bass_note == 50 && key[0].kind == K_NONE, "  on KEYS: one note, latched");
    tap(1, 55);
    check(bass_note == 55, "  another: in its place");
    frs_param_set(P_KY_BS_LATCH, 0);
    check(bass_note < 0, "  its LATCH turned off: the latched note let go");
    frs_key_down(0, 52, 100, 0);
    check(bass_note == 52 && key[0].hold, "  HOLD: sounding while down");
    frs_key_up(0);
    check(bass_note < 0, "  let go: silent");
    frs_param_set(P_KY_BS_LATCH, 1);
    frs_param_set(P_KY_BS_HARM, 0);
    tap(0, 51);
    check(bass_note == 51, "  HARMONY off: off the scale too");
    tap(0, 51);
    frs_param_set(P_KY_BS_HARM, 1);

    /* the drones: SWAP (STACK off), HOLD (LATCH off), SINGLE NOTE, HARMONY off; MIDI's HARMONY too */
    param_set(P_KB, 0);
    frs_param_set(P_KY_DR_STACK, 1);
    tap(0, 50);
    tap(1, 57);
    check(chords_lit() == 2u, "the drones, STACK on: two chords latched");
    frs_param_set(P_KY_DR_STACK, 0);
    tap(2, 62);
    check(chords_lit() == 1u && bit_get(lit, 62), "  STACK off (SWAP): a new chord in place of both");
    frs_key_down(0, 64, 100, 0);
    frs_key_down(1, 67, 100, 0);
    frs_key_up(0);
    frs_key_up(1);
    check(chords_lit() == 2u && bit_get(lit, 64) && bit_get(lit, 67), "  a hand's keys down together: stacked");
    tap(0, 64);
    check(chords_lit() == 1u, "  a tap on a lit one: it goes");
    frs_param_set(P_KY_DR_LATCH, 0);
    check(!chords_lit() && frs_param_get(P_KEYS) == 1u, "  LATCH turned off: the chords latched let go (KEY MODE HOLD)");
    frs_key_down(0, 50, 100, 0);
    memcpy(v1, st.v, sizeof v1);
    chord_build(50, v1, &c, &root);
    for (i = 0, ok = key[0].kind == K_HOLD && c.k == 3u; i < c.k; i++)
        ok &= held_by[(uint8_t)c.n[i]] == 1u;
    frs_key_up(0);
    check(ok && !held_by[(uint8_t)c.n[0]] && !chords_lit(), "  HOLD: the chord while down, nothing latched");
    frs_param_set(P_KY_DR_LATCH, 1);
    frs_param_set(P_KY_DR_NOTES, 0);
    tap(0, 50);
    check(chords_lit() == 1u && lat[50].k == 1u && lat[50].n[0] == 50, "  NOTES SINGLE NOTE: one note");
    frs_param_set(P_KY_DR_NOTES, 1);
    frs_param_set(P_KY_DR_HARM, 0);
    tap(0, 63);
    check(chords_lit() == 1u && bit_get(lit, 63) && lat[63].k == 1u && lat[63].n[0] == 63,
          "  HARMONY off: the key's own note, chromatic, in place of the one before (SWAP)");
    frs_midi(0x90, 61, 100);
    ms_run(10);
    check(bit_get(mid_on, 61) && mid[61].k == 1u && mid[61].n[0] == 61, "  MIDI on the drones' channel: HARMONY off too");
    frs_midi(0x80, 61, 0);
    frs_param_set(P_KY_DR_HARM, 1);
    frs_midi(0x90, 61, 100);
    ms_run(10);
    check(mid[61].k == 3u, "  HARMONY on: its chord, as ever");
    frs_midi(0x80, 61, 0);
    frs_close();
}

/* the iPad agent's points: a key into a waiting arpeggio; what a scene stores while keys are down; frs_hold chromatic with
 * HARMONY OFF; the keys' limit of eight chords */
static void keys_points_test(void)
{
    static uint8_t sc[FRS_SCENE_BYTES];
    uint32_t i, ok;
    frs_init(44100.0f);
    frs_keys_touch(1);
    frs_param_set(P_KY_GL_NOTES, 0);

    /* a key on a waiting arpeggio plays it, as frs_latch's and the ring's do */
    param_set(P_KB, 1);
    param_set(P_SQ_TYPE, SQT_ARP);
    arp_wait(&arp, 1);
    check(arp.paused == 1u, "a key into a waiting arpeggio: waiting");
    tap(0, 60);
    check(!arp.paused && arp.n == 1u, "  the key: it plays again, its note in");
    frs_clear(1);

    /* a scene stored with a finger on an unlatched arpeggio: the note is the finger's, not the scene's */
    frs_param_set(P_GA_LATCH, 0);
    ms_run(2);
    frs_key_down(0, 60, 100, 0);
    check(arp.n == 1u, "a scene stored, ARP's LATCH off, a key down");
    frs_scene_store(sc, 0);
    frs_key_up(0);
    frs_scene_take(sc);
    ms_run(50);
    check(!arp.n, "  taken again: that note not left playing with no finger on it");
    frs_param_set(P_GA_LATCH, 1);
    ms_run(2);
    frs_clear(1);
    param_set(P_KB, 2);
    param_set(P_BQ_ON, SQT_ARP);
    frs_param_set(P_BA_LATCH, 0);
    ms_run(2);
    frs_key_down(0, 48, 100, 0);
    frs_scene_store(sc, 0);
    frs_key_up(0);
    frs_scene_take(sc);
    ms_run(50);
    check(!barp.n, "  the bass's arpeggio the same");
    frs_param_set(P_BA_LATCH, 1);
    param_set(P_BQ_ON, 0);
    ms_run(2);

    /* a finger (frs_hold) on a latched arpeggio: its note is the finger's, not the scene's */
    frs_clear(1);
    param_set(P_KB, 1);
    param_set(P_SQ_TYPE, SQT_ARP);
    ms_run(2);
    frs_latch(1, 72, 0);
    frs_hold(1, 74, 1, FRS_CHORD_HARMONY);
    ms_run(2);
    check(arp.n == 2u, "a scene stored, a note latched and a finger on another");
    frs_scene_store(sc, 0);
    frs_hold(1, 74, 0, FRS_CHORD_HARMONY);
    frs_scene_take(sc);
    ms_run(50);
    check(arp.n == 1u, "  taken again: the latched note alone");
    frs_clear(1);
    param_set(P_SQ_TYPE, 0);
    ms_run(2);

    /* frs_hold on the drone: chromatic with HARMONY OFF, keys_touch on; as ever with it off */
    param_set(P_KB, 0);
    param_set(P_ROOT, 0);
    param_set(P_SCALE, 1);               /* (MAJOR: 61 not in it) */
    frs_param_set(P_KY_DR_HARM, 0);
    frs_hold(0, 61, 1, FRS_CHORD_HARMONY);
    check(hold_c[61].k == 1u && hold_c[61].n[0] == 61 && held_by[61] == 1u, "frs_hold, HARMONY OFF: the finger's own note");
    frs_hold(0, 61, 0, FRS_CHORD_HARMONY);
    check(!held_by[61], "  let go");
    frs_keys_touch(0);
    frs_hold(0, 61, 1, 0);
    check(hold_c[61].n[0] != 61, "  keys_touch off: the scale's, as it was");
    frs_hold(0, 61, 0, 0);
    frs_keys_touch(1);
    frs_param_set(P_KY_DR_HARM, 1);

    /* the keys stack eight chords, no more (a scene keeps eight) */
    frs_param_set(P_KY_DR_STACK, 1);
    for (i = 0; i < 10u; i++)
        tap(i % 4u, 40 + (int)i * 3);
    ok = chords_lit() == 8u;
    check(ok, "the keys: eight chords latched, the ninth refused");
    tap(0, 40 + 3 * 1);
    check(chords_lit() == 8u || chords_lit() == 7u, "  a tap on a lit one still lets it go");
    frs_param_set(P_KY_DR_STACK, 0);
    tap(1, 76);
    check(chords_lit() == 1u, "  SWAP still replaces them");
    frs_close();
}

/* DRONE SEQ from the knobs: a step's NOTES (REST, its notes, TIE), its LENGTH (ties), UNDO TAKE; RECORD STEP's LENGTH */
static void dq_knobs_test(void)
{
    static const uint8_t CH[3] = {48, 52, 55};
    char t[FRS_TEXT];
    uint8_t s0[SC_DSEQ][DQ_NOTES];
    uint32_t ok;
    dq_setup();
    param_set(P_ROOT, 0);
    frs_dseq_step(0, CH, 3);
    frs_param_set(P_DQ_STEP, 0);
    frs_param_text(P_DQ_NOTES, frs_param_get(P_DQ_NOTES), t);
    check(frs_param_get(P_DQ_NOTES) == 1u && !strcmp(t, "3 NOTES") && frs_param_get(P_DQ_NLEN) == 1u &&
              frs_param_kind(P_DQ_NOTES) == FRS_P_STEP && frs_param_kind(P_DQ_UNDO) == FRS_P_ACTION,
          "DRONE SEQ II's NOTES: the step on STEP, its three notes; LENGTH 1");
    frs_param_set(P_DQ_NOTES, 0);
    ok = !dnote[0][0];
    frs_param_set(P_DQ_NOTES, 2);
    ok &= dnote[0][0] == DQ_TIE;
    frs_param_set(P_DQ_NOTES, 1);
    check(ok && !memcmp(dnote[0], CH, 3) && !dnote[0][3], "  REST, TIE, then its notes back");
    frs_param_set(P_DQ_STEP, 1);
    frs_param_set(P_DQ_NOTES, 1);
    {
        chord_t c;
        int root;
        uint32_t i;
        chord_build(48 + (int)st.v[P_ROOT], st.v, &c, &root);
        for (i = 0, ok = c.k > 1u; i < c.k; i++)
            ok &= dq_has(dnote[1], (uint8_t)c.n[i]);
        check(ok, "  an empty step's notes, nothing held: HARMONY's chord on ROOT");
    }
    frs_param_set(P_DQ_NOTES, 0);
    frs_param_set(P_DQ_STEP, 0);
    frs_param_set(P_DQ_NLEN, 3);
    check(dnote[1][0] == DQ_TIE && dnote[2][0] == DQ_TIE && !dnote[3][0] && frs_param_get(P_DQ_NLEN) == 3u,
          "LENGTH 3: two ties after it");
    frs_param_set(P_DQ_STEP, 2);
    check(frs_param_get(P_DQ_NOTES) == 2u && frs_param_get(P_DQ_NLEN) == 3u, "  on its tie: TIE, the chord's LENGTH");
    frs_param_set(P_DQ_NLEN, 1);              /* (on a tie: as far as STEP at least) */
    frs_param_set(P_DQ_STEP, 0);
    check(frs_param_get(P_DQ_NLEN) == 3u, "  shortened on its tie: no shorter than to it");
    frs_param_set(P_DQ_NLEN, 1);
    check(!dnote[1][0] && !dnote[2][0], "  LENGTH 1 on the chord's step: the ties gone");

    /* UNDO TAKE: the sequence before the take */
    memcpy(s0, dnote, sizeof s0);
    frs_param_set(P_DQ_REC, 1);          /* (a take: STEP) */
    check(frs_param_get(P_DQ_NLEN) == 1u, "RECORD STEP: LENGTH carried on, the chord on STEP's");
    frs_param_set(P_DQ_NLEN, 2);
    param_set(P_KB, 0);
    frs_key_down(0, 57, 100, 0);
    frs_key_up(0);
    check(dnote[0][0] && dnote[1][0] == DQ_TIE && !dnote[2][0] && frs_param_get(P_DQ_STEP) == 2u,
          "  a key: its chord into STEP's step, LENGTH 2 (a tie), STEP on past it");
    frs_param_set(P_DQ_REC, 0);
    frs_param_set(P_DQ_UNDO, 1);
    frs_param_set(P_DQ_UNDO, 2);
    check(!memcmp(dnote, s0, sizeof s0) && !frs_param_get(P_DQ_UNDO), "  UNDO TAKE (turned twice): as before the take");
    frs_param_set(P_DQ_UNDO, 2);
    check(dnote[1][0] == DQ_TIE, "  again: the take back");
    frs_param_set(P_DQ_CLEAR, 2);
    frs_param_set(P_DQ_UNDO, 2);
    check(dnote[1][0] == DQ_TIE && dnote[0][0], "  CLEAR, then UNDO: back");
    frs_close();
}

/* REC on the selected layer (frs_record_layer): the drone's sequence, counted in from silence; STEP; OFF */
static void rec_layer_test(void)
{
    frs_view_t v;
    uint32_t k, began = 0, counted = 0;
    dq_setup();
    frs_count_in(1);
    check(frs_record_layer(0) == 3 && st.v[P_DQ_REC] == 2u && st.v[P_DQ_ON] && st.v[P_KB] == 0u,
          "REC on the drone: its sequence recording LIVE (frs_record_layer 0), the keys the drones'");
    frs_view(&v);
    check(v.counting && !dq.run, "  from silence, COUNT-IN on: a bar counted in, nothing playing");
    for (k = 0; k < 2600u * 441u / 2560u && !began; k++) {
        frs_render(L, R, 256);
        frs_view(&v);
        counted |= v.count_beat == 4u;
        began = !v.counting && dq.run;
    }
    check(began && counted, "  on the downbeat after its four: the drone's sequence, the take with it");
    frs_latch(0, 50, FRS_CHORD_HARMONY);
    check(dseq_has_notes(), "  a chord latched: into the take");
    check(frs_record_layer(0) == 3 && st.v[P_DQ_REC] == 1u, "  REC again: STEP");
    check(frs_record_layer(0) == 0 && !st.v[P_DQ_REC], "  again: off");
    check(frs_record_layer(0) == 3 && !cnt.run, "  REC while it plays: LIVE at once, no count");
    frs_record_layer(0);
    frs_record_layer(0);
    param_set(P_KB, 0);
    check(frs_record() == 1 && st.v[P_SQ_REC] == 2u, "frs_record (2.x's): on the drones, GLINT's");
    frs_record();
    frs_record();
    frs_play_all(0);
    check(frs_record_layer(1) == 1, "frs_record_layer 1: GLINT's");
    frs_view(&v);
    check(v.counting, "  from silence: counted in");
    check(frs_record_layer(1) == 1 && !cnt.run && st.v[P_SQ_REC] == 1u, "  STEP during the count: no more count");
    frs_record_layer(1);
    frs_close();

    /* from a new scene, untouched: GLINT on ARP with no notes is silence (its pattern empty) */
    frs_init(44100.0f);
    frs_count_in(1);
    check(st.v[P_SQ_TYPE] == SQT_ARP && !arp.n && frs_record_layer(0) == 3 && cnt.run,
          "a new shard (GLINT on ARP, no notes): silence, counted in");
    frs_close();

    /* the FM-1's own COUNT-IN (core.c, at eng_moves: count_own) */
    dq_setup();
    tr_reset();                          /* (the FM-1 has no tracks stopped: dq_setup's frs_play_all(0)) */
    count_own = 1;
    count_hush = count_hush_own;
    count_go = play_top;
    st.v[P_COUNT_IN] = 1;
    eng_record_on(0, 1);
    check(cnt.run && !dq.run, "the FM-1's REC from silence: counted in");
    ms_run(2600);
    check(!cnt.run && dq.run, "  then the drone's sequence plays (PLAY's top)");
    eng_play();
    check(!dq.run, "  PLAY: stopped");
    eng_play();
    check(cnt.run && !dq.run, "  PLAY, a LIVE take armed: counted in again");
    eng_play_tap(&taken_sc);
    check(!cnt.run && !dq.run, "  PLAY while it counts: no more count, nothing started");
    eng_play_tap(&taken_sc);
    check(cnt.run && !dq.run, "  PLAY tapped, the take armed, nothing held: counted in, not the scene at once");
    count_stop();
    eng_record_on(0, 1);
    frs_close();
}

/* METRONOME (OFF, REC, ALWAYS) over frs_metronome and frs_record_metronome */
static void metro_set_test(void)
{
    frs_init(44100.0f);
    check(frs_param_get(P_METRO) == 1u && st.metro && !metro_free && !frs_param_get(P_COUNT_IN),
          "METRONOME REC at the start (a take's ticks); COUNT-IN off (the engine alone's)");
    frs_param_set(P_METRO, 2);
    check(st.metro && metro_free, "  ALWAYS: the grid's beats too");
    frs_param_set(P_METRO, 0);
    check(!st.metro && !metro_free, "  OFF: none");
    frs_metronome(1);
    ms_run(30);
    check(frs_param_get(P_METRO) == 2u, "  frs_metronome on: it shows ALWAYS");
    frs_count_in(1);
    check(frs_param_get(P_COUNT_IN) == 1u, "  frs_count_in: COUNT-IN shows it");
    frs_close();
}

/* a layer's preset: its bytes captured, the sound changed, taken back; another layer's untouched */
static void preset_test(void)
{
    uint8_t g[FRS_PRESET_BYTES], d[FRS_PRESET_BYTES], b[FRS_PRESET_BYTES], bad[FRS_PRESET_BYTES];
    uint32_t ng, nd, nb, i, ok;
    uint8_t mist[NDRONE];
    frs_init(44100.0f);
    param_set(P_GL_PRESET, 5);
    param_set(P_GL_VIBRATO, 99);
    ng = frs_preset_capture(1, "MY GLASS", g);
    check(ng == FRS_PRESET_HEAD + 20u && g[0] == 'F' && g[1] == 'R' && g[2] == 'p' && g[3] == 1u && g[4] == 1u &&
              g[5] == 10u && !memcmp(g + 6, "MY GLASS", 8) && !g[14] && frs_preset_param(1, 0) == P_GL_RATIO &&
              frs_preset_param(1, 10) == -1 && frs_preset_is(g, ng) == 1,
          "a GLINT preset: \"FRp\", its layer, its ten settings, its name; the sound is it");
    param_set(P_GL_PRESET, 12);
    param_set(P_GL_VIBRATO, 10);
    param_set(P_GL_LEVEL, 33);
    param_set(P_BS_FILTER, 20);
    check(frs_preset_is(g, ng) == 0, "  the sound changed: not it");
    check(frs_preset_apply(g, ng) == 1 && frs_preset_is(g, ng) == 1 && st.v[P_GL_VIBRATO] == 99u &&
              st.v[P_GL_LEVEL] == 33u && st.v[P_BS_FILTER] == 20u,
          "  taken: its settings back, GLINT's LEVEL and the bass's untouched");
    param_set(P_DR_PRESET, 1);           /* (MIST) */
    for (i = 0; i < NDRONE; i++)
        mist[i] = st.v[DRONE_P[i]];
    nd = frs_preset_capture(0, "MY MIST", d);
    param_set(P_DR_PRESET, 9);
    param_set(P_CHOEUR, 3);
    check(frs_preset_apply(d, nd) == 0 && st.v[P_DR_PRESET] == 0u, "a drone preset taken: CUSTOM, from it");
    for (i = 0, ok = nd == FRS_PRESET_BYTES; i < NDRONE; i++)
        ok &= st.v[DRONE_P[i]] == mist[i] && custom[i] == mist[i];
    check(ok, "  its 33 settings, MIST's (the longest preset: FRS_PRESET_BYTES)");
    param_set(P_BS_MODEL, 4);
    param_set(P_BS_BITE, 77);
    nb = frs_preset_capture(2, 0, b);
    param_set(P_BS_MODEL, 1);
    param_set(P_BS_BITE, 0);
    check(nb == FRS_PRESET_HEAD + 28u && !b[6] && frs_preset_apply(b, nb) == 2 && st.v[P_BS_MODEL] == 4u &&
              st.v[P_BS_BITE] == 77u, "a bass preset (no name): its MODEL and sound back");
    memcpy(bad, g, ng);
    bad[PRESET_HEAD] = P_BS_LEVEL;        /* (another layer's setting in it: left alone) */
    bad[PRESET_HEAD + 1u] = 1;
    check(frs_preset_apply(bad, ng) == 1 && st.v[P_BS_LEVEL] != 1u, "  a setting not its layer's: never set");
    bad[0] = 'X';
    check(frs_preset_apply(bad, ng) == -1 && frs_preset_apply(g, 10) == -1 && frs_preset_capture(3, 0, bad) == 0u,
          "  not one, too short, no such layer: refused");
    frs_close();
}

/* "FRS}" from the inside: each TARGET's AMOUNT in what the LFOs add (x1.00 as before, bit for bit; x0.00 nothing; x5.00
 * five times; LFO 2's slots its own), SWELL gliding back on ANCHOR's MORPH (eng_swell hearing it) while the AMOUNTs switch at its
 * half, SWELL at once on a scene's MORPH (as BASS TONE), the scene's name of 32 on it */
static void amount_swell_test(void)
{
    uint8_t sc[FRS_SCENE_BYTES];
    uint32_t i;
    float depth = 100.0f * (1.0f / 127.0f), off, a35 = (float)35u / 100.0f, h;
    frs_init(44100.0f);
    frs_scene_new(sc);
    frs_scene_take(sc);
    for (i = 0; i < 4u; i++) {           /* (NEW SHARD's own TARGETs off) */
        param_set(P_L1_T1 + i, 0);
        param_set(P_L2_T1 + i, 0);
        param_set(P_L3_T1 + i, 0);
    }
    param_set(P_L1_SHAPE, 4);            /* (SQUARE: -1 or 1) */
    param_set(P_L1_DEPTH, 100);
    param_set(P_L1_T1, 1);               /* (TIMBRE) */
    param_set(P_L1_T2, 2);               /* (SPACE) */
    param_set(P_L1_T3, 1);               /* (TIMBRE again) */
    param_set(P_L1_AMT2, 35);
    param_set(P_L1_AMT3, 0);
    param_set(P_L2_SHAPE, 4);
    param_set(P_L2_DEPTH, 127);
    param_set(P_L2_T4, 3);               /* (MOTION) */
    ms_run(100);
    off = depth * lfo[0].out;
    check(lfo[0].on && fm_fabsf(lfo[0].out) == 1.0f && mod[P_TIMBRE] == off * (float)PD[P_TIMBRE].max,
          "LFO AMOUNT: TIMBRE at x1.00 and at x0.00 on one LFO, the first alone, as before");
    check(mod[P_ESPACE] == off * a35 * (float)PD[P_ESPACE].max, "  SPACE at x0.35: the LFO's times 0.35");
    check(mod[P_MOUVEMENT] == 127.0f * (1.0f / 127.0f) * lfo[1].out * (float)PD[P_MOUVEMENT].max,
          "  LFO 2's TARGET 4 at x1.00: as before, bit for bit");
    param_set(P_L2_AMT4, 200);
    ms_run(30);
    check(mod[P_MOUVEMENT] == 127.0f * (1.0f / 127.0f) * lfo[1].out * 5.0f * (float)PD[P_MOUVEMENT].max,
          "  at x5.00: five times");
    param_set(P_L1_AMT2, 0);
    ms_run(30);
    check(mod[P_ESPACE] == 0.0f, "  x0.00: nothing");
    frs_close();

    frs_init(44100.0f);
    frs_scene_new(sc);
    frs_scene_take(sc);
    param_set(P_SWELL, 20);
    frs_anchor_set();
    param_set(P_SWELL, 120);
    param_set(P_L1_AMT1, 30);
    ms_run(50);
    check(fm_fabsf(st.swell_sent - 120.0f / 127.0f) < 0.005f, "SWELL at 120: heard");
    frs_anchor_back(4000);
    check(st.v[P_SWELL] == 20u && morph_off[P_SWELL] == 100.0f, "ANCHOR's MORPH: SWELL the anchor's, heard from where it was");
    ms_run(1000);
    h = (float)st.v[P_SWELL] + morph_off[P_SWELL];
    check(h > 25.0f && h < 115.0f && fm_fabsf(st.swell_sent - h / 127.0f) < 0.005f, "  a second in: SWELL gliding, heard so");
    check(st.v[P_L1_AMT1] == 30u, "  an AMOUNT: the hand's before its half");
    ms_run(1500);
    check(st.v[P_L1_AMT1] == 100u, "  past its half: the anchor's (x1.00, NEW SHARD's)");
    ms_run(2000);
    check(!morph_off[P_SWELL] && fm_fabsf(st.swell_sent - 20.0f / 127.0f) < 0.005f, "  at its end: SWELL the anchor's, heard");

    memcpy(sc, "ABCDEFGHIJKLMNOP", 16);
    memcpy(sc + 590, "QRSTUVWXYZ012345", 16);
    sc[618] = 90;
    param_set(P_MORPH, 3);
    frs_scene_go(sc);
    check(mo.on && !mo.anc && st.v[P_SWELL] == 90u && !morph_off[P_SWELL], "a scene's MORPH: its SWELL at once (as BASS TONE)");
    check(!strcmp(mo.name, "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"), "  its name of 32, whole");
    frs_close();
}

/* a TARGET past this engine's list (an FM-1's or an SMK-37's scene with a LEAD TARGET, card #144: 62 .. 83 there)
 * opened here: none, not the list's last (DROPOUT); one in range as it was; an op's (a stream's, LIVE's) the same */
static void tgt_test(void)
{
    scene_t s;
    _Static_assert(sizeof s == FRS_SCENE_BYTES, "a scene's bytes, scene_t");
    frs_init(48000.0f);
    frs_scene_new((uint8_t *)&s);
    s.v[P_L1_T1] = (uint8_t)NLFO_TGT;   /* (one past this engine's last) */
    s.v[P_L1_T2] = (uint8_t)(NLFO_TGT + 21u);
    s.v[P_L2_T1] = (uint8_t)(NLFO_TGT - 1u);
    s.v[P_L1_SHAPE] = 255u;              /* (another past its range: its range's last, as ever) */
    frs_scene_take((const uint8_t *)&s);
    check(st.v[P_L1_T1] == 0u && st.v[P_L1_T2] == 0u, "a TARGET past this engine's list: none, not DROPOUT");
    check(st.v[P_L2_T1] == NLFO_TGT - 1u && st.v[P_L1_SHAPE] == PD[P_L1_SHAPE].max,
          "  one in range as it was; a value past its range its last");
    frs_param_set(P_L3_T4, NLFO_TGT + 5u);
    check(st.v[P_L3_T4] == 0u, "  an op's the same (FST_PARAM: a stream's, LIVE's)");
    frs_param_set(P_L3_T4, 3u);
    check(st.v[P_L3_T4] == 3u, "  an op's in range as it was");
    /* card #199: the LFOs' six after the LEAD's, the same numbers here (the LEAD's none); every factory scene's TARGETs
     * as they were, none of them */
    frs_scene_new((uint8_t *)&s);
    s.v[P_L1_T1] = 62u;                  /* (the LEAD's first and last) */
    s.v[P_L1_T2] = 83u;
    s.v[P_L1_T3] = (uint8_t)LFO_T_XM;
    s.v[P_L1_T4] = (uint8_t)(NLFO_TGT - 1u);
    s.v[P_L2_T1] = 61u;
    frs_scene_take((const uint8_t *)&s);
    check(!st.v[P_L1_T1] && !st.v[P_L1_T2] && st.v[P_L1_T3] == 84u && st.v[P_L1_T4] == 89u && st.v[P_L2_T1] == 61u,
          "  the LEAD's 62 .. 83 none here; the LFOs' 84 .. 89 kept (card #199); DROPOUT's 61 as it was");
    frs_param_set(P_L3_T4, 70u);
    check(!st.v[P_L3_T4], "  an op's the same (the LEAD's)");
    {
        uint32_t i, k;
        int same = 1, none = 1;
        for (i = 0; !frs_scene_factory(i, (uint8_t *)&s); i++) {
            frs_scene_take((const uint8_t *)&s);
            for (k = 0; k < 12u; k++) {
                uint32_t p = LFO_BASE[k / 4u] + 4u + k % 4u;
                same &= st.v[p] == s.v[p];
                none &= s.v[p] < LFO_T_XM;
            }
        }
        check(i == NFACTORY && same && none, "  every factory scene's TARGETs as they were, none on an LFO");
    }
    frs_close();
}

/* the LFOs on the LFOs (card #199: TARGETs 84 .. 89, each LFO's RATE and DEPTH). One at a time from a scene with its
 * TARGETs off: LFO 1 a SQUARE, UP or DOWN (its first half a constant offset), on LFO 2's RATE: free, RATE's Hz that
 * many steps on (in octaves), its phase going on; on the grid, its division's pace by as many octaves, from 16 BARS to
 * 1/16 at the most; on LFO 2's DEPTH: in its steps, to its top or to nothing (LFO 2 still turning); LFO 2 at DEPTH 0
 * opened by LFO 1 (not by itself, nor by an LFO at DEPTH 0); one pass later, eased (LFO_XM_TAU). Then all three on
 * each other and each on itself at x5.00, every SHAPE and POLARITY: two minutes, every offset finite and in its range,
 * never ringing at lfo_run's pace (the step from one pass to the next never turning back at once) */
static void xm_scene(void)
{
    uint8_t sc[FRS_SCENE_BYTES];
    uint32_t i;
    frs_init(44100.0f);
    frs_scene_new(sc);
    frs_scene_take(sc);
    for (i = 0; i < 4u; i++) {           /* (NEW SHARD's own TARGETs off) */
        param_set(P_L1_T1 + i, 0);
        param_set(P_L2_T1 + i, 0);
        param_set(P_L3_T1 + i, 0);
    }
}

static double lfo_turns(uint32_t i) { return (double)lfo[i].cyc + (double)lfo[i].ph; }

static double lfo2_pace(uint32_t ms)     /* (LFO 2's turns over ms, after a moment for the offset to settle) */
{
    double t0;
    ms_run(400);
    t0 = lfo_turns(1);
    ms_run(ms);
    return lfo_turns(1) - t0;
}

static void lfo1_on(uint32_t t, uint32_t pol, uint32_t amt)   /* LFO 1 a slow SQUARE (200 s a turn), at its top, on t */
{
    param_set(P_L1_SHAPE, 4);
    param_set(P_L1_RATE, 0);
    param_set(P_L1_DEPTH, 127);
    param_set(P_L1_POL, (int32_t)pol);
    param_set(P_L1_T1, (int32_t)t);
    param_set(P_L1_AMT1, (int32_t)amt);
}

static void xmod_test(void)
{
    double base, up, want;
    float steps = 0.08f * (float)PD[P_L2_RATE].max;   /* (x0.08 on RATE: 6 steps, some an octave) */
    uint32_t k, n, flips, steps_n, amt;
    int ok;
    char t[FRS_TEXT];
    check(LFO_TGT[LFO_T_XM] == P_L1_RATE && LFO_TGT[LFO_T_XM + 1u] == P_L1_DEPTH && LFO_TGT[LFO_T_XM + 4u] == P_L3_RATE &&
              LFO_TGT[NLFO_TGT - 1u] == P_L3_DEPTH && PD[P_L1_T1].max == 89u,
          "the LFOs' TARGETs: 84 .. 89, LFO 1's RATE, DEPTH .. LFO 3's DEPTH, the list's last");
    frs_param_text(P_L2_T3, LFO_T_XM + 3u, t);
    check(!strcmp(t, "LFO2 DEPTH"), "  their words: LFO2 DEPTH");

    /* free: RATE 30, 6 steps up */
    xm_scene();
    param_set(P_L2_SHAPE, 0);
    param_set(P_L2_RATE, 30);
    param_set(P_L2_DEPTH, 64);
    param_set(P_L2_T1, 1);               /* (TIMBRE) */
    base = lfo2_pace(10000);
    lfo1_on(LFO_T_XM + 2u, 1, 8);
    up = lfo2_pace(10000);
    want = (double)(lfo_hz(30.0f + steps) / lfo_hz(30.0f));
    printf("       LFO 2 free (RATE 30): %.3f turns in 10 s, %.3f with LFO 1 UP x0.08 on its RATE (%.2f steps, %.3f "
           "octaves): x%.4f, x%.4f wanted\n", base, up, (double)steps, (double)(steps * LFO_OCT), up / base, want);
    check(fabs(up / base / want - 1.0) < 0.01, "LFO 1 on LFO 2's free RATE: its Hz that many steps on, in octaves");
    param_set(P_L1_POL, 2);
    up = lfo2_pace(10000);
    check(fabs(up / base * want - 1.0) < 0.01, "  DOWN: as many octaves slower");
    param_set(P_L1_AMT1, 200);           /* (x5.00: 375 steps, the free range's ends) */
    param_set(P_L1_POL, 1);
    up = lfo2_pace(4000);
    check(fabs(up / (4.0 * (double)lfo_hz((float)(LFO_NFREE - 1u))) - 1.0) < 0.01, "  far up: 10 Hz, RATE's free top");
    param_set(P_L1_POL, 2);
    up = lfo2_pace(10000);
    check(fabs(up / (10.0 * (double)lfo_hz(0.0f)) - 1.0) < 0.02 && lfo[1].on, "  far down: 0.005 Hz, its free bottom, still on");
    frs_close();

    /* on the grid: 1/4 */
    xm_scene();
    param_set(P_L2_SHAPE, 0);
    param_set(P_L2_RATE, (int32_t)LFO_NFREE + 7);
    param_set(P_L2_DEPTH, 64);
    param_set(P_L2_T1, 1);
    base = lfo2_pace(10000);
    lfo1_on(LFO_T_XM + 2u, 1, 8);
    up = lfo2_pace(10000);
    printf("       LFO 2 on the grid (1/4): %.3f turns in 10 s, %.3f with LFO 1 UP x0.08 on its RATE: x%.4f\n", base, up,
           up / base);
    check(fabs(up / base / want - 1.0) < 0.01, "LFO 1 on LFO 2's RATE on the grid: the same octaves");
    param_set(P_L1_AMT1, 200);
    up = lfo2_pace(10000);
    check(fabs(up / base - 24.0 / 6.0) < 0.02, "  far up: 1/16 at the most");
    param_set(P_L1_POL, 2);
    up = lfo2_pace(10000);
    check(fabs(up / base - 24.0 / 1536.0) < 0.002, "  far down: 16 BARS at the least");
    frs_close();

    /* DEPTH */
    xm_scene();
    param_set(P_L2_SHAPE, 0);
    param_set(P_L2_RATE, 50);
    param_set(P_L2_DEPTH, 32);
    param_set(P_L2_T1, 1);
    ms_run(3000);
    lfo[1].peak = 0.0f;
    ms_run(3000);
    check(fabsf(lfo[1].peak - 32.0f / 127.0f) < 0.01f, "LFO 2 alone: DEPTH 32's");
    lfo1_on(LFO_T_XM + 3u, 1, 50);       /* (x0.50: 63.5 steps on) */
    ms_run(400);
    lfo[1].peak = 0.0f;
    ms_run(3000);
    check(fabsf(mod[P_L2_DEPTH] - 63.5f) < 0.01f && fabsf(lfo[1].peak - 95.5f / 127.0f) < 0.01f,
          "LFO 1 UP x0.50 on LFO 2's DEPTH: 63.5 of its steps on, settled");
    param_set(P_L1_AMT1, 100);
    ms_run(400);
    lfo[1].peak = 0.0f;
    ms_run(3000);
    check(lfo[1].peak > 0.999f, "  x1.00: at its top");
    param_set(P_L1_POL, 2);
    ms_run(400);
    lfo[1].peak = 0.0f;
    base = lfo_turns(1);
    ms_run(3000);
    check(lfo[1].peak == 0.0f && lfo[1].on && lfo_turns(1) > base + 1.0, "  DOWN: nothing, LFO 2 turning on");
    param_set(P_L1_POL, 1);
    param_set(P_L2_DEPTH, 0);            /* (opened) */
    ms_run(400);
    lfo[1].peak = 0.0f;
    ms_run(3000);
    check(lfo[1].on && lfo[1].peak > 0.999f, "  LFO 2 at DEPTH 0: on, LFO 1 opening it");
    param_set(P_L1_DEPTH, 0);
    ms_run(400);
    check(!lfo[1].on && !lfo[0].on, "  LFO 1 at DEPTH 0 too: both off");
    param_set(P_L1_T1, 0);
    param_set(P_L2_T2, LFO_T_XM + 3u);   /* (LFO 2 on its own DEPTH) */
    ms_run(400);
    check(!lfo[1].on, "  LFO 2 at DEPTH 0 on its own DEPTH: off (it never opens itself)");
    param_set(P_L2_T2, LFO_T_XM + 1u);   /* (LFO 2, at DEPTH 0, on LFO 1's DEPTH: neither opens the other) */
    param_set(P_L1_T1, LFO_T_XM + 3u);
    ms_run(400);
    check(!lfo[0].on && !lfo[1].on, "  two at DEPTH 0 on each other's: both off");
    frs_close();

    /* one pass later: LFO 1 on at once, its offset on LFO 2's DEPTH there only after lfo_run's next pass */
    xm_scene();
    param_set(P_L2_SHAPE, 0);
    param_set(P_L2_RATE, 50);
    param_set(P_L2_DEPTH, 32);
    param_set(P_L2_T1, 1);
    ms_run(200);
    lfo1_on(LFO_T_XM + 3u, 1, 100);
    for (k = 0; k < 8u && !lfo[0].on; k++)
        frs_render(L, R, 256);
    check(lfo[0].on && mod[P_L2_DEPTH] > 0.0f && mod[P_L2_DEPTH] < 0.31f * 127.0f,
          "  a pass later and eased: some of it (x1.00's 127 steps, 40 ms)");
    frs_close();

    /* all on each other, each on itself: x5.00 (the far ends), then x1.20 (moving) */
    for (amt = 200u; amt; amt = amt == 200u ? 104u : 0u) {
        float was[2u * NLFO], dwas[2u * NLFO], top = 0.0f;
        uint32_t j;
        xm_scene();
        for (k = 0; k < NLFO; k++) {
            param_set((int)LFO_BASE[k] + 2, 90);
            for (j = 0; j < 4u; j++)
                param_set(P_L1_AMT1 + 4 * (int)k + (int)j, (int32_t)amt);
        }
        param_set(P_L1_SHAPE, 0);        /* SINE BIPOLAR, free: on LFO 2's RATE and DEPTH, its own DEPTH, TIMBRE */
        param_set(P_L1_RATE, 40);
        param_set(P_L1_T1, LFO_T_XM + 2u);
        param_set(P_L1_T2, LFO_T_XM + 3u);
        param_set(P_L1_T3, LFO_T_XM + 1u);
        param_set(P_L1_T4, 1);
        param_set(P_L2_SHAPE, 6);        /* DRIFT DOWN (RANDOM's, eased): on LFO 1's RATE and DEPTH, LFO 3's DEPTH, its own
                                          * RATE */
        param_set(P_L2_RATE, 45);
        param_set(P_L2_POL, 2);
        param_set(P_L2_T1, LFO_T_XM);
        param_set(P_L2_T2, LFO_T_XM + 1u);
        param_set(P_L2_T3, LFO_T_XM + 5u);
        param_set(P_L2_T4, LFO_T_XM + 2u);
        param_set(P_L3_SHAPE, 1);        /* TRIANGLE BIPOLAR on the grid (1/8): on LFO 1's DEPTH, LFO 2's RATE, its own RATE
                                          * and DEPTH */
        param_set(P_L3_RATE, (int32_t)LFO_NFREE + 9);
        param_set(P_L3_T1, LFO_T_XM + 1u);
        param_set(P_L3_T2, LFO_T_XM + 2u);
        param_set(P_L3_T3, LFO_T_XM + 4u);
        param_set(P_L3_T4, LFO_T_XM + 5u);
        frs_latch(0, 48, FRS_CHORD_HARMONY);
        memset(was, 0, sizeof was);
        memset(dwas, 0, sizeof dwas);
        ok = 1;
        flips = steps_n = 0;
        for (n = 0; n < 12000u; n++) {   /* (two minutes, a pass of lfo_run at a time: 441 frames, 10 ms) */
            frs_render(L, R, 441);
            for (j = 0; j < 441u; j++)
                ok &= isfinite(L[j]) && isfinite(R[j]);
            for (j = 0; j < 2u * NLFO; j++) {
                float m = mod[LFO_BASE[j / 2u] + 1u + j % 2u], d = m - was[j];
                ok &= isfinite(m) && fm_fabsf(m) <= 3.0f * 5.0f * 127.0f;
                if (fm_fabsf(d) > 0.05f && fm_fabsf(dwas[j]) > 0.05f) {
                    steps_n++;
                    flips += (d > 0.0f) != (dwas[j] > 0.0f);
                }
                if (fm_fabsf(m) > top)
                    top = fm_fabsf(m);
                dwas[j] = d;
                was[j] = m;
            }
            for (j = 0; j < NLFO; j++)
                ok &= lfo[j].ph >= 0.0f && lfo[j].ph < 1.0f && lfo[j].peak <= 1.0f;
        }
        printf("       all three on each other and on themselves at x%.2f, two minutes: offsets up to %.1f steps, %u of %u "
               "moves turned back at once\n", (double)(100u + 4u * (amt - 100u)) / 100.0, (double)top, flips, steps_n);
        check(ok && lfo[0].on && lfo[1].on && lfo[2].on && top > 20.0f,
              amt == 200u ? "the LFOs on each other and on themselves at x5.00: every offset finite and in range, the sound "
                            "too (at the ranges' ends, most of the time)"
                          : "  at x1.16: the same, moving");
        if (amt != 200u)                 /* (at x5.00 pinned at the ends: little moves) */
            check(steps_n > 2000u && flips * 20u < steps_n,
                  "  never ringing at lfo_run's pace (under 5 % of its moves turned back at once)");
        frs_close();
    }
}

/* MIDI's transport (card #204): a DAW's clock at 120 BPM (48 kHz: a clock each 1000 frames), its Start, Stop, Continue
 * and Song Position, on a scene whose three sequences play sixteenths (a NOTE on each step) over a chord latched */
static void xclk(uint32_t n)              /* n clocks, each with its 1000 frames after it */
{
    while (n--) {
        frs_midi(0xF8, 0, 0);
        frs_render(L, R, 1000);
    }
}
static void xms(uint32_t ms)              /* ms without a clock */
{
    for (; ms >= 20u; ms -= 20u)
        frs_render(L, R, 960);
}
static uint8_t xp_scene[FRS_SCENE_BYTES];
static void xp_open(void)
{
    scene_t sc;
    uint32_t i;
    static const int8_t C0[5] = {48, 48, 52, 55, -1};
    frs_init(48000.0f);
    frs_scene_new(xp_scene);
    memcpy(&sc, xp_scene, sizeof sc);
    sc.v[P_SQ_TYPE] = SQT_SEQ;
    sc.v[P_ARP_RATE] = 7;                /* (sixteenths: ARP_TICKS[7], 6 clocks) */
    sc.v[P_ARP_CHANCE] = 127;
    sc.v[P_SQ_LEN] = 15;                 /* (16 steps) */
    sc.v[P_SQ_GATE] = 40;
    for (i = 0; i < SC_SEQ; i++)
        sc.seq[i] = 72u;
    for (i = 0; i < SC_BSEQ; i++)
        sc.bnote[i] = 36u;
    sc.bq_on = SQT_SEQ;
    sc.bq_steps = 15;
    sc.bq_len = 0;                       /* (a sixteenth: BQ_TICKS[0]) */
    sc.ba_chance = 127;
    sc.nch = 1;
    memcpy(sc.ch[0], C0, 5);
    memcpy(xp_scene, &sc, sizeof sc);
    frs_scene_take(xp_scene);
    for (i = 0; i < SC_DSEQ; i++)        /* (the drone's: sixteen sixteenths, a note each) */
        dnote[i][0] = 60u;
    param_set(P_DQ_RATE, 0);
    param_set(P_DQ_LEN, 15);
    param_set(P_DQ_CHANCE, 127);
    param_set(P_DQ_ON, 1);
    param_set(P_TEMPO, 60);              /* (TEMPO 90 BPM: the clock's 120 tells them apart) */
}

static void transport_test(void)
{
    frs_view_t v;
    uint8_t live[FRS_LIVE_BYTES];
    uint32_t gs, bs, ds, n, both;
    double e;
    xp_open();
    frs_midi(0xFA, 0, 0);                /* Start, then 33 clocks: step 5 under way (30 .. 35) */
    xclk(33);
    frs_view(&v);
    check(v.seq_run && v.bseq_run && v.dseq_run && v.seq_pos == 5 && v.bseq_pos == 5 && v.dseq_pos == 5 &&
              fabsf(v.tempo - 120.0f) < 1.0f,
          "MIDI Start and 33 clocks: the three sequences on their sixth step, at the clock's 120 BPM");

    frs_midi(0xFC, 0, 0);                /* Stop: the sequences stop, the drones sound on */
    xms(40);
    gs = sq.steps, bs = bq.steps, ds = dq.steps;
    frs_view(&v);
    check(!v.seq_run && !v.bseq_run && !v.dseq_run && bits4(v.latched) == 1u && xp.stop && xp.at == 33u,
          "MIDI Stop: the three sequences stopped, the chord still latched, the place kept (33 clocks)");
    e = rms(48000);
    xms(1500);                           /* (the clock gone: the grid on TEMPO meanwhile) */
    printf("       (the drones after the Stop: RMS %.4f)\n", e);
    check(e > 1e-4 && sq.steps == gs && bq.steps == bs && dq.steps == ds,
          "  the drones sounding on, no step played for two seconds");

    frs_midi(0xFB, 0, 0);                /* Continue, the clock again: on from 33, step 6 at 36 */
    xclk(2);
    frs_view(&v);
    check(v.seq_run && v.bseq_run && v.dseq_run && sq.steps == gs && bq.steps == bs && dq.steps == ds && !xp.stop &&
              v.grid > 34.5f && v.grid < 35.5f,
          "MIDI Continue: the sequences on, the grid at 35 two clocks on, the step under way silent");
    xclk(1);
    frs_view(&v);
    check(v.seq_pos == 6 && v.bseq_pos == 6 && v.dseq_pos == 6 && sq.steps == gs + 1u && bq.steps == bs + 1u &&
              dq.steps == ds + 1u,
          "  the clock at 36: each on its seventh step, where it left off (not from the top)");

    frs_midi(0xFC, 0, 0);                /* Stop, Song Position 203 (14 bits: 75, 1), Continue: step 203 % 16 = 11 */
    xms(300);
    frs_midi(0xF2, 75, 1);
    xms(40);
    frs_view(&v);
    check(!v.seq_run && xp.cued && xp.at == 1218u, "Song Position while stopped: kept for Continue (203 sixteenths)");
    gs = sq.steps;
    frs_midi(0xFB, 0, 0);
    xclk(1);
    frs_view(&v);
    check(v.seq_pos == 11 && v.bseq_pos == 11 && v.dseq_pos == 11 && sq.steps == gs + 1u && grid.wraps == 0u &&
              v.grid > 1218.5f && v.grid < 1219.5f,
          "  Continue: each on step 11 at once (on its start), the grid at 1218 and on");
    frs_midi(0xFC, 0, 0);
    frs_midi(0xF2, 50, 12);              /* (1586 sixteenths: 9516 clocks, the grid's wrap near) */
    frs_midi(0xFB, 0, 0);
    xclk(100);
    frs_view(&v);
    check(grid.wraps == 1u && v.grid > 15.5f && v.grid < 16.5f && v.seq_pos == (int)((1586u * 6u + 100u) / 6u % 16u) &&
              v.bseq_pos == (int)((1586u * 6u + 100u) % 96u / 6u),
          "  past the grid's wrap (9600 clocks): GLINT's step and the bass's where the song is");

    xclk(10);                            /* a Song Position while the clock comes (a loop, a jump): there at once */
    frs_midi(0xF2, 4, 0);
    xclk(1);
    frs_view(&v);
    check(v.seq_pos == 4 && v.bseq_pos == 4 && v.dseq_pos == 4 && !xp.cued && v.grid > 24.5f && v.grid < 25.5f,
          "Song Position while playing on the clock: the sequences there at once (step 4)");
    n = sq.steps;
    frs_midi(0xF2, 5, 0);
    xclk(1);
    check(sq.steps == n + 1u && sq.pos == 5, "  again, to 5: its step struck once");
    xclk(5);
    check(apart(1500, &both) == 0 && both > 200u, "  GLINT's sequence and the bass's on the same step after it");

    frs_midi(0xFA, 0, 0);                /* Start after all that: from the top, as before */
    xclk(1);
    frs_view(&v);
    check(v.seq_pos == 0 && v.bseq_pos == 0 && v.dseq_pos == 0 && v.grid < 1.5f, "MIDI Start: from the top, as it was");
    frs_track_play(1, 0);                /* GLINT stopped by the player (the iPad's): so through all of it */
    frs_midi(0xFC, 0, 0);
    frs_midi(0xF2, 8, 0);
    frs_midi(0xFB, 0, 0);
    xclk(7);
    frs_view(&v);
    check(!v.seq_run && v.bseq_run && v.bseq_pos == 9 && v.dseq_pos == 9,
          "GLINT stopped by the player: so through Stop, Song Position, Continue (the others on step 9)");
    frs_track_play(1, 1);
    frs_midi(0xFA, 0, 0);
    xclk(1);

    param_set(P_TRANSPORT, 1);           /* TEMPO ONLY: the clock leads the tempo, the transport ignored */
    xclk(40);
    frs_midi(0xFC, 0, 0);
    xclk(2);
    frs_view(&v);
    check(v.seq_run && v.bseq_run && v.dseq_run && !xp.stop && v.seq_pos == 7 && fabsf(v.tempo - 120.0f) < 1.0f,
          "TRANSPORT TEMPO ONLY: Stop ignored, the sequences on at the clock's 120 BPM");
    frs_midi(0xFA, 0, 0);
    frs_midi(0xF2, 0, 0);
    frs_midi(0xFB, 0, 0);
    xclk(1);
    frs_view(&v);
    check(v.seq_pos == 7 && v.grid > 43.5f && v.grid < 44.5f, "  Start, Song Position, Continue ignored: the grid on (44)");
    param_set(P_TRANSPORT, 0);
    param_set(P_CLOCK, 1);               /* CLOCK INTERNAL: none of it either */
    frs_midi(0xFC, 0, 0);
    xms(40);
    frs_view(&v);
    check(v.seq_run && !xp.stop, "CLOCK INTERNAL: Stop ignored");
    param_set(P_CLOCK, 0);

    /* the arpeggios: GLINT's on ARP waits with the Stop, on again with Continue; one PLAY had waiting stays so */
    param_set(P_SQ_TYPE, SQT_ARP);
    frs_latch(1, 67, FRS_CHORD_HARMONY);
    frs_midi(0xFA, 0, 0);
    xclk(24);
    frs_view(&v);
    n = arp.steps;
    check(!v.arp_paused && n > 2u, "GLINT on ARP, its arpeggio playing on the clock");
    frs_midi(0xFC, 0, 0);
    xms(1000);
    frs_view(&v);
    check(v.arp_paused && arp.steps == n && !v.dseq_run && bits4(v.latched) == 1u,
          "  MIDI Stop: it waits (nothing played for a second), the drones latched");
    {
        uint32_t ln = frs_live_save(live, sizeof live, xp_scene), at = xp.at;
        xp.stop = xp.arps = 0;
        xp.at = 0;
        check(ln > 0u && frs_live_load(live, ln, xp_scene) == 0 && xp.stop && xp.arps == 1u && xp.at == at,
              "  the running state carries it (LIVE: stopped, its place, the arpeggio it holds)");
    }
    frs_midi(0xFB, 0, 0);
    xclk(12);
    frs_view(&v);
    check(!v.arp_paused && arp.steps > n && v.dseq_run && !xp.arps, "  MIDI Continue: on again, the drone's sequence too");
    frs_play();                          /* (PLAY: the transport stopped by hand, the arpeggio waiting) */
    frs_view(&v);
    n = arp.steps;
    frs_midi(0xFC, 0, 0);
    frs_midi(0xFB, 0, 0);
    xclk(12);
    frs_view(&v);
    check(v.arp_paused && arp.steps == n, "  waiting by PLAY: Stop and Continue leave it waiting");
    frs_close();
}

int main(void)
{
    frs_view_t v;
    uint32_t i, any = 0;
    uint64_t first = swell_run();        /* (the first open of all) */
    /* the clock, near its wrap: four seconds across it */
    frs_init(48000.0f);
    frs_latch(0, 48, FRS_CHORD_HARMONY);
    rms(48000);
    now64 = 0xFFFFFF00u;   /* (a quarter of a second before the wrap) */
    due_ms = (double)now64;
    check(rms(4u * 48000u) > 1e-5, "the clock across 2^32 ms: it renders on, the chord sounding");
    check(now64 > 0xFFFFFFFFu && eng_ms() == (uint32_t)now64, "its milliseconds past 2^32, the brain's wrapped");
    frs_close();

    /* a note under the scale's lowest: ROOT D, MAJOR; notes 0 .. 11 */
    frs_init(44100.0f);
    param_set(P_ROOT, 2);
    param_set(P_SCALE, 1);
    param_set(P_KB, 0);
    param_set(P_KEYS, 0);
    for (i = 0; i < 12u; i++) {
        frs_key_down(i, (int)i, 100, 0);
        frs_key_up(i);
        any |= frs_latch(0, i, FRS_CHORD_HARMONY) == 1;
    }
    frs_view(&v);
    check(!any && !v.latched[0], "notes 0 .. 11 under D major's lowest: nothing latched (none written outside)");
    frs_key_down(0, 2, 100, 0);
    frs_key_up(0);
    frs_view(&v);
    check(v.latched[0] >> 2 & 1u, "D0 itself: latched");
    frs_close();

    /* the statics that carried time: as at the first open, after others (44.1, 48, 96 kHz, sounding) */
    frs_init(96000.0f);
    frs_swell(1);
    frs_latch(0, 52, FRS_CHORD_HARMONY);
    rms(96000);
    frs_close();
    check(swell_run() == first, "the swell held, opened again: the first open's samples (its last step started again)");
    tracks();
    fingers_test();
    metro_test();
    count_test();
    take_test();
    dseq_test();
    keys_test();
    keys_points_test();
    dq_knobs_test();
    rec_layer_test();
    metro_set_test();
    preset_test();
    amount_swell_test();
    tgt_test();
    xmod_test();
    transport_test();
    printf("inside: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
