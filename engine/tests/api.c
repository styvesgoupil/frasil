/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The engine's API alone (frasil_engine.h, nothing else of it), as an iPad or a plug-in calls it: its renders the same
 * whatever the frames asked at a time; one open at a time; a scene taken and stored; THE KEYS (latch, hold, clear); the
 * keys as the FM-1 has them (KEYS, KEY MODE); MIDI, time-stamped in the host's buffer; a host's tempo; a setting set,
 * edited, in words, of a kind; the state beside a scene; PANIC; the transport; the views, the snapshot.
 *   engine/tests/run.sh, or: api */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../frasil_engine.h"

static int fails;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "  ok" : "FAIL", what);
    fails += !ok;
}

static uint64_t fnv(const int32_t *lr, uint32_t n, uint64_t h)
{
    uint32_t i;
    for (i = 0; i < 2u * n; i++) {
        h ^= (uint32_t)lr[i];
        h *= 0x100000001B3u;
    }
    return h;
}

/* a factory scene played for `frames`, rendered `chunk` frames at a time: its hash */
static uint64_t played(uint32_t chunk, uint32_t frames)
{
    static int32_t lr[2 * 4096];
    uint8_t sc[FRS_SCENE_BYTES];
    uint64_t h = 0xCBF29CE484222325u;
    uint32_t done = 0;
    frs_close();
    frs_init(44100.0f);
    frs_scene_factory(0, sc);
    frs_scene_take(sc);
    while (done < frames) {
        uint32_t n = frames - done < chunk ? frames - done : chunk;
        frs_render_i24(lr, n);
        h = fnv(lr, n, h);
        done += n;
    }
    return h;
}

/* the bass alone on a note, its sine wide open, the cloud's return at `cloud`, its SPACE at `space`: the mean square of
 * its last second of two */
/* a glint held alone at AUTO VIB's value: its left channel, n frames from the strike */
static void glint_alone(uint32_t vib, int32_t *out, uint32_t n)
{
    static int32_t lr[2 * 256];
    uint8_t sc[FRS_SCENE_BYTES];
    uint32_t done = 0, i;
    frs_close();
    frs_init(48000.0f);
    frs_scene_new(sc);
    frs_scene_take(sc);
    frs_param_set(P_MX_DRONE, 0);
    frs_param_set(P_MX_BASS, 0);
    frs_param_set(P_SQ_TYPE, 0);
    frs_param_set(P_GL_SUSTAIN, 127);
    frs_param_set(P_GD_MIX, 0);
    frs_param_set(P_GL_VIB_AUTO, vib);
    frs_render_i24(lr, 256);
    frs_hold(1u, 72u, 1, 0);
    while (done < n) {
        frs_render_i24(lr, 256);
        for (i = 0; i < 256u && done < n; i++)
            out[done++] = lr[2u * i];
    }
    frs_hold(1u, 72u, 0, 0);
}

static double bass_alone(uint32_t space, uint32_t cloud, uint32_t note)
{
    static int32_t lr[2 * 4096];
    uint8_t sc[FRS_SCENE_BYTES];
    double e = 0.0;
    uint32_t done = 0, i;
    frs_close();
    frs_init(48000.0f);
    frs_scene_new(sc);
    frs_scene_take(sc);
    frs_param_set(P_MX_DRONE, 0);
    frs_param_set(P_MX_GLINT, 0);
    frs_param_set(P_MX_CLOUD, cloud);
    frs_param_set(P_MS_RATIO, 0);
    frs_param_set(P_PUMP_RATE, 0);
    frs_param_set(P_BS_MODEL, 0);
    frs_param_set(P_BS_FILTER, 127);
    frs_param_set(P_BS_SPACE, space);
    frs_render_i24(lr, 4096);
    frs_latch(2u, note, 0);
    while (done < 96000u) {
        frs_render_i24(lr, 4096);
        done += 4096u;
        if (done > 48000u)
            for (i = 0; i < 8192u; i++)
                e += (double)lr[i] * (double)lr[i];
    }
    return e * (1.0 / 16777216.0) * (1.0 / 16777216.0);
}

static float l_[4096], r_[4096];
static float rms(uint32_t frames)        /* (the mean square) */
{
    double e = 0.0;
    uint32_t done = 0, i;
    while (done < frames) {
        uint32_t n = frames - done < 4096u ? frames - done : 4096u;
        frs_render(l_, r_, n);
        for (i = 0; i < n; i++)
            e += (double)l_[i] * l_[i] + (double)r_[i] * r_[i];
        done += n;
    }
    return (float)(e / (2.0 * frames));
}

static int param(const char *name, uint32_t nth)   /* a setting by its name (the nth of that name) */
{
    uint32_t p, k = 0;
    const char *n;
    for (p = 0; p < frs_param_count(); p++)
        if (!frs_param_info(p, &n, 0, 0) && !strcmp(n, name) && k++ == nth)
            return (int)p;
    return -1;
}

static int bits(const uint32_t *b)
{
    int n = 0, i;
    for (i = 0; i < 128; i++)
        n += (int)(b[i >> 5] >> (i & 31) & 1u);
    return n;
}

static int rung(void)                    /* GLINT's voices sounding (frs_live's levels, 20..23) */
{
    float live[FRS_LIVE];
    int n = 0, i;
    frs_live(live);
    for (i = 0; i < 4; i++)
        n += live[20 + i] > 0.0f;
    return n;
}

static void passes(int k)                /* k tenths of a second */
{
    while (k-- > 0)
        rms(4800);
}

static int voices(void)                  /* the drone's voices sounding (frs_live's first eight) */
{
    float live[FRS_LIVE];
    int n = 0, i;
    frs_live(live);
    for (i = 0; i < 8; i++)
        n += live[i] >= 0.0f;
    return n;
}

static char said[48];
static void on_say(void *user, const char *a, const char *b)
{
    (void)user;
    snprintf(said, sizeof said, "%s %s", a, b);
}

/* a host at 48 kHz, its MIDI clock at 120 BPM time-stamped in its buffers of `buf` frames: the tempo the engine finds */
static float clocked(uint32_t buf)
{
    frs_view_t v;
    double per = 48000.0 * 60.0 / (120.0 * 24.0), next = 0.0;   /* frames a clock */
    uint64_t at = 0;
    uint32_t k;
    frs_close();
    frs_init(48000.0f);
    for (k = 0; k < 8u * 48000u / buf; k++) {
        while (next < (double)(at + buf)) {
            frs_midi_at((uint32_t)(next - (double)at), 0xF8, 0, 0);
            next += per;
        }
        rms(buf);
        at += buf;
    }
    frs_view(&v);
    return v.tempo;
}

int main(void)
{
    uint8_t sc[FRS_SCENE_BYTES], st[FRS_SCENE_BYTES], state[FRS_STATE_BYTES];
    char t[FRS_TEXT];
    frs_view_t v, w;
    float live[FRS_LIVE];
    int kb = param("KEYS", 0), keys = param("KEY MODE", 0), preset = param("PRESET", 1), sqtype = param("SEQ", 0);
    int clear = param("CLEAR", 0), note = param("NOTE", 0), freeze = param("FREEZE", 0), drone_ch = param("DRONE CH", 0);
    uint32_t n, k;
    uint64_t a = played(256, 441000), b = played(97, 441000), c = played(4096, 441000);
    check(a == b && b == c, "a scene's 10 s the same, 256, 97 or 4096 frames at a time");
    check(frs_init(48000.0f) == -2, "one open at a time: a second refused");
    frs_close();
    check(frs_init(22050.0f) == -1 && frs_init(192000.0f) == -1 && frs_init(48000.0f) == 0, "rates: 44.1 to 96 kHz");
    check(frs_rate() == 48000.0f, "its rate");
    check(!strcmp(frs_version(), FRS_VERSION) && frs_version()[0] >= '2', "its version (FRS_VERSION)");
    frs_callbacks(on_say, 0, 0);
    check(rms(4800) < 1e-9f, "nothing plays at the start (NEW SHARD)");
    check(frs_latch(0, 48, FRS_CHORD_HARMONY) == 1 && frs_latch(0, 55, FRS_CHORD_HARMONY) == 1, "THE KEYS: two chords latched");
    frs_view(&v);
    check((v.latched[1] >> (48 - 32) & 1u) && (v.latched[1] >> (55 - 32) & 1u), "the view: their roots latched");
    check(rms(24000) > 1e-6f, "they sound");
    check(frs_snapshot(&w, live) == 0 && w.latched[1] == v.latched[1] && w.ms > 0, "the snapshot: the same, after a block");
    frs_live(live);
    check(live[0] >= 0.0f, "the live view: a drone voice");
    check(frs_latch(0, 48, FRS_CHORD_HARMONY) == 0, "a root again: let go");
    frs_clear(0);
    frs_view(&v);
    check(!v.latched[0] && !v.latched[1] && !v.latched[2] && !v.latched[3], "CLEAR: none latched");
    frs_hold(2, 36, 1, FRS_CHORD_HARMONY);
    frs_view(&v);
    check(v.bass >= 0, "HOLD on the bass: it sounds");
    frs_hold(2, 36, 0, FRS_CHORD_HARMONY);
    frs_view(&v);
    check(v.bass < 0, "let go: silent");
    frs_param_set((uint32_t)kb, 0);
    frs_param_set((uint32_t)keys, 0);
    frs_key_down(3, 50, 100, 0);
    frs_key_up(3);
    frs_view(&v);
    check(v.latched[1] >> (50 - 32) & 1u, "a key (LATCH): latched");
    frs_param_set((uint32_t)keys, 1);
    frs_view(&v);
    check(!v.latched[1], "KEY MODE to HOLD lets the latched go");
    frs_key_down(4, 52, 100, 0);
    frs_view(&v);
    check(v.held[1] >> (52 - 32) & 1u, "a key (HOLD): its chord while down");
    frs_key_up(4);
    frs_view(&v);
    check(!(v.held[1] >> (52 - 32) & 1u), "up: gone");
    frs_param_set((uint32_t)keys, 0);
    frs_midi(0x90, 45, 100);
    rms(480);
    frs_view(&v);
    check(v.latched[0] == 0 && (v.held[1] >> (45 - 32) & 1u), "MIDI: a note on the drones' channel, its chord held");
    frs_midi(0x80, 45, 0);
    rms(480);
    frs_param_text((uint32_t)preset, 3, t);
    check(!strcmp(t, "GLACIER"), "the drone's PRESET 3 in words: GLACIER");
    frs_param_set((uint32_t)preset, 3);
    check(frs_param_get((uint32_t)preset) == 3 && frs_dirty(), "a setting set: the sound changed");
    check(frs_param_kind((uint32_t)preset) == FRS_P_SCENE && frs_param_kind((uint32_t)keys) == FRS_P_STATE &&
              frs_param_kind((uint32_t)clear) == FRS_P_ACTION && frs_param_kind((uint32_t)note) == FRS_P_STEP &&
              frs_param_kind((uint32_t)freeze) == FRS_P_MOMENT && frs_param_kind((uint32_t)param("-", 0)) == FRS_P_NONE,
          "the settings' kinds: in a scene, beside it, an action, a step's, of the moment, none");
    frs_param_set((uint32_t)sqtype, 1);
    check(frs_latch(1, 72, FRS_CHORD_HARMONY) == 1, "GLINT's arpeggio: a note in");
    frs_latch(0, 48, FRS_CHORD_HARMONY);
    frs_scene_store(st, 0);
    check(!frs_dirty(), "stored: nothing changed since");
    frs_scene_take(st);
    frs_view(&v);
    check((v.latched[1] >> (48 - 32) & 1u) && v.narp == 1, "taken again: its chord and its arpeggio");
    frs_scene_factory(1, sc);
    frs_scene_play(sc, 1);
    check(rms(48000) > 1e-6f, "a factory scene played");
    frs_release_all();
    frs_view(&v);
    check(!v.latched[0] && !v.latched[1] && !v.latched[2] && !v.latched[3] && v.bass < 0, "RELEASE ALL");
    frs_release_all();
    frs_view(&v);
    check(v.latched[0] | v.latched[1] | v.latched[2] | v.latched[3], "again: RESTORE");
    frs_play();
    check(said[0] != 0, "the engine's words (PLAY)");

    /* the state beside a scene */
    frs_param_set((uint32_t)keys, 1);
    frs_param_set((uint32_t)drone_ch, 5);
    frs_param_set(P_ARP_SNAP, 5);
    n = frs_state_save(state);
    frs_param_set((uint32_t)keys, 0);
    frs_param_set((uint32_t)drone_ch, 1);
    frs_param_set(P_ARP_SNAP, 3);
    check(n <= FRS_STATE_BYTES && frs_state_load(state, n) == 0 && frs_param_get((uint32_t)keys) == 1 &&
              frs_param_get((uint32_t)drone_ch) == 5 && frs_param_get(P_ARP_SNAP) == 5,
          "the state beside a scene: saved, back (KEY MODE, DRONE CH, ARP SNAP)");
    frs_param_set(P_ARP_SNAP, 3);
    state[0] = 'X';
    check(frs_state_load(state, n) == -1, "a record that isn't one: refused");
    frs_param_set((uint32_t)keys, 0);

    /* NaN for a gain: silent while it lasts, not for good */
    frs_latch(0, 60, FRS_CHORD_HARMONY);
    rms(4096);
    frs_gain(NAN);
    rms(8192);
    check(rms(4096) == 0.0f, "a NaN gain: silent");
    frs_gain(1.0f);
    rms(4096);
    check(rms(4096) > 1e-6f, "a gain again: it sounds");

    /* PANIC: all of it, at once; a chord after it sounds whole */
    frs_param_set((uint32_t)sqtype, 1);
    frs_latch(0, 48, FRS_CHORD_HARMONY);
    frs_latch(1, 84, FRS_CHORD_HARMONY);
    frs_latch(1, 88, FRS_CHORD_HARMONY);
    frs_midi(0x90, 40, 100);
    frs_midi(0x91, 76, 100);
    rms(48000);
    frs_panic();
    rms(256);                            /* (the block rendered before it) */
    frs_view(&v);
    check(rms(48000) < 1e-12f && !bits(v.latched) && !bits(v.held) && !v.narp && !v.seq_run && v.bass < 0 &&
              voices() == 0, "PANIC: silent, nothing latched, held or arpeggiated, the echoes and the cloud gone");
    frs_latch(0, 52, FRS_CHORD_HARMONY);
    rms(4096);
    frs_view(&v);
    check(bits(v.held) > 1 && voices() == bits(v.held), "a chord after PANIC: every note of it sounds");

    /* MIDI clock time-stamped in the host's buffers: its tempo, whatever the buffer */
    check(fabsf(clocked(256) - 120.0f) < 0.5f && fabsf(clocked(1024) - 120.0f) < 0.5f &&
              fabsf(clocked(2048) - 120.0f) < 0.5f && fabsf(clocked(333) - 120.0f) < 0.5f,
          "MIDI clock at 120 BPM, time-stamped (buffers of 256, 1024, 2048, 333): 120 BPM followed");

    /* a host's tempo and transport */
    frs_close();
    frs_init(48000.0f);
    for (k = 0; k < 8u * 48000u / 1024u; k++) {
        frs_host_tempo(97.0, (double)k * 1024.0 / 48000.0 * 97.0 / 60.0, 1);
        rms(1024);
    }
    frs_view(&v);
    check(fabsf(v.tempo - 97.0f) < 0.5f && v.grid > 0.0f, "a host at 97 BPM, playing: the grid follows it");
    for (k = 0; k < 48000u / 1024u; k++) {
        frs_host_tempo(97.0, 0.0, 0);
        rms(1024);
    }
    frs_view(&v);
    check(fabsf(v.tempo - (float)(frs_param_get((uint32_t)param("TEMPO", 0)) + 30u)) < 0.01f,
          "stopped: TEMPO again");

    /* the host locates (2 beats ahead, 4 back), changes its tempo, gives a beat out of range: its tempo held */
    frs_close();
    frs_init(48000.0f);
    check(frs_snapshot(&w, 0) == -1, "opened again: no snapshot before its first block");
    frs_param_set((uint32_t)param("RATE", 2), 3);   /* (PUMP's, the third RATE: on the beat) */
    {
        double beat = 0.0, t1 = 0.0, t2 = 0.0, t3 = 0.0, bpm = 120.0;
        float worst = 0.0f;
        for (k = 0; k < 3000u; k++) {
            if (k == 1000u)
                beat += 2.0;
            if (k == 1500u)
                beat -= 4.0;
            if (k == 2000u)
                bpm = 140.0;
            frs_host_tempo(bpm, beat, 1);
            rms(512);
            beat += 512.0 / 48000.0 * bpm / 60.0;
            frs_view(&v);
            if (k > 10u && k < 2000u && fabsf(v.tempo - 120.0f) > worst)
                worst = fabsf(v.tempo - 120.0f);
            if (k == 1000u)
                t1 = v.tempo;
            if (k == 1010u)
                t2 = v.tempo;
            if (k == 2000u)
                t3 = v.tempo;
        }
        check(worst < 0.01f && fabs(t1 - 120.0) < 0.01 && fabs(t2 - 120.0) < 0.01,
              "a host locating 2 beats ahead, then 4 back: 120 BPM held, at once and after");
        check(fabs(t3 - 140.0) < 0.01, "the host's tempo to 140: 140 at once");
        frs_host_tempo(120.0, 1e300, 1);
        rms(4096);
        frs_host_tempo(120.0, -INFINITY, 1);
        rms(4096);
        frs_host_tempo(120.0, NAN, 1);
        check(rms(4096) == rms(4096) && frs_ms() > 0u, "a beat of 1e300, -inf, NaN: it renders on");
        frs_host_tempo(5.0, 0.0, 1);
        rms(48000);
        frs_view(&v);
        check(fabsf(v.tempo - (float)(frs_param_get((uint32_t)param("TEMPO", 0)) + 30u)) < 0.01f,
              "a host at 5 BPM: under 13, TEMPO");
    }

    /* 2.5: the settings by name (frasil_params.h), their kinds; LFO TARGET's choices; TRACKS; CUT; PLAY from silence;
     * the effects as they sound (frs_live_fx, the snapshot's, frs_fx_response) */
    frs_close();
    frs_init(48000.0f);
    check(frs_param_count() == NPARAM && param("CLOCK", 0) == P_CLOCK && param("CUTOFF", 0) == P_FL_CUT &&
              param("SATURATION", 0) == P_TP_SAT && param("CLOUD PUMP", 0) == P_PUMP_CLOUD &&
              param("MID FREQ", 0) == P_EQ_MIDF && param("KEY MODE", 0) == P_KEYS,
          "frasil_params.h: the settings as their names have them");
    check(frs_param_kind(P_EQ_PRESET) == FRS_P_SCENE && frs_param_kind(P_FL_CUT) == FRS_P_SCENE &&
              frs_param_kind(P_TP_TRACKS) == FRS_P_SCENE && frs_param_kind(P_FL_PRE) == FRS_P_SCENE &&
              frs_param_kind(P_PN_CLOUD) == FRS_P_SCENE && frs_param_kind(P_CLOCK) == FRS_P_STATE &&
              frs_param_kind(P_SPEAKER) == FRS_P_NONE && frs_param_kind(P_ANIM) == FRS_P_NONE,
          "2.5's kinds: the effects and the pans in a scene, CLOCK beside it, SPK/JACK and ANIMATIONS none");
    frs_param_set(P_CLOCK, 1);
    n = frs_state_save(state);
    frs_param_set(P_CLOCK, 0);
    check(n <= FRS_STATE_BYTES && frs_state_load(state, n) == 0 && frs_param_get(P_CLOCK) == 1,
          "CLOCK beside a scene: saved, back");
    for (k = 0; k < 80u; k++)            /* (CLOCK INTERNAL: a host's tempo not followed) */
        frs_host_tempo(97.0, (double)k * 1024.0 / 48000.0 * 97.0 / 60.0, 1), rms(1024);
    frs_view(&v);
    check(fabsf(v.tempo - (float)(frs_param_get(P_TEMPO) + 30u)) < 0.01f, "CLOCK INTERNAL: a host at 97 BPM, TEMPO");
    frs_host_tempo(0.0, 0.0, 0);
    frs_param_set(P_CLOCK, 0);
    frs_param_text(P_L1_T1, 41, t);
    check(frs_lfo_targets() == 55u && frs_lfo_target(0) == -1 && frs_lfo_target(56) == -1 &&
              frs_lfo_target(1) == P_TIMBRE && frs_lfo_target(41) == P_FL_CUT && frs_lfo_target(42) == P_FL_RESO &&
              frs_lfo_target(43) == P_PN_DRONE && frs_lfo_target(46) == P_PN_CLOUD &&
              frs_lfo_target(47) == P_CH_SOFT && !strcmp(t, "MS CUTOFF") &&
              frs_lfo_target(48) == P_GL_ATTACK && frs_lfo_target(49) == P_GL_DECAY && frs_lfo_target(50) == P_GL_SUSTAIN &&
              frs_lfo_target(51) == P_GL_RELEASE && frs_lfo_target(52) == P_BS_ATTACK && frs_lfo_target(53) == P_BS_DECAY &&
              frs_lfo_target(54) == P_BS_SUSTAIN && frs_lfo_target(55) == P_BS_RELEASE,
          "LFO TARGET: 55 choices, 41 MS CUTOFF .. 46 CLOUD PAN, 47 CH SOFT (2.6's) as they were, 48 .. 55 the ADSRs, OFF none");
    frs_param_text(P_FL_TRACKS, FRS_TRACKS_GLINT_BASS, t);
    check(frs_tracks_bits(FRS_TRACKS_ALL) == 7u && frs_tracks_bits(FRS_TRACKS_DRONE) == FRS_TRK_DRONE &&
              frs_tracks_bits(FRS_TRACKS_BASS) == FRS_TRK_BASS &&
              frs_tracks_bits(FRS_TRACKS_DRONE_GLINT) == (FRS_TRK_DRONE | FRS_TRK_GLINT) &&
              frs_tracks_bits(FRS_TRACKS_GLINT_BASS) == (FRS_TRK_GLINT | FRS_TRK_BASS) && !strcmp(t, "GLINT+BASS"),
          "TRACKS: its values' tracks (GLINT+BASS: 6)");
    {
        float fx[FRS_LIVE_FX], fy[FRS_LIVE_FX];
        frs_live_fx(fx);
        check(fx[0] == 0.0f && fx[9] == 0.0f && fx[14] == 0.0f && frs_fx_response(fx, 0, 1000.0f, 48000.0f) == 0.0f &&
                  frs_fx_response(fx, 1, 1000.0f, 48000.0f) == 0.0f,
              "NEW SHARD's effects: the tape, the filter and the EQ out, flat");
        frs_param_set(P_TP_ON, 1);
        frs_param_set(P_FL_TYPE, 1);
        frs_param_set(P_FL_TRACKS, FRS_TRACKS_DRONE_BASS);
        frs_param_set(P_EQ_PRESET, 1);   /* (CUSTOM: its bands flat) */
        frs_param_set(P_EQ_LOW, 0);      /* (-12 dB at 120 Hz) */
        frs_param_set(P_EQ_HIGH, 48);    /* (+12 dB above 8 kHz) */
        frs_latch(0, 48, FRS_CHORD_HARMONY);
        frs_latch(2, 36, FRS_CHORD_HARMONY);
        rms(48000);
        frs_live_fx(fx);
        check(fx[0] > 0.99f && fx[1] > 0.9f && fx[1] < 1.1f && fx[8] > 0.99f && fx[9] == 1.0f && fx[10] > 15000.0f &&
                  fx[12] == (float)(FRS_TRK_DRONE | FRS_TRK_BASS) && fx[13] == 0.0f && fx[14] > 0.99f,
              "the effects in: the tape's mix and speed, the filter's type, cutoff, tracks, the EQ's mix");
        check(fx[32] > 0.0f && fx[33] > 0.0f && fx[34] > 0.0f && fx[35] > 0.0f && fx[34] <= fx[32] && fx[30] <= 0.0f,
              "the output's peak and RMS, left and right; MASTERING's reduction");
        check(fabsf(frs_fx_response(fx, 1, 30.0f, 48000.0f) + 12.0f) < 1.0f &&
                  fabsf(frs_fx_response(fx, 1, 1000.0f, 48000.0f)) < 1.5f &&
                  fabsf(frs_fx_response(fx, 1, 18000.0f, 48000.0f) - 12.0f) < 1.0f &&
                  fabsf(frs_fx_response(fx, 0, 100.0f, 48000.0f)) < 0.1f,
              "frs_fx_response: the EQ's low shelf down 12 dB, its middle flat, its high shelf up 12; the filter open");
        frs_param_set(P_FL_CUT, 64);
        rms(24000);
        frs_live_fx(fx);
        check(frs_snapshot_fx(fy) == 0 && !memcmp(fx, fy, sizeof fx), "the snapshot's: the same, after a block");
        check(fx[10] > 200.0f && fx[10] < 2000.0f && frs_fx_response(fx, 0, fx[10] / 4.0f, 48000.0f) > -1.0f &&
                  frs_fx_response(fx, 0, fx[10] * 8.0f, 48000.0f) < -24.0f,
              "the filter half shut: open under its cutoff, 24 dB down three octaves over");
        frs_param_set(P_TP_ON, 0);
        frs_param_set(P_FL_TYPE, 0);
        frs_param_set(P_EQ_PRESET, 0);
        rms(48000);
        frs_live_fx(fx);
        check(fx[0] < 0.01f && fx[8] < 0.01f && fx[14] < 0.01f, "out again: their mixes down");
    }
    frs_release_all();                   /* (CUT: the scene let go, then everything emptied) */
    frs_release_all();
    frs_param_set(P_ESPACE, 127);
    frs_param_set(P_GD_MIX, 127);
    {
        float was = rms(48000), now;
        frs_cut();
        rms(512);                        /* (the faded block, the silent one) */
        frs_view(&v);
        now = rms(48000);                /* (2.5.1's, the FM-1's too: what the cloud's lines' filters and the mix's
                                          * kept rings on, some 35 dB under, falling) */
        check(now < 1e-3f * was && rms(48000) < now && !bits(v.latched) && !bits(v.held) && v.bass < 0,
              "CUT: the chord, the bass, the cloud's tail and the echoes gone");
    }
    frs_play();
    frs_view(&v);
    check(bits(v.latched) && v.bass >= 0 && rms(24000) > 1e-6f, "PLAY from silence: what CUT let go plays again");
    frs_scene_factory(1, sc);
    frs_scene_play(sc, 1);
    rms(4096);
    frs_panic();                         /* (nothing let go to bring back: the scene's own notes, then) */
    rms(4096);
    frs_view(&v);
    k = (uint32_t)bits(v.latched);
    frs_play();
    frs_view(&v);
    check(k == 0u && (bits(v.latched) || v.narp || v.bass >= 0) && rms(24000) > 1e-6f,
          "PLAY after PANIC: the scene given last's own notes");

    /* BASS TONE: a scene's, at 0 as a scene without it; FRSv's 319 bytes made over, its arpeggio's notes moved on */
    frs_close();
    frs_init(48000.0f);
    frs_scene_new(sc);
    frs_scene_take(sc);
    rms(4800);
    {
        uint8_t t[FRS_SCENE_BYTES], o[FRS_SCENE_BYTES];
        check(!sc[302] && !sc[303] && !sc[304], "BASS TONE: at 0 in NEW SHARD");
        frs_param_set(P_BS_WEIGHT, 90);
        frs_param_set(P_BS_PUNCH, 40);
        frs_param_set(P_BS_BITE, 20);
        rms(4800);
        check(frs_dirty(), "  turned: the scene changed");
        frs_scene_store(t, sc);
        check(t[302] == 90u && t[303] == 40u && t[304] == 20u, "  stored");
        frs_scene_take(sc);
        rms(4800);
        check(!frs_param_get(P_BS_WEIGHT) && !frs_param_get(P_BS_PUNCH) && !frs_param_get(P_BS_BITE), "  a scene without it: 0");
        frs_scene_take(t);
        rms(4800);
        check(frs_param_get(P_BS_WEIGHT) == 90u && frs_param_get(P_BS_PUNCH) == 40u && frs_param_get(P_BS_BITE) == 20u, "  taken");
        memset(o, 0, sizeof o);
        memcpy(o, t, 302);
        o[302] = 2;                      /* (FRSv: the arpeggio's notes here) */
        o[303] = 40;
        o[304] = 52;
        check(!frs_scene_over(o, 319) && !o[302] && !o[303] && !o[304] && o[305] == 2u && o[306] == 40u && o[307] == 52u,
              "  FRSv's: BASS TONE at 0, the notes after it");
    }

    /* the bass's SPACE: the scene's, its last byte, 0 in NEW SHARD; its share in the cloud's return alone, the dry as it
     * was to the bit, its low end left out */
    {
        uint8_t t[FRS_SCENE_BYTES], o[FRS_SCENE_BYTES];
        double d36, w36, d72, w72;
        frs_close();
        frs_init(48000.0f);
        frs_scene_new(sc);
        check(!sc[322], "the bass's SPACE: 0 in NEW SHARD");
        frs_scene_take(sc);
        rms(4800);
        check(frs_param_kind(P_BS_SPACE) == FRS_P_SCENE, "  the scene's");
        frs_param_set(P_BS_SPACE, 77);
        rms(4800);
        check(frs_dirty(), "  turned: the scene changed");
        frs_scene_store(t, sc);
        check(t[322] == 77u, "  stored");
        frs_scene_take(sc);
        rms(4800);
        check(!frs_param_get(P_BS_SPACE), "  a scene without it: 0");
        frs_scene_take(t);
        rms(4800);
        check(frs_param_get(P_BS_SPACE) == 77u, "  taken");
        memset(o, 0xEE, sizeof o);
        memcpy(o, t, 322);
        check(!frs_scene_over(o, 322) && !o[322] && !o[323] && !memcmp(o, t, 322), "  FRSw's 322 bytes: 0, the rest as it was");
        check(!sc[323], "AUTO VIB: 0 in NEW SHARD");
        check(frs_param_kind(P_GL_VIB_AUTO) == FRS_P_SCENE, "  the scene's");
        frs_param_set(P_GL_VIB_AUTO, 55);
        rms(4800);
        check(frs_dirty(), "  turned: the scene changed");
        frs_scene_store(t, sc);
        check(t[323] == 55u, "  stored");
        memset(o, 0xEE, sizeof o);
        memcpy(o, t, 323);
        check(!frs_scene_over(o, 323) && !o[323] && !memcmp(o, t, 323), "  FRSx's 323 bytes: 0, the rest as it was");
        frs_scene_take(sc);
        rms(4800);
        check(!frs_param_get(P_GL_VIB_AUTO), "  a scene without it: 0");
        frs_scene_take(t);
        rms(4800);
        check(frs_param_get(P_GL_VIB_AUTO) == 55u, "  taken");
        frs_param_set(P_GL_VIB_AUTO, 0);
        rms(4800);
        frs_param_set(P_BS_SPACE, 77);
        rms(4800);
        {                                /* (a glint: the same at 0, straight at its strike, bent once it has come in) */
            static int32_t a0[48000], b0[48000], up[48000];
            double early = 0.0, late = 0.0;
            uint32_t i;
            glint_alone(0, a0, 48000u);
            glint_alone(0, b0, 48000u);
            glint_alone(127, up, 48000u);
            check(!memcmp(a0, b0, sizeof a0), "AUTO VIB at 0: a glint as it is, to the bit, twice over");
            for (i = 0; i < 2400u; i++)
                early += fabs((double)up[i] - (double)a0[i]);
            for (i = 24000u; i < 26400u; i++)
                late += fabs((double)up[i] - (double)a0[i]);
            check(late > 0.0 && early < 0.1 * late, "  at the top: straight at the strike (50 ms), bent later (0.5 s on)");
            frs_close();
            frs_init(48000.0f);
            frs_scene_new(sc);
            frs_scene_take(sc);
            rms(4800);
            frs_param_set(P_BS_SPACE, 77);
            rms(4800);
        }
        d36 = bass_alone(0, 0, 48);
        w36 = bass_alone(127, 0, 48);
        check(d36 > 0.0 && fabs(w36 / d36 - 1.0) < 1e-6, "  the cloud's return down: the dry the same, up or not");
        d36 = bass_alone(0, 100, 36);
        w36 = bass_alone(127, 100, 36) - d36;
        d72 = bass_alone(0, 100, 72);
        w72 = bass_alone(127, 100, 72) - d72;
        check(d36 > 0.0 && d72 > 0.0 && w72 > 0.002 * d72, "  up: the cloud has it");
        check(w36 / d36 < 0.5 * (w72 / d72), "  a low note: much less of it");
    }

    /* the LFOs' ADSR targets: GL SUSTAIN and a voice held; a saved TARGET keeps its setting */
    {
        float lv[2], live[FRS_LIVE];
        uint32_t k;
        char w[FRS_TEXT];
        for (k = 0; k < 2u; k++) {
            frs_close();
            frs_init(48000.0f);
            frs_scene_new(sc);
            frs_scene_take(sc);
            frs_param_set(P_SQ_TYPE, 0);
            frs_param_set(P_GL_SUSTAIN, 127);
            frs_param_set(P_L1_RATE, 0);
            frs_param_set(P_L1_POL, 2);
            frs_param_set(P_L1_DEPTH, 127);
            frs_param_set(P_L1_T1, k ? 50u : 0u);
            frs_render(l_, r_, 4096);
            frs_hold(1u, 76u, 1, 0);
            rms(96000);
            frs_live(live);
            lv[k] = live[20];
            frs_hold(1u, 76u, 0, 0);
        }
        check(lv[0] > 0.0f && lv[1] > 0.0f && lv[1] < lv[0], "GL SUSTAIN: an LFO's DOWN holds the voice lower");
        frs_param_set(P_L1_T1, 47);
        frs_param_text(P_L1_T1, frs_param_get(P_L1_T1), w);
        check(!strcmp(w, "CH SOFT") && frs_lfo_target(47) == P_CH_SOFT, "  a saved TARGET, 47: as it was");
    }

    /* GLINT off ARP: its notes kept, silent, CLEAR on the sequence leaving them; ARP again, there; PLAY sees them only
     * on ARP */
    frs_close();
    frs_init(48000.0f);
    frs_scene_factory(0, sc);
    frs_scene_take(sc);
    rms(4800);
    frs_view(&w);
    frs_param_set(P_SQ_TYPE, 2);
    rms(4800);
    frs_view(&v);
    check(w.narp > 0 && v.narp == w.narp, "off ARP, on SEQ: the arpeggio's notes kept");
    frs_clear(1);
    rms(4800);
    frs_view(&v);
    check(v.narp == w.narp, "  CLEAR on the sequence leaves them");
    frs_param_set(P_SQ_TYPE, 0);
    rms(4800);
    frs_view(&v);
    check(v.narp == w.narp, "  off too");
    frs_param_set(P_SQ_TYPE, 1);
    rms(4800);
    frs_view(&v);
    check(v.narp == w.narp, "  ARP again: there");
    frs_clear(1);
    rms(4800);
    frs_view(&v);
    check(v.narp == 0, "  CLEAR on ARP lets them go");

    /* GLINT's keys latched, its sequence off: its arpeggio's list rings, held, the last four; a fifth retires the oldest
     * come in; ARP steps them, SEQ leaves them silent, the list the same; a track stopped waits, CLEAR and PANIC let go, a
     * scene keeps them; a finger on one lets it ring on */
    {
        static uint8_t st8[FRS_SCENE_BYTES];
        uint32_t oc, k;
        int low;
        frs_close();
        frs_init(48000.0f);
        frs_scene_new(sc);
        frs_scene_take(sc);
        frs_param_set(P_SQ_TYPE, 0);
        frs_param_set(P_GL_SUSTAIN, 127);
        passes(2);
        oc = 12u * frs_param_get(P_GL_OCTAVE);
        check(frs_latch(1, 72, FRS_CHORD_HARMONY) == 1 && frs_latch(1, 60, FRS_CHORD_HARMONY) == 1 && frs_latch(1, 64, FRS_CHORD_HARMONY) == 1 &&
              frs_latch(1, 67, FRS_CHORD_HARMONY) == 1, "GLINT's keys: four notes latched");
        passes(3);
        frs_view(&v);
        check(v.narp == 4u && rung() == 4, "  they ring, held");
        check(frs_latch(1, 69, FRS_CHORD_HARMONY) == 1, "  a fifth");
        passes(30);
        frs_view(&v);
        low = v.arp[v.narp - 1u] + (int)oc;
        check(v.narp == 4u && low == 69 && rung() == 4, "  the first come in (72, not the lowest) gone, four ring on");
        check(frs_latch(1, 69, FRS_CHORD_HARMONY) == 0, "  latched again: let go");
        passes(30);
        frs_view(&v);
        check(v.narp == 3u && rung() == 3, "  three ring");
        frs_param_set(P_SQ_TYPE, 2);
        passes(30);
        frs_view(&v);
        check(v.narp == 3u && rung() == 0, "  on SEQ: silent, kept");
        frs_param_set(P_SQ_TYPE, 1);
        passes(3);
        frs_view(&v);
        check(v.narp == 3u, "  on ARP: there");
        frs_param_set(P_SQ_TYPE, 0);
        passes(30);
        check(rung() == 3, "  KEYS again: they ring");
        frs_track_play(1, 0);
        passes(30);
        frs_view(&v);
        check(v.narp == 3u && rung() == 0 && !(v.playing >> 1 & 1u), "  the track stopped: they wait");
        frs_track_play(1, 1);
        passes(3);
        frs_view(&v);
        check(rung() == 3 && (v.playing >> 1 & 1u), "  started: they ring");
        k = v.arp[0] + oc;
        frs_hold(1, k, 1, FRS_CHORD_HARMONY);
        passes(2);
        frs_hold(1, k, 0, FRS_CHORD_HARMONY);
        passes(30);
        check(rung() >= 3, "  a finger on one, let go: it rings on");
        frs_scene_store(st8, sc);
        passes(1);
        frs_clear(1);
        passes(30);
        frs_view(&v);
        check(v.narp == 0u && rung() == 0, "  CLEAR lets them go");
        frs_scene_take(st8);
        passes(3);
        frs_view(&v);
        check(v.narp == 3u && rung() == 3, "  the scene stored: they ring again");
        frs_panic();
        passes(1);
        frs_view(&v);
        check(v.narp == 0u && rung() == 0, "  PANIC: gone");
    }

    /* PLAY after the transport stopped every track: what it kept, back (not the scene's notes over the player's) */
    frs_close();
    frs_init(48000.0f);
    frs_scene_factory(0, sc);
    frs_scene_take(sc);
    frs_param_set(P_SQ_TYPE, 0);
    frs_clear(1);
    frs_latch(0, 62, FRS_CHORD_HARMONY);
    rms(4800);
    frs_view(&w);
    frs_play_all(0);
    rms(4800);
    frs_view(&v);
    k = !bits(v.latched) && bits(v.kept) == bits(w.latched);
    check(frs_play() == 0 && k, "PLAY with every track stopped: nothing played as it was tapped");
    frs_view(&v);
    check(!memcmp(v.latched, w.latched, sizeof v.latched) && !bits(v.kept) && rms(4800) > 1e-6f,
          "  what the transport kept, back: the player's chords");

    /* PLAY tapped twice quick on the sequences alone: RELEASE ALL (frs_release), never RESTORE */
    frs_close();
    frs_init(48000.0f);
    check(frs_play() == 0, "PLAY from silence: nothing played as it was tapped");
    frs_param_set(P_SQ_TYPE, 2);         /* (GLINT SEQ, a note on its first step) */
    frs_param_set(P_SQ_STEP, 0);
    frs_param_set(P_SQ_NOTE, 72);
    rms(48000);
    frs_view(&w);
    k = (uint32_t)frs_play();
    frs_release();
    rms(48000);
    frs_view(&v);
    check(w.seq_run && k == 1u && !v.seq_run && !bits(v.latched) && !v.narp && v.bass < 0,
          "PLAY tapped twice quick (FRS_PLAY_TWICE_MS): the sequence stopped, everything let go, not started again");

    /* a 2.0 engine's state (version 1: its settings past its NSOUND 24 lower) and a 2.5 engine's (2: one lower): read
     * as 2.6's */
    {
        static const uint8_t V1[] = {'F', 'R', 's', 't', 1, 130, 1, 131, 5, 255, 1};
        static const uint8_t V2[] = {'F', 'R', 's', 't', 2, 153, 1, 154, 6, 181, 1, 255, 1};
        uint32_t eq = frs_param_get(P_EQ_PRESET), lo = frs_param_get(P_EQ_LOW), so = frs_param_get(P_CH_SOFT);
        frs_param_set(P_KEYS, 0);
        frs_param_set(P_CANAL, 1);
        n = frs_state_save(state);
        check(state[4] == 3u && frs_state_load(V1, sizeof V1) == 0 && frs_param_get(P_KEYS) == 1 &&
                  frs_param_get(P_CANAL) == 5 && frs_param_get(P_EQ_PRESET) == eq && frs_param_get(P_EQ_LOW) == lo,
              "the state's version 3; a 2.0 engine's (1): KEY MODE and DRONE CH back, MASTER EQ untouched");
        frs_param_set(P_KEYS, 0);
        frs_param_set(P_CANAL, 1);
        check(frs_state_load(V2, sizeof V2) == 0 && frs_param_get(P_KEYS) == 1 && frs_param_get(P_CANAL) == 6 &&
                  frs_param_get(P_CLOCK) == 1 && frs_param_get(P_CH_SOFT) == so,
              "a 2.5 engine's (2): KEY MODE, DRONE CH and CLOCK back, SOFT untouched");
        frs_param_set(P_KEYS, 0);
        frs_param_set(P_CLOCK, 0);
    }

    /* 2.6: CHOIR III's SOFT (by name, in a scene, on the sound), SHAPE's words, LOFI TAPE's drag (frs_live's 52) */
    frs_close();
    frs_init(48000.0f);
    {
        uint8_t st6[FRS_SCENE_BYTES];
        float live[FRS_LIVE], fx[FRS_LIVE_FX], e0, e1, most = 0.0f;
        const char *nm;
        uint32_t max, def, i;
        int bad = 0, same = 1;
        check(param("SOFT", 0) == P_CH_SOFT && P_CH_SOFT == NSOUND - 1 && NSOUND == 154 && NPARAM == 227 &&
                  P_KEYS == 154 && P_CLOCK == 182 && P_BQ_NLEN == 183 && P_BA_MODE == 184 && P_SQ_NLEN == 191 && P_BS_WEIGHT == 192 && P_BS_SPACE == 195 &&
                  P_GL_VIB_AUTO == 196 && P_DQ_ON == 197 && P_DQ_STEP == 205 && P_DQ_NOTES == 206 && P_DQ_UNDO == 208 &&
                  P_GA_LATCH == 209 && P_BA_LATCH == 210 && P_KY_DR_HARM == 211 && P_KY_BS_STACK == 222 &&
                  P_METRO == 223 && P_COUNT_IN == 224 && P_ARP_SNAP == 225 && P_SAVER == 226 && FRS_SCENE_BYTES == 586u &&
                  !frs_param_info(P_CH_SOFT, &nm, &max, &def) && max == 127u && def == 0u &&
                  frs_param_kind(P_CH_SOFT) == FRS_P_SCENE,
              "SOFT: the sound's last (153), 0 .. 127 from 0, kept in a scene; KEY MODE 154 .. CLOCK 182, LENGTH 183, the "
              "bass's MODE 184 .. GLINT's LENGTH 191, BASS TONE 192 .. 194, its SPACE 195, GLINT's AUTO VIB 196, the drone's "
              "sequence 197 .. 205, 3.0's after it: its NOTES 206 .. UNDO TAKE 208, ARP II's LATCH 209, 210, KEYS 211 .. "
              "222, METRONOME 223, COUNT-IN 224, ARP SNAP 225, SCREENSAVER 226; a scene 586 B");
        frs_param_text(P_ARP_SNAP, 3, t);
        check(!frs_param_info(P_ARP_SNAP, &nm, &max, &def) && max == 5u && def == 3u && !strcmp(t, "1/4") &&
                  frs_param_kind(P_ARP_SNAP) == FRS_P_STATE,
              "ARP SNAP: OFF, 1/16, 1/8, 1/4, 1/2, 1 BAR, 1/4 at the start, kept beside a scene");
        frs_param_text(P_CH_SHAPE, 0, t);
        same = !strcmp(t, "SAW");
        frs_param_text(P_CH_SHAPE, 64, t);
        same = same && !strcmp(t, "SQUARE");
        frs_param_text(P_CH_SHAPE, 127, t);
        check(same && !strcmp(t, "PULSE"), "SHAPE's words: SAW, SQUARE at the middle, PULSE at the end");
        frs_param_set(P_CHOEUR, 127);
        frs_param_set(P_PARTIELS, 0);
        frs_param_set(P_SOUFFLE, 0);
        frs_param_set(P_CH_SHAPE, 64);
        frs_param_set(P_CH_INTERVAL, 1);
        frs_latch(0, 48, FRS_CHORD_HARMONY);
        rms(48000);
        e0 = rms(24000);
        frs_param_set(P_CH_SOFT, 127);
        rms(24000);
        e1 = rms(24000);
        frs_scene_store(st6, 0);
        check(e0 > 1e-5f && e1 > 1e-5f && e1 == e1 && fabsf(e1 - e0) > 1e-3f * e0 && st6[16 + P_CH_SOFT] == 127u,
              "SOFT 127 on a square choir: another sound, not silent; stored in the scene");
        frs_param_set(P_CH_SOFT, 0);
        frs_scene_take(st6);
        check(frs_param_get(P_CH_SOFT) == 127u, "  taken again: SOFT 127");
        frs_param_set(P_TP_ON, 1);
        frs_param_set(P_TP_WOBBLE, 127);
        for (i = 0; i < 400u; i++) {     /* (some 8 s: the drags come by themselves) */
            rms(960);
            frs_live(live);
            frs_live_fx(fx);
            bad |= !(live[52] >= 0.0f && live[52] <= 1.0f) || fabsf(live[52] - 12.5f * fx[5]) > 1e-6f;
            most = live[52] > most ? live[52] : most;
        }
        frs_param_set(P_TP_ON, 0);
        rms(4800);
        frs_live(live);
        check(!bad && most > 0.0f && live[52] == 0.0f,
              "LOFI TAPE's drag (frs_live 52): 0 .. 1, the effects' drag (5) as 8 % is 1; none with the tape off");
    }

    /* 2.7: the bass's sequence on one grid: RATE the sequence's, a note's LENGTH its HOLDs; a 2.6 scene made over; PUMP's
     * duck in the effects' view */
    {
        uint8_t a[FRS_SCENE_BYTES], b[FRS_SCENE_BYTES];
        float fx[FRS_LIVE_FX], most = 0.0f;
        char t[FRS_TEXT], u[FRS_TEXT], w[FRS_TEXT];
        frs_view_t v;
        uint32_t i;
        int bad = 0;
        uint8_t u8[FRS_SCENE_BYTES];
        enum { NOTE = 261, ON = 293, STEPS = 294, TEMPO = 296, LEN = 297, BAMODE = 298 };   /* (the River's schema's
                                                                                             * offsets) */
        {
            const char *nm;
            check(!frs_param_info(P_BQ_LEN, &nm, 0, 0) && !strcmp(nm, "RATE") && param("LENGTH", 0) == P_BQ_NLEN &&
                      frs_param_kind(P_BQ_LEN) == FRS_P_SCENE && frs_param_kind(P_BQ_NLEN) == FRS_P_STEP,
                  "2.7: RATE, the sequence's, kept in a scene; LENGTH, the note on STEP's");
        }
        frs_param_text(P_BQ_LEN, 3, t);
        frs_param_text(P_BQ_NLEN, 0, u);
        frs_param_text(P_BQ_NLEN, 3, w);
        check(!strcmp(t, "1/2") && !strcmp(u, "-") && !strcmp(w, "3 STEPS"), "  their words");
        check(!frs_scene_factory(0, a) && a[STEPS] == 15u && a[LEN] == 2u && a[NOTE] == 66u && a[NOTE + 1] == 127u,
              "  a factory scene: sixteen steps of 1/4, 2 BARS a note and seven HOLDs");
        frs_scene_new(a);
        frs_scene_take(a);
        frs_param_set(P_BQ_STEPS, 7);
        frs_param_set(P_BQ_LEN, 1);
        frs_param_set(P_BQ_STEP, 2);
        frs_param_set(P_BQ_NOTE, 50);
        frs_param_set(P_BQ_NLEN, 3);
        frs_view(&v);
        check(v.bnote[2] == 50u && v.bnote[3] == 127u && v.bnote[4] == 127u && v.bnote[5] == 0u && v.bq_steps == 7u &&
                  v.bq_len == 1u && frs_param_get(P_BQ_NLEN) == 3u,
              "  LENGTH 3: two HOLDs after the note; the view has the grid");
        frs_param_set(P_BQ_NLEN, 32);
        frs_view(&v);
        check(v.bnote[7] == 127u && v.bnote[8] == 0u && frs_param_get(P_BQ_NLEN) == 6u, "  never past SEQ LENGTH");
        frs_param_set(P_BQ_NLEN, 1);
        frs_view(&v);
        check(v.bnote[3] == 0u && v.bnote[7] == 0u && frs_param_get(P_BQ_NLEN) == 1u, "  down: CUTs");
        frs_scene_store(b, a);
        check(b[LEN] == 1u && b[STEPS] == 7u && b[NOTE + 2] == 50u, "  stored with its grid");
        memset(b + NOTE, 0, 32);         /* 2.6's: a 1/4 and a 1/16, two steps */
        b[NOTE] = 50, b[NOTE + 1] = 53, b[NOTE + 16] = 2, b[NOTE + 17] = 0;
        b[STEPS] = 1, b[ON] = 1, b[TEMPO] = 60;
        memset(b + LEN, 0xAA, FRS_SCENE_BYTES - LEN);
        check(!frs_scene_over(b, 297), "  frs_scene_over: 297 bytes, 2.6's");
        check(b[NOTE] == 50u && b[NOTE + 1] == 127u && b[NOTE + 3] == 127u && b[NOTE + 4] == 53u && b[NOTE + 5] == 0u &&
                  b[STEPS] == 4u && b[LEN] == 0u && b[ON] == 2u && b[TEMPO] == 60u,
              "  made over, a 1/4 + a 1/16 on 1/16s, PLAYS' 1 now 2 (SEQ)");
        check(b[BAMODE] == 0u && b[BAMODE + 1] == 1u && b[BAMODE + 2] == 127u && b[BAMODE + 3] == 127u && b[BAMODE + 4] == 0u,
              "  no arpeggio on the bass, MODE .. GATE their defaults, whatever followed its bytes");
        frs_scene_store(b, a);           /* 2.7's first test builds': on the one grid, PLAYS 1, 298 bytes */
        memcpy(u8, b, sizeof u8);
        b[ON] = 1;
        memset(b + BAMODE, 0x55, FRS_SCENE_BYTES - BAMODE);
        check(!frs_scene_over(b, 298) && b[ON] == 2u && !memcmp(b + NOTE, u8 + NOTE, 32) && b[LEN] == u8[LEN] &&
                  b[BAMODE + 4] == 0u && b[BAMODE + 2] == 127u,
              "  298 bytes, FRSu: its steps as they are, PLAYS' 1 now 2, the arpeggio none");
        b[ON] = 0;
        memset(b + BAMODE, 0x55, FRS_SCENE_BYTES - BAMODE);
        check(!frs_scene_over(b, 298) && b[ON] == 0u, "  KEYS stays KEYS");
        memcpy(u8, b, sizeof u8);
        check(!frs_scene_over(b, FRS_SCENE_BYTES) && !memcmp(u8, b, sizeof u8), "  FRS_SCENE_BYTES: as it is");
        check(frs_scene_over(b, 296) == -1 && !memcmp(u8, b, sizeof u8), "  a size it does not know: -1, untouched");
        frs_scene_take(a);
        frs_param_set(P_PUMP_RATE, 5);
        frs_param_set(P_PUMP_DRONE, 127);
        for (i = 0; i < 200u; i++) {
            rms(480);
            frs_live_fx(fx);
            bad |= !(fx[36] >= 0.0f && fx[36] <= 1.0f);
            most = fx[36] > most ? fx[36] : most;
        }
        check(!bad && most > 0.2f, "frs_live_fx 36: PUMP's duck, 0 .. 1");
        frs_param_set(P_PUMP_RATE, 0);
        rms(48000);
        frs_live_fx(fx);
        check(fx[36] == 0.0f, "  none with RATE OFF");
    }

    /* 2.7: the bass as GLINT: PLAYS (0 KEYS, 1 ARP, 2 SEQ), its arpeggio's notes in the view, its own settings; THE KEYS,
     * the transport, a scene with both, REC's cycle */
    {
        uint8_t sc[FRS_SCENE_BYTES], k[FRS_SCENE_BYTES];
        char t[FRS_TEXT], u[FRS_TEXT];
        frs_view_t v;
        uint32_t i;
        int bad = 0, heard = 0;
        enum { BAMODE = 298, TONE = 302, NBARP = 305, BARP = 306 };
        frs_scene_new(sc);
        frs_scene_take(sc);
        frs_param_text(P_BQ_ON, 0, t);
        frs_param_text(P_SQ_TYPE, 1, u);
        check(!strcmp(t, "KEYS") && !strcmp(u, "ARP") && frs_param_kind(P_BA_MODE) == FRS_P_SCENE &&
                  frs_param_kind(P_BQ_GATE) == FRS_P_SCENE && frs_param_kind(P_BQ_ACC) == FRS_P_STEP &&
                  frs_param_kind(P_SQ_NLEN) == FRS_P_STEP && frs_param_kind(P_BQ_NEW) == FRS_P_ACTION &&
                  frs_param_kind(P_BQ_UNDO) == FRS_P_ACTION,
              "2.7: PLAYS' words, the new settings' kinds");
        frs_param_set(P_BQ_ON, 1);
        frs_param_set(P_BQ_LEN, 0);      /* (1/16) */
        frs_param_set(P_BA_RANGE, 0);
        frs_param_set(P_BS_RELEASE, 0);
        check(frs_latch(2, 38, FRS_CHORD_HARMONY) == 1 && frs_latch(2, 45, FRS_CHORD_HARMONY) == 1, "  two notes latched");
        frs_view(&v);
        check(v.nbarp == 2u && v.barp[0] == 50 && v.barp[1] == 57 && v.barp_latch == 1u && v.bass < 0 && (v.playing & 4u),
              "  in the view, as played (without OCTAVE), low to high; none of them sounding yet");
        for (i = 0; i < 300u; i++) {     /* (some 3 s) */
            rms(480);
            frs_view(&v);
            if (v.barp_note >= 0) {
                heard |= v.barp_note == 50 ? 1 : v.barp_note == 57 ? 2 : 4;
                bad |= v.bass != v.barp_note - 12;   /* (its OCTAVE on it: one under) */
            }
        }
        check(heard == 3 && !bad, "  its steps: each of its notes in turn, barp_note the one the bass sounds");
        check(frs_latch(2, 45, FRS_CHORD_HARMONY) == 0, "  latched again: let go");
        frs_view(&v);
        check(v.nbarp == 1u, "  one left");
        frs_track_play(2, 0);
        rms(4800);
        frs_view(&v);
        check(!(v.playing & 4u) && v.barp_paused == 1u && v.nbarp == 1u && v.bass < 0, "  the track stopped: it waits, its notes kept");
        frs_track_play(2, 1);
        rms(48000);
        frs_view(&v);
        check((v.playing & 4u) && v.barp_paused == 0u, "  played: on");
        frs_play_all(0);
        rms(4800);
        frs_view(&v);
        check(v.barp_paused == 1u && !(v.playing & 4u), "  frs_play_all(0): the same");
        check(frs_play() >= 0, "  frs_play: what the transport stopped");
        frs_view(&v);
        check(v.barp_paused == 0u && (v.playing & 4u) && v.nbarp == 1u, "  back as it was");
        frs_hold(2, 45, 1, FRS_CHORD_HARMONY);
        frs_view(&v);
        check(v.nbarp == 2u, "  a finger on a note: in the arpeggio while it is down");
        frs_hold(2, 45, 0, FRS_CHORD_HARMONY);
        frs_view(&v);
        check(v.nbarp == 1u, "  let go: out");
        frs_param_set(P_BQ_ON, 0);
        frs_clear(2);
        frs_view(&v);
        check(v.nbarp == 1u && v.bass < 0, "  CLEAR off ARP: its notes wait for it, as GLINT's");
        frs_param_set(P_BQ_ON, 1);
        frs_clear(2);
        frs_view(&v);
        check(v.nbarp == 0u, "  on ARP: gone");
        /* PLAYS turned under a finger: its up lets go what its down took, whatever PLAYS is by then */
        frs_param_set(P_BQ_ON, 0);
        frs_hold(2, 45, 1, FRS_CHORD_HARMONY);
        frs_param_set(P_BQ_ON, 1);
        frs_hold(2, 45, 0, FRS_CHORD_HARMONY);
        frs_param_set(P_BQ_ON, 0);
        rms(4800);
        frs_view(&v);
        check(v.bass < 0, "  a finger from KEYS, lifted on ARP: no note left behind on KEYS");
        frs_param_set(P_BQ_ON, 1);
        frs_hold(2, 45, 1, FRS_CHORD_HARMONY);
        frs_view(&v);
        i = v.nbarp;
        frs_param_set(P_BQ_ON, 0);
        frs_hold(2, 45, 0, FRS_CHORD_HARMONY);
        frs_view(&v);
        check(i == 1u && v.nbarp == 0u, "  a finger from ARP, lifted on KEYS: out of the arpeggio");
        frs_param_set(P_BQ_ON, 1);
        /* both layers on ARP, different notes, each its settings: a scene keeps them */
        frs_param_set(P_SQ_TYPE, 1);
        frs_latch(1, 84, FRS_CHORD_HARMONY);
        frs_latch(1, 96, FRS_CHORD_HARMONY);
        frs_latch(2, 40, FRS_CHORD_HARMONY);
        frs_param_set(P_BA_MODE, 2);
        frs_param_set(P_BA_RANGE, 2);
        frs_param_set(P_BA_CHANCE, 90);
        frs_param_set(P_BQ_GATE, 70);
        frs_param_set(P_ARP_MODE, 1);
        frs_scene_store(k, sc);
        check(k[293] == 1u && k[BAMODE] == 2u && k[BAMODE + 1] == 2u && k[BAMODE + 2] == 90u && k[BAMODE + 3] == 70u &&
                  k[NBARP] == 1u && k[BARP] == 52, "  stored: PLAYS, MODE, RANGE, CHANCE, GATE, the note");
        frs_scene_new(sc);
        frs_scene_take(sc);
        frs_view(&v);
        check(v.nbarp == 0u && v.narp == 0u && frs_param_get(P_BA_MODE) == 0u && frs_param_get(P_BA_CHANCE) == 127u &&
                  frs_param_get(P_BQ_GATE) == 127u, "  another scene: its own, the defaults");
        frs_scene_take(k);
        frs_view(&v);
        check(v.nbarp == 1u && v.barp[0] == 52 && v.narp == 2u && frs_param_get(P_BA_MODE) == 2u &&
                  frs_param_get(P_BA_RANGE) == 2u && frs_param_get(P_BA_CHANCE) == 90u && frs_param_get(P_BQ_GATE) == 70u &&
                  frs_param_get(P_ARP_MODE) == 1u, "  taken again: both as they were, each its own");
        frs_release_all();
        frs_view(&v);
        check(v.nbarp == 0u && v.narp == 0u, "  RELEASE ALL: both gone");
        frs_release_all();
        frs_view(&v);
        check(v.nbarp == 1u && v.narp == 2u, "  RESTORE: both back");
        /* REC, the layer the keys play: OFF, LIVE, STEP, OFF, turned to SEQ, the arpeggio's notes kept */
        frs_param_set(P_KB, 2);
        check(frs_record() == 2 && frs_param_get(P_BQ_REC) == 2u && frs_param_get(P_BQ_ON) == 2u, "  REC: LIVE, PLAYS SEQ");
        check(frs_record() == 2 && frs_param_get(P_BQ_REC) == 1u, "  again: STEP");
        check(frs_record() == 0 && frs_param_get(P_BQ_REC) == 0u, "  again: off");
        frs_view(&v);
        check(v.nbarp == 1u && v.seq_run == 0u, "  its arpeggio's notes kept, silent");
        frs_param_set(P_BQ_STEP, 0);
        frs_param_set(P_BQ_ACC, 1);
        frs_param_set(P_SQ_STEP, 0);
        frs_param_set(P_SQ_NOTE, 60);
        frs_param_set(P_SQ_NLEN, 3);
        frs_view(&v);
        check((v.bnote[0] & 128u) != 0u && v.seq[0] == 60u && v.seq[1] == 127u && v.seq[2] == 127u && v.seq[3] == 0u,
              "  ACCENT in the bass's step; GLINT's LENGTH, its TIEs");
    }

    frs_close();
    printf("api: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
