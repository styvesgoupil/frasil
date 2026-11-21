/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* MIDI OUT (card #192, engine/mout.c, FRS_MIDI_OUT 1: the iPad's engine): what goes out, and when.
 *   KEYS: a key's note on as played on CHANNEL, its pressure as PRESSURE says (CHANNEL the hardest key's, POLY each
 *   key's), its note off on the channel it went out on (CHANNEL changed while held); VELOCITY's curves.
 *   ARP: three keys held, MODE UP: their notes out in turn on RATE's steps, each let go on GATE's share of its step;
 *   SWING: every second step late; LEGATO: each held till the next is out; LATCH off: the keys let go, it stops.
 *   SEQ: four steps (a note, a rest, a note tied over the next): out on RATE's steps, the tie held on; GATE; played and
 *   stopped by frs_mout_play, with FRASIL's PLAY (frs_play_all).
 *   CLOCK OUT: 24 clocks a beat, Start and Stop with its sequence.
 *   frs_release_all, frs_cut, frs_panic: every note it sent ended; nothing sounds here (the output silent throughout).
 *   Card #219 (what the iPad needs of the FM-1's 3.4): its KEYS (card #194: HARMONY, NOTES, a chord out on a key, a note
 *   two keys have on once and off with the last, POLY's pressure each note's, its ARP the chord's notes, its SEQ one);
 *   CLOCK OUT's transport after a skip that went past a Stop, a Start, both; NOTES OUT (card #201: GLINT's, SPARKLE's and
 *   the bass's own notes on 14, 15 and 16, each let go, the sound bit for bit with it or without, none struck in a
 *   skip, all let go at RELEASE ALL and off); the LEAD's chord on POLY from frs_key_notes, a key each.
 *   engine/tests/run.sh, or: mout */
#include "../frasil_engine.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if !FRS_MIDI_OUT
#error "MIDI OUT's test: built with -DFRS_MIDI_OUT=1 (engine/tests/run.sh)"
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

static float L[256], R[256], peak;
static frs_mout_ev_t ev[4096];
static uint32_t nev;
static void run_ms(uint32_t ms)          /* the engine on that many milliseconds, what it sent kept in ev */
{
    uint32_t n = (uint32_t)(ms * 48u), i;
    while (n) {
        uint32_t k = n < 256u ? n : 256u;
        frs_render(L, R, k);
        for (i = 0; i < k; i++)
            peak = fmaxf(peak, fmaxf(fabsf(L[i]), fabsf(R[i])));
        nev += frs_mout_read(ev + nev, 4096u - nev);
        n -= k;
    }
}
static uint32_t count(uint8_t status, int note)   /* (note -1: any) */
{
    uint32_t i, c = 0;
    for (i = 0; i < nev; i++)
        c += ev[i].status == status && (note < 0 || ev[i].d1 == note);
    return c;
}
static int find(uint32_t from, uint8_t hi, uint32_t *at)   /* the next of its kind (status & 0xF0) from `from` */
{
    uint32_t i;
    for (i = from; i < nev; i++)
        if ((ev[i].status & 0xF0u) == hi) {
            *at = i;
            return 1;
        }
    return 0;
}
static void fresh(void)
{
    uint8_t sc[FRS_SCENE_BYTES];
    frs_close();
    if (frs_init(48000.0f))
        exit(2);
    frs_scene_new(sc);
    frs_scene_take(sc);
    frs_play_all(0);
    run_ms(50);
    nev = 0;
}
static int index_of(uint8_t status, uint32_t from)   /* the first of that status from `from`, -1 none */
{
    uint32_t i;
    for (i = from; i < nev; i++)
        if (ev[i].status == status)
            return (int)i;
    return -1;
}

/* a skip's tick hook: MIDI OUT's sequence played (1) or stopped (0) at each of its milliseconds */
static uint32_t hook_ms[2], nhook, skip_end;
static int hook_on[2];
static void skip_hook(void *user)
{
    uint32_t i;
    (void)user;
    for (i = 0; i < nhook; i++)
        if (frs_ms() == hook_ms[i])
            frs_mout_play(hook_on[i]);
}
/* its sequence on CLOCK OUT, playing or not, then a skip of 10 s with what the hook does in it: the messages after it */
static void skip_with(int playing, uint32_t n, uint32_t at0, int on0, uint32_t at1, int on1)
{
    uint8_t s[32] = {60, 62, 64, 65};
    uint32_t t0;
    fresh();
    frs_mout_steps(s);
    frs_mout_set(FRS_MO_TYPE, 2);
    frs_mout_set(FRS_MO_LEN, 3);
    frs_mout_set(FRS_MO_CLOCK, 1);
    frs_mout_play(playing);
    run_ms(500);
    nev = 0;
    t0 = frs_ms();
    hook_ms[0] = t0 + at0;
    hook_on[0] = on0;
    hook_ms[1] = t0 + at1;
    hook_on[1] = on1;
    nhook = n;
    frs_tick_hook(skip_hook, 0);
    frs_skip_to(t0 + 10000u);
    skip_end = frs_ms();
    frs_tick_hook(0, 0);
    run_ms(30);
}

/* NOTES OUT's messages (channels 14 .. 16): each channel's notes out, its ons and offs, an on over its own note, an off
 * of none */
static struct {
    uint32_t open[3][4], on[3], off[3], twice, stray, n;
} nt;
static void nt_take(const frs_mout_ev_t *e, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint32_t s = e[i].status, c = s & 15u, k = e[i].d1, *w, b;
        if (c < 13u || ((s & 0xF0u) != 0x90u && (s & 0xF0u) != 0x80u))
            continue;
        w = &nt.open[c - 13u][k >> 5];
        b = 1u << (k & 31u);
        nt.n++;
        if ((s & 0xF0u) == 0x90u) {
            nt.twice += (*w & b) != 0u;
            *w |= b;
            nt.on[c - 13u]++;
        } else {
            nt.stray += (*w & b) == 0u;
            *w &= ~b;
            nt.off[c - 13u]++;
        }
    }
}
static int nt_open(void)                 /* NOTES OUT's notes out now */
{
    uint32_t c, w, n = 0;
    for (c = 0; c < 3u; c++)
        for (w = 0; w < 4u; w++)
            n += (uint32_t)__builtin_popcount(nt.open[c][w]);
    return (int)n;
}
static uint64_t nt_play(uint32_t ms, uint64_t h)   /* rendered on, NOTES OUT's messages taken: the sound's hash on h */
{
    static int32_t lr[2 * 480];
    uint32_t i, n;
    frs_mout_ev_t e[256];
    for (i = 0; i < ms / 10u; i++) {
        uint32_t j;
        frs_render_i24(lr, 480);
        for (j = 0; j < 2u * 480u; j++) {
            h ^= (uint32_t)lr[j];
            h *= 0x100000001B3u;
        }
        while ((n = frs_mout_read(e, 256)) > 0)
            nt_take(e, n);
    }
    return h;
}

int main(void)
{
    uint32_t i, a = 0, b = 0;
    char t[128];
    /* ------------------------------------------------------------ KEYS */
    fresh();
    frs_mout_set(FRS_MO_CH, 2);
    frs_mout_key(5, 60, 90, 1);
    run_ms(5);
    check(nev == 1 && ev[0].status == 0x92 && ev[0].d1 == 60 && ev[0].d2 == 90, "KEYS: note on, channel 3, as played");
    frs_mout_pressure(5, 70);
    run_ms(5);
    check(count(0xD2, -1) == 1 && ev[nev - 1].d1 == 70, "KEYS: CHANNEL pressure");
    frs_mout_pressure(5, 71);
    run_ms(5);
    check(count(0xD2, -1) == 1, "KEYS: a pressure one off not sent again");
    frs_mout_set(FRS_MO_CH, 9);
    frs_mout_key(5, 60, 0, 0);
    run_ms(5);
    check(count(0x82, 60) == 1 && count(0x89, -1) == 0, "KEYS: its note off on the channel it went out on");
    frs_mout_set(FRS_MO_PRS, 2);
    frs_mout_key(6, 64, 100, 1);
    frs_mout_pressure(6, 40);
    frs_mout_key(6, 64, 0, 0);
    run_ms(5);
    check(count(0xA9, 64) >= 1 && count(0x89, 64) == 1, "KEYS: POLY pressure, then its note off");
    for (i = 0; i < 4u; i++) {
        static const uint8_t want[4] = {90, 64, 32, 100};   /* SOFT, NORMAL, HARD, FIXED of 64 */
        frs_mout_set(FRS_MO_VEL, i);
        nev = 0;
        frs_mout_key(7, 50, 64, 1);
        frs_mout_key(7, 50, 0, 0);
        run_ms(3);
        snprintf(t, sizeof t, "VELOCITY %u: 64 out as %u (%u)", i, want[i], nev ? ev[0].d2 : 0);
        check(nev == 2 && ev[0].d2 == want[i], t);
    }
    frs_mout_text(FRS_MO_AGATE, 101, t);
    check(!strcmp(t, "LEGATO"), "words: GATE past 100 % LEGATO");
    frs_mout_text(FRS_MO_SWING, 8, t);
    check(!strcmp(t, "58 %"), "words: SWING 58 %");
    frs_mout_text(FRS_MO_RATE, 5, t);
    check(t[0] == '1' && t[1] == '/', "words: RATE GLINT's");
    /* ------------------------------------------------------------- ARP */
    fresh();
    frs_mout_set(FRS_MO_TYPE, 1);        /* ARP, MODE UP over one octave, RATE 1/8 (12 clocks), GATE 50 %, at TEMPO */
    frs_mout_set(FRS_MO_RANGE, 0);
    frs_mout_key(1, 60, 100, 1);
    frs_mout_key(2, 64, 100, 1);
    frs_mout_key(3, 67, 100, 1);
    run_ms(3000);
    {
        uint32_t on[64], n = 0, ok = 1, gate = 1;
        float step = 12.0f * 2500.0f / (float)(frs_param_get(P_TEMPO) + 30u);
        for (i = 0; i < nev && n < 64u; i++)
            if ((ev[i].status & 0xF0u) == 0x90u)
                on[n++] = i;
        for (i = 0; i + 1u < n; i++) {
            float dt = (float)(ev[on[i + 1u]].ms - ev[on[i]].ms);
            ok &= fabsf(dt - step) <= 2.0f;
            ok &= ev[on[i + 1u]].d1 == (ev[on[i]].d1 == 60 ? 64 : ev[on[i]].d1 == 64 ? 67 : 60);
            if (find(on[i], 0x80u, &a) && ev[a].d1 == ev[on[i]].d1)
                gate &= fabsf((float)(ev[a].ms - ev[on[i]].ms) - step * 0.5f) <= 2.0f;
            else
                gate = 0;
        }
        snprintf(t, sizeof t, "ARP: %u notes up in turn every %.0f ms", n, step);
        check(n >= 8u && ok, t);
        check(gate, "ARP: each let go at GATE's 50 % of its step");
    }
    frs_mout_set(FRS_MO_SWING, 16);      /* 66 %: a pair's second a third of a step late */
    nev = 0;
    run_ms(3000);
    {
        uint32_t on[64], n = 0, ok = 0;
        for (i = 0; i < nev && n < 64u; i++)
            if ((ev[i].status & 0xF0u) == 0x90u)
                on[n++] = i;
        for (i = 0; i + 2u < n; i++)
            ok += abs((int)(ev[on[i + 1u]].ms - ev[on[i]].ms) - (int)(ev[on[i + 2u]].ms - ev[on[i + 1u]].ms)) > 40;
        check(n >= 8u && ok >= n - 3u, "ARP: SWING, the steps long and short in turn");
    }
    frs_mout_set(FRS_MO_SWING, 0);
    frs_mout_set(FRS_MO_AGATE, 101);     /* LEGATO */
    nev = 0;
    run_ms(2000);
    {
        uint32_t on = 0, over = 1, last = 0;
        for (i = 0; i < nev; i++)
            if ((ev[i].status & 0xF0u) == 0x90u) {   /* (the one before let go as this one goes out, not before) */
                if (on++)
                    over &= i + 1u < nev && ev[i + 1u].status == 0x80u && ev[i + 1u].d1 == last && ev[i + 1u].ms == ev[i].ms;
                last = ev[i].d1;
            }
        check(on >= 6u && over, "ARP: LEGATO, each held till the next is out");
    }
    frs_mout_set(FRS_MO_LATCH, 0);       /* LATCH off: its keys let go, it stops */
    frs_mout_key(1, 60, 0, 0);
    frs_mout_key(2, 64, 0, 0);
    frs_mout_key(3, 67, 0, 0);
    run_ms(1000);
    nev = 0;
    run_ms(1000);
    check(count(0x90, -1) == 0, "ARP: LATCH off, the keys let go: nothing more out");
    /* ------------------------------------------------------------- SEQ */
    fresh();
    {
        uint8_t s[32];
        float step;
        memset(s, 0, sizeof s);
        s[0] = 60;
        s[2] = 67;
        s[3] = 127;                      /* (a tie: 67 held over step 4) */
        frs_mout_steps(s);
        frs_mout_set(FRS_MO_TYPE, 2);
        frs_mout_set(FRS_MO_LEN, 3);     /* four steps */
        frs_mout_set(FRS_MO_GATE, 127);
        frs_mout_play(1);
        run_ms(4000);
        step = 12.0f * 2500.0f / (float)(frs_param_get(P_TEMPO) + 30u);
        check(find(0, 0x90u, &a) && ev[a].d1 == 60 && find(a + 1u, 0x90u, &b) && ev[b].d1 == 67 &&
                  fabsf((float)(ev[b].ms - ev[a].ms) - 2.0f * step) <= 2.0f,
              "SEQ: step 1 then step 3, two steps apart");
        check(find(b, 0x80u, &a) && ev[a].d1 == 67 && fabsf((float)(ev[a].ms - ev[b].ms) - 2.0f * step) <= 2.0f,
              "SEQ: the tie holds 67 two steps");
        check(frs_mout_playing(), "SEQ: playing");
        frs_play_all(0);
        run_ms(10);
        nev = 0;
        run_ms(2000);
        check(count(0x90, -1) == 0 && !frs_mout_playing(), "SEQ: stopped with FRASIL's PLAY");
        frs_play_all(1);
        run_ms(1000);
        check(count(0x90, -1) >= 1 && frs_mout_playing(), "SEQ: played again with FRASIL's PLAY");
        /* ------------------------------------------------------- CLOCK OUT */
        frs_mout_set(FRS_MO_CLOCK, 1);
        nev = 0;
        run_ms(2000);
        {
            float beat = 2500.0f * 24.0f / (float)(frs_param_get(P_TEMPO) + 30u), clocks = (float)count(0xF8, -1);
            snprintf(t, sizeof t, "CLOCK OUT: %.0f clocks in 2 s (24 a beat of %.0f ms)", clocks, beat);
            check(fabsf(clocks - 2000.0f / beat * 24.0f) <= 2.0f && count(0xFA, -1) == 1, t);
        }
        frs_mout_play(0);
        run_ms(20);
        check(count(0xFC, -1) == 1, "CLOCK OUT: Stop with its sequence");
    }
    /* ------------------------------------------------ RELEASE ALL, CUT, panic */
    fresh();
    frs_mout_key(1, 50, 100, 1);
    frs_mout_key(2, 55, 100, 1);
    run_ms(5);
    frs_release_all();
    run_ms(5);
    check(count(0x80, 50) == 1 && count(0x80, 55) == 1, "RELEASE ALL: every note it sent ended");
    frs_mout_key(1, 52, 100, 1);
    run_ms(5);
    frs_cut();
    run_ms(5);
    check(count(0x80, 52) == 1, "CUT: its note ended");
    frs_mout_key(3, 53, 100, 1);
    run_ms(5);
    frs_panic();
    run_ms(5);
    check(count(0x80, 53) == 1, "panic: its note ended");
    frs_mout_key(3, 54, 100, 1);
    run_ms(5);
    check(count(0x90, 54) == 1, "after a panic: its key plays again");
    /* ------------------------------------------- what the review asked (card #192) */
    fresh();
    frs_mout_key(4, 61, 100, 1);
    run_ms(5);
    frs_close();                         /* (an engine opened afresh, as a stream's: its panic's note-off kept, out at once) */
    if (frs_init(48000.0f))
        return 2;
    nev = 0;
    run_ms(5);
    check(count(0x80, 61) == 1 && ev[0].ms == 0, "an engine opened again: the note-off its panic sent still goes out, at once");
    fresh();
    frs_mout_set(FRS_MO_PRS, 1);
    frs_mout_key(1, 62, 100, 1);
    frs_mout_pressure(1, 90);
    run_ms(5);
    frs_mout_set(FRS_MO_PRS, 2);         /* (CHANNEL to POLY: the channel's pressure back to 0) */
    run_ms(5);
    check(nev >= 3 && ev[nev - 1].status == 0xD0 && ev[nev - 1].d1 == 0, "PRESSURE changed: CHANNEL's pressure let go");
    frs_mout_key(2, 62, 100, 1);         /* (the same note on two keys: off with the last) */
    frs_mout_key(1, 62, 0, 0);
    run_ms(5);
    check(count(0x80, 62) == 0, "the same note on two keys: the first up lets nothing go");
    frs_mout_key(2, 62, 0, 0);
    run_ms(5);
    check(count(0x80, 62) == 1, "... the last up lets it go");
    fresh();
    {
        uint8_t s[32] = {60, 62, 64, 65};
        frs_mout_steps(s);
        frs_mout_set(FRS_MO_TYPE, 2);
        frs_mout_set(FRS_MO_LEN, 3);
        frs_mout_set(FRS_MO_CLOCK, 1);
        frs_mout_play(1);
        run_ms(500);
        nev = 0;
        frs_skip_to(frs_ms() + 10000u);  /* (a SEEK's skip: no burst of clocks and notes after it) */
        run_ms(50);
        snprintf(t, sizeof t, "a skip of 10 s: %u notes and %u clocks out after it (its note-offs let through)", count(0x90, -1), count(0xF8, -1));
        check(count(0x90, -1) <= 2u && count(0xF8, -1) < 6u, t);
        frs_mout_set(FRS_MO_CLOCK, 0);
        run_ms(5);
        check(ev[nev - 1].status == 0xFC, "CLOCK OUT turned off while running: its Stop");
    }
    /* --------------------------------------- CLOCK OUT's transport after a skip (card #219) */
    {
        int fa, fc;
        skip_with(1, 1, 3000, 0, 0, 0);      /* (playing, stopped in the skip) */
        fa = index_of(0xFA, 0);
        fc = index_of(0xFC, 0);
        snprintf(t, sizeof t, "a skip past CLOCK OUT's Stop: its Stop after it, at the new place (%d at %u, the skip's end %u)",
                 fc, fc >= 0 ? ev[fc].ms : 0u, skip_end);
        check(fc >= 0 && fa < 0 && ev[fc].ms == skip_end && count(0xFC, -1) == 1u, t);
        skip_with(0, 1, 3000, 1, 0, 0);      /* (stopped, played in the skip) */
        fa = index_of(0xFA, 0);
        fc = index_of(0xFC, 0);
        snprintf(t, sizeof t, "a skip past its Start: its Start after it, at the new place, the clocks after it (%d at %u, the skip's end %u)",
                 fa, fa >= 0 ? ev[fa].ms : 0u, skip_end);
        check(fa >= 0 && fc < 0 && ev[fa].ms == skip_end && count(0xFA, -1) == 1u && index_of(0xF8, 0) > fa, t);
        skip_with(1, 2, 2000, 0, 4000, 1);   /* (playing, stopped then played again in the skip) */
        fa = index_of(0xFA, 0);
        fc = index_of(0xFC, 0);
        check(fc >= 0 && fa > fc && count(0xFA, -1) == 1u && count(0xFC, -1) == 1u,
              "a skip past a Stop and a Start: Stop, then Start");
        skip_with(1, 0, 0, 0, 0, 0);         /* (playing through) */
        check(count(0xFA, -1) == 0u && count(0xFC, -1) == 0u, "a skip, playing through: no Start, no Stop");
        skip_with(0, 2, 2000, 1, 4000, 0);   /* (stopped, played and stopped in the skip) */
        check(count(0xFA, -1) == 0u && count(0xFC, -1) == 0u, "a skip, stopped at both ends: no Start, no Stop");
    }
    /* ------------------------------------------------- its KEYS (card #194) */
    fresh();
    {
        uint8_t c[FRS_CHORD_MAX], s[FRS_CHORD_MAX];
        uint32_t nc, ok, j;
        int n;
        frs_mout_view_t mv;
        frs_mout_text(FRS_MO_HARM, 1, t);
        frs_mout_text(FRS_MO_NOTES, 1, t + 16);
        check(!frs_mout_get(FRS_MO_HARM) && !frs_mout_get(FRS_MO_NOTES) && frs_mout_max(FRS_MO_NOTES) == 1u &&
                  !strcmp(t, "ON") && !strcmp(t + 16, "CHORD"), "KEYS: HARMONY OFF, SINGLE NOTE at the start; their words");
        frs_mout_set(FRS_MO_HARM, 1);
        frs_mout_set(FRS_MO_NOTES, 1);
        frs_mout_set(FRS_MO_CH, 4);
        nc = frs_key_notes(60, 1, 1, c);
        frs_mout_key(5, 60, 90, 1);
        run_ms(5);
        for (ok = nc >= 3u, j = 0; j < nc; j++)
            ok &= count(0x94, c[j]) == 1u;
        snprintf(t, sizeof t, "KEYS CHORD: HARMONY's chord on the key, its %u notes out on CHANNEL", nc);
        check(ok && count(0x94, -1) == nc && ev[0].d1 == c[0], t);
        frs_mout_key(6, 60, 90, 1);          /* (the same chord on a second key: out already) */
        run_ms(5);
        check(count(0x94, -1) == nc, "  the same chord on a second key: nothing out again");
        frs_mout_key(5, 60, 0, 0);
        run_ms(5);
        check(count(0x84, -1) == 0u, "  the first up: nothing let go, the second holds them");
        frs_mout_key(6, 60, 0, 0);
        run_ms(5);
        for (ok = 1, j = 0; j < nc; j++)
            ok &= count(0x84, c[j]) == 1u;
        check(ok && count(0x84, -1) == nc, "  the last up: each let go, once");
        frs_mout_set(FRS_MO_PRS, 2);
        nev = 0;
        frs_mout_key(7, 60, 90, 1);
        frs_mout_pressure(7, 50);
        frs_mout_key(7, 60, 0, 0);
        run_ms(5);
        check(count(0xA4, -1) == nc && count(0x84, -1) == nc, "  POLY pressure: each note of its chord's");
        frs_mout_set(FRS_MO_NOTES, 0);
        for (n = 50; n < 72 && (frs_key_notes(n, 1, 0, s), s[0] == n); n++)
            ;
        nev = 0;
        frs_mout_key(8, n, 90, 1);
        frs_mout_key(8, n, 0, 0);
        run_ms(5);
        check(n < 72 && nev == 2u && ev[0].d1 == s[0] && ev[1].d1 == s[0], "SINGLE NOTE: the scale's note at or below it");
        frs_mout_set(FRS_MO_NOTES, 1);
        frs_mout_set(FRS_MO_TYPE, 2);
        nev = 0;
        frs_mout_key(9, 60, 90, 1);
        frs_mout_key(9, 60, 0, 0);
        run_ms(5);
        check(count(0x94, -1) == 1u && ev[0].d1 == c[0], "  on its SEQ: one note, the chord's root");
        frs_mout_set(FRS_MO_TYPE, 1);
        frs_mout_set(FRS_MO_LATCH, 0);
        frs_mout_key(10, 60, 100, 1);
        run_ms(5);
        frs_mout_view(&mv);
        for (ok = mv.narp == nc, j = 0; j < nc; j++)
            ok &= memchr(mv.arp, c[j], mv.narp) != 0;
        check(ok, "  on ARP: the chord's notes into its arpeggio");
        frs_mout_key(11, 64, 100, 1);        /* (another chord sharing notes with it: they stay) */
        frs_mout_key(10, 60, 0, 0);
        run_ms(5);
        frs_mout_view(&mv);
        nc = frs_key_notes(64, 1, 1, s);
        for (ok = mv.narp == nc, j = 0; j < nc; j++)
            ok &= memchr(mv.arp, s[j], mv.narp) != 0;
        check(ok, "  LATCH off, the first up: the other key's chord left whole");
        frs_mout_key(11, 64, 0, 0);
        run_ms(5);
        frs_mout_view(&mv);
        check(mv.narp == 0u, "  the last up: none left");
        frs_mout_set(FRS_MO_TYPE, 0);
        nc = frs_key_notes(60, 1, 1, c);
        nev = 0;
        frs_mout_key(12, 60, 90, 1);
        run_ms(5);
        frs_release_all();
        run_ms(5);
        check(count(0x84, -1) == nc, "  RELEASE ALL: each of its notes ended");
    }
    check(peak == 0.0f, "nothing sounds here: the output silent throughout");

    /* ---------------------------------------------------- NOTES OUT (card #201) */
    {
        uint8_t sc[FRS_SCENE_BYTES];
        uint64_t h[2];
        uint32_t pass, ons = 0;
        for (pass = 0; pass < 2u; pass++) {  /* (RESILIENCE: GLINT's arpeggio, the bass's sequence; SPARKLE up) */
            frs_close();
            frs_init(48000.0f);
            frs_scene_factory(2, sc);
            frs_scene_take(sc);
            frs_param_set(P_GX_SPARK, 100);
            frs_mout_notes((int)pass);
            memset(&nt, 0, sizeof nt);
            h[pass] = nt_play(12000, 0xCBF29CE484222325u);
            ons = nt.on[0] + nt.on[1] + nt.on[2];
            if (!pass)
                check(nt.n == 0u, "NOTES OUT off: nothing out on 14 .. 16");
        }
        snprintf(t, sizeof t, "NOTES OUT: GLINT's %u on 14, SPARKLE's %u on 15, the bass's %u on 16", nt.on[0], nt.on[1],
                 nt.on[2]);
        check(nt.on[0] >= 10u && nt.on[1] >= 2u && nt.on[2] >= 5u && !nt.twice && !nt.stray && nt_open() <= 3, t);
        check(h[0] == h[1], "  the sound bit for bit, with it or without");
        {
            frs_mout_view_t mv;
            frs_mout_view(&mv);
            check(!mv.out[0] && !mv.out[1] && !mv.out[2] && !mv.out[3], "  none on MIDI OUT's strip");
        }
        frs_release_all();
        nt_play(20, 0);
        check(nt_open() == 0 && !nt.stray, "  RELEASE ALL: every one let go");
        frs_release_all();                   /* (RESTORE) */
        nt_play(3000, 0);
        check(nt_open() > 0 || nt.on[0] + nt.on[1] + nt.on[2] > ons, "  RESTORE: out again");
        nt_play(500, 0);
        frs_mout_notes(0);
        nt_play(20, 0);
        check(nt_open() == 0 && !nt.stray, "  turned off: every one let go");
        frs_mout_notes(1);
        nt_play(2000, 0);
        ons = nt.on[0] + nt.on[1] + nt.on[2];
        {
            uint32_t t0 = frs_ms(), i, inside = 0, n;
            frs_mout_ev_t e[512];
            frs_skip_to(t0 + 8000u);
            n = frs_mout_read(e, 512);
            for (i = 0; i < n; i++)
                inside += (e[i].status & 0xF0u) == 0x90u && (e[i].status & 15u) >= 13u;
            nt_take(e, n);
            nt_play(2000, 0);
            check(!inside && !nt.twice && !nt.stray && nt.on[0] + nt.on[1] + nt.on[2] > ons,
                  "  a skip of 8 s: none struck in it, what was out let go, out again after it");
        }
        frs_panic();
        nt_play(10, 0);
        check(nt_open() == 0, "  a panic: every one let go");
    }

    /* the LEAD's chords (card #194, the iPad's way): frs_key_notes' notes, a key each, on POLY; MONO one note */
    {
        uint8_t c[FRS_CHORD_MAX];
        uint32_t nc, j, voices = 0;
        frs_view_t v;
        fresh();
        frs_param_set(P_LD_MODE, 1);
        frs_param_set(P_KB, 3);
        nc = frs_key_notes(60, 1, 1, c);
        for (j = 0; j < nc; j++)
            frs_key_down(20u + j, c[j], 100, 1);
        run_ms(100);
        frs_view(&v);
        for (j = 0; j < FRS_LEAD_NV; j++)
            voices += v.lead_note[j] >= 0 && v.lead_level[j] > 0.0f;
        snprintf(t, sizeof t, "the LEAD on POLY: its chord's %u notes, a key each, %u voices", nc, voices);
        check(nc >= 3u && voices == nc && v.lead_held == nc, t);
        for (j = 0; j < nc; j++)
            frs_key_up(20u + j);
        run_ms(100);
        frs_view(&v);
        check(!v.lead_held, "  each let go with its key");
    }
    printf("mout: %d ok, %d FAIL\n", oks, fails);
    return fails != 0;
}
