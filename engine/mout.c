/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* MIDI OUT for a shell with no app of its own (card #192: the iPad; the FM-1's and the SMK-37's are app/app.c's mout_*
 * and app/mseq.c, which this follows): a fifth track whose notes go out over MIDI, never sounding here, FRASIL playing
 * on. Its keys go out as played (KEYS), or into its arpeggio (the engine's arpeggiator, marp: GLINT's MODE, RATE,
 * RANGE, CHANCE, LATCH and ARP SNAP; its notes out on their time, GATE, SWING and VELOCITY as the FM-1's MIDI ARP II and
 * III), or play its sequence (SC_SEQ steps in GLINT's bytes: SEQ LENGTH, RATE, GATE, CHANCE, TRANSPOSE along the
 * scale), on FRASIL's grid. CLOCK OUT: the grid's clocks, 24 a beat, Start and Stop with its sequence. Every message goes
 * into a ring stamped with the engine's millisecond; the shell reads it after each render (frs_mout_read) and sends it
 * on its time. Its settings are the shell's to keep (none in a scene: frs_mout_set, frs_mout_get), as are its steps
 * (frs_mout_steps). Its KEYS (card #194): HARMONY and NOTES, a key's chord out (app.c mout_key_down's). NOTES OUT
 * (card #201, app.c nout_*'s): the brain's own notes out on channels 14 .. 16 (no_*, core.c's GEN_ hooks). Part of the
 * unity build: frasil_engine.c includes it last, FRS_MIDI_OUT 1; mo_pass runs in each millisecond's tick, after the
 * grid */

#define MO_RING 512u                     /* messages waiting for the shell (a power of two) */
#define MO_LEGATO 101u                   /* GATE past 100 %: each note held till the next is out */
enum { MO_VEL_SOFT, MO_VEL_NORMAL, MO_VEL_HARD, MO_VEL_FIXED };
enum { MO_PRS_OFF, MO_PRS_CHANNEL, MO_PRS_POLY };
/* each setting: its range, its default (GLINT's where it borrows GLINT's) */
static const struct {
    uint8_t min, max, def;
} MO_AS[FRS_MO_N] = {
    [FRS_MO_TYPE] = {0, 2, 0},       [FRS_MO_MODE] = {0, 4, 0},     [FRS_MO_RATE] = {0, 9, 5},
    [FRS_MO_RANGE] = {0, 2, 1},      [FRS_MO_CHANCE] = {0, 127, 127}, [FRS_MO_LATCH] = {0, 1, 1},
    [FRS_MO_LEN] = {0, 31, 15},      [FRS_MO_GATE] = {0, 127, 64},  [FRS_MO_TRANS] = {0, 28, 14},
    [FRS_MO_AGATE] = {5, MO_LEGATO, 50}, [FRS_MO_AVEL] = {0, 127, 0}, [FRS_MO_SWING] = {0, 25, 0},
    [FRS_MO_CH] = {0, 15, 0},        [FRS_MO_VEL] = {0, 3, MO_VEL_NORMAL}, [FRS_MO_PRS] = {0, 2, MO_PRS_CHANNEL},
    [FRS_MO_CLOCK] = {0, 1, 0},      [FRS_MO_HARM] = {0, 1, 0},     [FRS_MO_NOTES] = {0, 1, 0},
};
/* the settings it shares with GLINT's: that setting, for its words (KEYS' HARMONY and NOTES: GLINT's KEYS', the FM-1's
 * MIDI KEYS page's; OFF and SINGLE NOTE at the start, as there) */
static const uint16_t MO_WORDS[FRS_MO_N] = {
    [FRS_MO_TYPE] = P_SQ_TYPE, [FRS_MO_MODE] = P_ARP_MODE, [FRS_MO_RATE] = P_ARP_RATE, [FRS_MO_RANGE] = P_ARP_RANGE,
    [FRS_MO_CHANCE] = P_ARP_CHANCE, [FRS_MO_LATCH] = P_GA_LATCH, [FRS_MO_LEN] = P_SQ_LEN, [FRS_MO_GATE] = P_SQ_GATE,
    [FRS_MO_TRANS] = P_SQ_TRANS, [FRS_MO_AGATE] = NPARAM, [FRS_MO_AVEL] = NPARAM, [FRS_MO_SWING] = NPARAM,
    [FRS_MO_CH] = NPARAM, [FRS_MO_VEL] = NPARAM, [FRS_MO_PRS] = NPARAM, [FRS_MO_CLOCK] = NPARAM,
    [FRS_MO_HARM] = P_KY_GL_HARM, [FRS_MO_NOTES] = P_KY_GL_NOTES,
};
typedef struct {
    uint32_t ms;
    uint8_t s, d1, d2;
} mo_ev;                                 /* (a message out, its engine's millisecond) */
static uint8_t mo_v[FRS_MO_N];
static uint8_t mo_seq[SC_SEQ];
static struct {
    uint32_t own;                        /* bit k: key k is MIDI's */
    int8_t note[FRS_NKEYS][MAXCHORD];    /* each key's notes out, its root first (-1: none, or past them: into the
                                          * arpeggio, or none went; a chord's, its KEYS, card #194) */
    int8_t anote[FRS_NKEYS][MAXCHORD];   /* ... its notes into the arpeggio (-1: none) */
    uint8_t kch[FRS_NKEYS];              /* ... on that channel */
    uint8_t kp[FRS_NKEYS], kp_sent[FRS_NKEYS];   /* its pressure now, and as POLY sent it */
    uint8_t cp_sent;                     /* CHANNEL's pressure sent last */
    uint8_t last, last_vel;              /* the last note out, its velocity */
    uint32_t out[4];                     /* the notes out now (bit n % 32 of out[n / 32]): the shell's strip */
    uint32_t sent;
    mo_ev ring[MO_RING];
    uint32_t r, w;                       /* the ring's reads and writes */
} mot = {.r = 0};
/* its sequence playing (msq: app/mseq.c's) */
static struct {
    uint8_t run, wait;                   /* playing; begun: its first step at the grid's next of its RATE (1), or now (2) */
    int8_t pos;                          /* the step playing, -1: none yet */
    int16_t sounding;                    /* its note out, -1: none */
    uint8_t sch;
    float t, g;                          /* MIDI clocks into the step; the grid at the last pass */
} mos = {.pos = -1, .sounding = -1};
/* the arpeggio's notes out on their time (ma: app/mseq.c's) */
static struct {
    int16_t note[2];                     /* the notes out, the newest first, -1: none; their channels */
    uint8_t ch[2], leg[2];
    uint32_t off[2];                     /* when each lets go */
    int16_t due;                         /* a step's note waiting for its time (SWING's second of a pair), -1: none */
    uint8_t due_vel, due_leg;
    uint32_t due_at, due_len;
} ma = {.note = {-1, -1}, .due = -1};
static struct {
    float g;
    uint8_t run;                         /* Start sent, no Stop after it */
    uint8_t told, started;               /* (frs_skip_to's: run as the shell last had it, a Start the skip went past) */
} mck;

/* ------------------------------------------------------------ out --- */
static int mo_held_back(uint32_t s, uint32_t d1)   /* (frs_skip_to's brain alone: nothing out but what lets go, no burst
                                                    * after) */
{
    return mo_skip && (s & 0xF0u) != 0x80u && !((s & 0xF0u) == 0xB0u && (d1 == 64u || d1 == 123u));
}
static void mo_send(uint32_t s, uint32_t d1, uint32_t d2)   /* into the ring (NOTES OUT's: here alone) */
{
    if (mo_held_back(s, d1))
        return;
    if (mot.w - mot.r >= MO_RING)          /* (the shell not reading: the oldest goes) */
        mot.r++;
    mot.ring[mot.w % MO_RING].ms = eng_ms();
    mot.ring[mot.w % MO_RING].s = (uint8_t)s;
    mot.ring[mot.w % MO_RING].d1 = (uint8_t)d1;
    mot.ring[mot.w % MO_RING].d2 = (uint8_t)d2;
    mot.w++;
    mot.sent++;
}
static void mo_put(uint32_t s, uint32_t d1, uint32_t d2)   /* ... MIDI OUT's own, on its strip */
{
    uint32_t n = d1 & 127u;
    if (mo_held_back(s, d1))
        return;
    mo_send(s, d1, d2);
    if ((s & 0xF0u) == 0x90u)
        mot.out[n >> 5] |= 1u << (n & 31u);
    else if ((s & 0xF0u) == 0x80u)
        mot.out[n >> 5] &= ~(1u << (n & 31u));
}
static uint32_t mo_curve(uint32_t v)     /* VELOCITY: as played (NORMAL), more for a light touch (SOFT), less (HARD), 100 */
{
    v = v < 1u ? 1u : v > 127u ? 127u : v;
    if (mo_v[FRS_MO_VEL] == MO_VEL_SOFT)
        v = (uint32_t)(fm_sqrtf((float)v * 127.0f) + 0.5f);
    else if (mo_v[FRS_MO_VEL] == MO_VEL_HARD)
        v = (v * v + 63u) / 127u;
    else if (mo_v[FRS_MO_VEL] == MO_VEL_FIXED)
        v = 100u;
    return v < 1u ? 1u : v > 127u ? 127u : v;
}
static uint32_t mo_ch(void) { return mo_v[FRS_MO_CH] & 15u; }

/* --------------------------------------------------- the arpeggio --- */
static void ma_off(uint32_t i)
{
    mo_put(0x80u | ma.ch[i], (uint32_t)ma.note[i], 0);
    if (!i) {
        ma.note[0] = ma.note[1];
        ma.ch[0] = ma.ch[1];
        ma.leg[0] = ma.leg[1];
        ma.off[0] = ma.off[1];
    }
    ma.note[1] = -1;
}
static void ma_flush(void)
{
    while (ma.note[0] >= 0)
        ma_off(0);
    ma.due = -1;
}
static void ma_out(uint32_t now)         /* the step waiting, out now (the same note out already: let go first) */
{
    uint32_t i;
    int note = ma.due;
    ma.due = -1;
    for (i = 2u; i-- > 0u;)
        if (ma.note[i] == note || (i == 1u && ma.note[1] >= 0))
            ma_off(i);
    mo_put(0x90u | mo_ch(), (uint32_t)note, ma.due_vel);
    ma.note[1] = ma.note[0];
    ma.ch[1] = ma.ch[0];
    ma.leg[1] = ma.leg[0];
    ma.off[1] = ma.off[0];
    ma.note[0] = (int16_t)note;
    ma.ch[0] = (uint8_t)mo_ch();
    ma.leg[0] = ma.due_leg;
    ma.off[0] = now + ma.due_len;
    if (ma.note[1] >= 0 && ma.leg[1])    /* (LEGATO: the one before let go once this one is out) */
        ma_off(1);
    mot.last = (uint8_t)note;
    mot.last_vel = ma.due_vel;
}
static void ma_run(void)
{
    uint32_t now = eng_ms(), i;
    if (ma.due >= 0 && (mo_v[FRS_MO_TYPE] != SQT_ARP || marp.paused))
        ma.due = -1;
    for (i = 2u; i-- > 0u;)
        if (ma.note[i] >= 0 && !ma.leg[i] && (int32_t)(now - ma.off[i]) >= 0)
            ma_off(i);
    if (ma.due >= 0 && (int32_t)(now - ma.due_at) >= 0)
        ma_out(now);
    for (i = 2u; i-- > 0u;)
        if (ma.note[i] >= 0 && ma.leg[i] && ma.due < 0 && (int32_t)(now - ma.off[i]) >= 0)
            ma_off(i);
}
/* the engine's arp_step, marp's: its note out on its time (SWING: a pair's second late), as long as GATE says */
static void mout_arp_on(int note, float v)
{
    uint32_t now = eng_ms(), ticks = ARP_TICKS[mo_v[FRS_MO_RATE] % 10u];
    float k = (float)ticks, ms = grid_ms_tick(now), s = 0.5f + (float)mo_v[FRS_MO_SWING] * 0.01f;
    int32_t n = (int32_t)(grid.tick / k);
    float into = grid.tick - (float)n * k, late = n & 1 ? (2.0f * s - 1.0f) * k : 0.0f;
    float len = (n & 1 ? 2.0f * (1.0f - s) : 2.0f * s) * k * ms;
    if (note < 0 || note > 127)
        return;
    if (ma.due >= 0)
        ma_out(now);
    ma.due = (int16_t)lk_note(LK_MIDI, note);   /* (TRACKS LINK on MIDI: out where it moves it, card #212) */
    ma.due_vel = (uint8_t)(mo_v[FRS_MO_AVEL] ? mo_v[FRS_MO_AVEL] : mo_curve((uint32_t)(v * 127.0f + 0.5f)));
    ma.due_leg = mo_v[FRS_MO_AGATE] >= MO_LEGATO;
    ma.due_at = now + (late > into ? (uint32_t)((late - into) * ms + 0.5f) : 0u);
    len *= ma.due_leg ? 1.5f : (float)mo_v[FRS_MO_AGATE] * 0.01f;
    ma.due_len = len < 1.0f ? 1u : (uint32_t)(len + 0.5f);
    ma_run();
}
static void mout_arp_off(int note) { (void)note; }   /* (ours on GATE's time: ma_run) */

/* --------------------------------------------------- the sequence --- */
static uint32_t mo_len(void) { return mo_v[FRS_MO_LEN] + 1u; }
static void mos_off(void)
{
    if (mos.sounding >= 0)
        mo_put(0x80u | mos.sch, (uint32_t)mos.sounding, 0);
    mos.sounding = -1;
}
static void mos_begin(int wait)
{
    mos_off();
    mos.run = 1;
    mos.wait = (uint8_t)wait;
    mos.pos = -1;
}
static void mos_stop(void)
{
    mos_off();
    mos.run = 0;
    mos.pos = -1;
}
static void mos_step(int32_t i)
{
    uint32_t b = mo_seq[i], note;
    mos.pos = (int8_t)i;
    if ((b & 127u) == SQ_TIE)
        return;
    mos_off();
    if ((b & 127u) == SQ_REST || a_rand(&marp) % 127u >= mo_v[FRS_MO_CHANCE])
        return;
    note = (uint32_t)clamp(lk_note(LK_MIDI, scale_shift((int)(b & 127u), (int)mo_v[FRS_MO_TRANS] - 14, st.v)), 0, 127);
    mo_put(0x90u | mo_ch(), note, mo_curve(b & SQ_ACC ? 127u : 100u));
    mos.sounding = (int16_t)note;
    mos.sch = (uint8_t)mo_ch();
    mot.last = (uint8_t)note;
    mot.last_vel = (uint8_t)mo_curve(b & SQ_ACC ? 127u : 100u);
}
static void mos_run(void)
{
    uint32_t ticks = ARP_TICKS[mo_v[FRS_MO_RATE] % 10u], len = mo_len();
    float g = grid.tick, dg = g - mos.g, k = (float)ticks;
    if (dg < -4800.0f)                   /* (the grid wraps at 9600) */
        dg += 9600.0f;
    else if (dg < 0.0f)
        dg = 0.0f;
    mos.g = g;
    if (!mos.run || mo_v[FRS_MO_TYPE] != SQT_SEQ) {
        mos_off();
        return;
    }
    if (mos.wait == 1) {                  /* at the grid's next step of its RATE */
        if (fm_floorf(g / k) == fm_floorf((g - dg) / k))
            return;
        mos.t = g - k * fm_floorf(g / k);
    } else if (mos.wait == 2) {
        mos.t = g < k ? g : 0.0f;
    } else {
        mos.t += dg;
    }
    if (mos.wait) {
        mos.wait = 0;
        mos_step(0);
    }
    while (mos.t >= k) {
        mos.t -= k;
        mos_step(mos.pos + 1 < (int32_t)len ? mos.pos + 1 : 0);
    }
    if (mo_v[FRS_MO_GATE] < 127u && mos.sounding >= 0 && mos.pos >= 0 &&   /* GATE: its note let go in its last step */
        (mo_seq[mos.pos + 1 < (int32_t)len ? mos.pos + 1 : 0] & 127u) != SQ_TIE &&
        mos.t >= k * (0.05f + 0.95f * (float)mo_v[FRS_MO_GATE] * (1.0f / 127.0f)))
        mos_off();
}

/* ------------------------------------------------------- CLOCK OUT --- */
static int mo_transport(void) { return transport_on() || (mo_v[FRS_MO_TYPE] == SQT_SEQ && mos.run); }
static void mo_clock(void)
{
    float g = grid.tick, dg = g - mck.g;
    uint32_t n = 0;
    int on = mo_transport();
    if (dg < -4800.0f)
        dg += 9600.0f;
    if (!mo_v[FRS_MO_CLOCK]) {
        if (mck.run)                     /* (turned off while running: its Stop) */
            mo_put(0xFCu, 0, 0);
        mck.g = g;
        mck.run = 0;
        return;
    }
    if (on != mck.run) {                 /* Start, Stop (frs_skip_to's held back: mo_skip_end) */
        mo_put(on ? 0xFAu : 0xFCu, 0, 0);
        mck.run = (uint8_t)on;
        mck.started |= (uint8_t)(on && mo_skip);
    }
    if (dg < 0.0f) {
        mck.g = g;
        return;
    }
    while (fm_floorf(mck.g) != fm_floorf(g) && n < 8u) {   /* a clock each whole one the grid passed */
        mck.g = fm_floorf(mck.g) + 1.0f;
        if (mck.g >= 9600.0f)
            mck.g -= 9600.0f;
        mo_put(0xF8u, 0, 0);
        n++;
    }
    mck.g = g;
}

/* ------------------------------------------------------- NOTES OUT --- */
/* NOTES OUT (card #201: app.c nout_*'s, the FM-1's GLO › MIDI II K3; frs_mout_notes, off at the start): the notes the
 * brain plays by itself out as it strikes them (core.c's GEN_NOTE, GEN_CUT, GEN_PULSE), its sound as it was. GLINT's
 * arpeggio and sequence on channel 14, SPARKLE's glints on 15, the bass's arpeggio and sequence on 16, each note as it
 * sounds (GLINT's OCTAVE on it, the bass's), at its strike's velocity. Each let go as the engine lets its own go
 * (no_run: the note no longer the one its place sounds), at once at RELEASE ALL, CUT, a scene's load (GEN_CUT), a panic
 * (mo_all_stop), OFF; PULSE's let go for its gap and out again. A note on only with room in the ring past NO_ROOM, its
 * note off always; a note of a channel out once (struck again, by any place: let go first); none struck in frs_skip_to.
 * Not on MIDI OUT's strip (mo_send: the ring alone) */
#define NO_ROOM 8u
static const uint8_t NO_CH[NGEN] = {13, 13, 14, 14, 15};   /* (each place's channel, less one: 14, 15, 16) */
#define NO_TRK(i) ((uint32_t)NO_CH[i] - 13u)   /* (its track: 0 GLINT, 1 SPARKLE, 2 the bass; GEN_CUT's bits) */
static struct {
    uint8_t on;
    int16_t at[NGEN];                    /* each place's note as the engine has it, -1: none */
    int8_t out[NGEN];                    /* ... as it went out (as it sounds), -1: none out */
    uint8_t bass_vel;                    /* the bass's out at (PULSE's strike again the same) */
    int8_t again;                        /* PULSE's: the bass's note out again once its gap ends, -1: none */
    uint32_t again_ms;
} no = {.at = {-1, -1, -1, -1, -1}, .out = {-1, -1, -1, -1, -1}, .again = -1};
static int no_room(void) { return MO_RING - (mot.w - mot.r) > NO_ROOM; }
static void no_off(uint32_t i)
{
    mo_send(0x80u | NO_CH[i], (uint32_t)no.out[i], 0);
    no.out[i] = -1;
}
/* GEN_NOTE: place `at` struck `note` (as the engine has it) at velocity v (0 .. 1, an accent more) */
static void no_note(uint32_t at, int note, float v)
{
    uint32_t i, vel = (uint32_t)(v * 127.0f + 0.5f);
    int n;
    if (!no.on || at >= NGEN)
        return;
    n = at == GEN_BASS ? bass_sounding(lk_note(LK_BASS, note))   /* (TRACKS LINK's as they sound: not SPARKLE's) */
        : (at == GEN_SPK || at == GEN_SPK2 ? note : lk_note(LK_GLINT, note)) + 12 * (int)st.v[P_GL_OCTAVE];
    no.at[at] = (int16_t)note;
    if (at == GEN_BASS)
        no.again = -1;
    for (i = 0; i < NGEN; i++)           /* (its own last, and this note of its channel another place's: let go first) */
        if (no.out[i] >= 0 && (i == at || (NO_CH[i] == NO_CH[at] && no.out[i] == n)))
            no_off(i);
    if (mo_skip || n < 0 || n > 127 || !no_room())
        return;
    vel = vel < 1u ? 1u : vel > 127u ? 127u : vel;
    mo_send(0x90u | NO_CH[at], (uint32_t)n, vel);
    no.out[at] = (int8_t)n;
    if (at == GEN_BASS)
        no.bass_vel = (uint8_t)vel;
}
static void no_cut(uint32_t tracks)      /* GEN_CUT: those tracks' notes let go now (1 GLINT, 2 SPARKLE, 4 the bass) */
{
    uint32_t i;
    for (i = 0; i < NGEN; i++)
        if (no.out[i] >= 0 && (tracks >> NO_TRK(i) & 1u))
            no_off(i);
    if (tracks & 4u)
        no.again = -1;
}
static void no_pulse(float ms)           /* GEN_PULSE: the bass's note let go for PULSE's gap, then out again (no_run) */
{
    if (!no.on || no.out[GEN_BASS] < 0)
        return;
    no.again = no.out[GEN_BASS];
    no_off(GEN_BASS);
    no.again_ms = eng_ms() + (uint32_t)(ms + 0.5f);
}
static void no_move(void)                /* GEN_MOVE: the bass's note retuned by TRACKS LINK (card #212, app.c nout_move's):
                                          * its note out let go and out again where it sounds now; in PULSE's gap, out
                                          * again there */
{
    int n;
    if (!no.on || no.at[GEN_BASS] < 0)
        return;
    n = bass_sounding(lk_note(LK_BASS, no.at[GEN_BASS]));
    if (no.again >= 0)
        no.again = (int8_t)n;
    if (no.out[GEN_BASS] < 0 || no.out[GEN_BASS] == n)
        return;
    no_off(GEN_BASS);
    if (mo_skip || n < 0 || n > 127 || !no_room())
        return;
    mo_send(0x90u | NO_CH[GEN_BASS], (uint32_t)n, no.bass_vel);
    no.out[GEN_BASS] = (int8_t)n;
}
/* each pass, after the brain's: a place's note out let go once the engine's own is gone (let go, or another struck
 * there by hand: a key on the bass), PULSE's out again after its gap while it is still the bass's */
static void no_run(void)
{
    int16_t has[NGEN];
    uint32_t i;
    if (!no.on)
        return;
    has[GEN_ARP] = arp.sounding;
    has[GEN_SEQ] = sq.sounding;
    has[GEN_SPK] = spk.note[0];
    has[GEN_SPK2] = spk.note[1];
    has[GEN_BASS] = (int16_t)(bass_note >= 0 && (barp.sounding == bass_note || bq.sounding == bass_note) ? bass_note : -1);
    for (i = 0; i < NGEN; i++)
        if (no.out[i] >= 0 && has[i] != no.at[i])
            no_off(i);
    if (no.again >= 0 && (int32_t)(eng_ms() - no.again_ms) >= 0) {
        if (!mo_skip && has[GEN_BASS] == no.at[GEN_BASS] && no.out[GEN_BASS] < 0 && no_room()) {
            mo_send(0x90u | NO_CH[GEN_BASS], (uint32_t)no.again, no.bass_vel);
            no.out[GEN_BASS] = no.again;
        }
        no.again = -1;
    }
}

void frs_mout_notes(int on)
{
    no.on = (uint8_t)(on != 0);
    if (!no.on)
        no_cut(7u);
}

/* each millisecond, after the brain's (frasil_engine.c eng_tick): the arpeggio, its notes out, the sequence, the clock,
 * NOTES OUT's let go as the brain let its own go */
static void mo_pass(void)
{
    uint32_t now = eng_ms(), ticks = ARP_TICKS[mo_v[FRS_MO_RATE] % 10u];
    float ms_tick = grid_ms_tick(now);
    marp.latch = mo_v[FRS_MO_LATCH];
    arp_turn(&marp, mo_v[FRS_MO_TYPE], mo_v[FRS_MO_MODE], mo_v[FRS_MO_RANGE], mo_v[FRS_MO_CHANCE], (int32_t)(grid.tick / (float)ticks),
             now, (uint32_t)((float)ticks * ms_tick), ticks, ARP_SNAP_TICKS[st.v[P_ARP_SNAP] % 6u], ms_tick);
    ma_run();
    mos_run();
    mo_clock();
    no_run();
}

static void mo_key_offs(uint32_t k);     /* (the keys, below) */
/* RELEASE ALL, CUT, a panic: the keys' notes let go, the arpeggio emptied, the sequence stopped, every note out ended
 * (NOTES OUT's too) */
static void mo_all_stop(void)
{
    uint32_t k;
    for (k = 0; k < FRS_NKEYS; k++)
        if (mot.own >> k & 1u) {
            mo_key_offs(k);
            memset(mot.anote[k], -1, sizeof mot.anote[k]);
        }
    mot.own = 0;
    no_cut(7u);
    mos_stop();
    marp.n = marp.pn = marp.gap = 0;
    marp.dirty = 0;
    marp.paused = 0;
    arp_stop(&marp);
    ma_flush();
    if (mot.cp_sent) {
        mo_put(0xD0u | mo_ch(), 0, 0);
        mot.cp_sent = 0;
    }
}
/* PLAY for everything (frs_play_all): its sequence from the top with the others, stopped with them; its arpeggio waits */
static void mo_play_all(int on)
{
    if (mo_v[FRS_MO_TYPE] == SQT_SEQ) {
        if (on)
            mos_begin(1);
        else
            mos_stop();
    } else if (mo_v[FRS_MO_TYPE] == SQT_ARP) {
        arp_wait(&marp, !on);
    }
}

/* ------------------------------------------------------------ keys --- */
static void mo_channel_prs(void)         /* CHANNEL: the hardest key's, on a change of two or more, or to 0 */
{
    uint32_t k, top = 0;
    for (k = 0; k < FRS_NKEYS; k++)
        if (mot.own >> k & 1u && mot.note[k][0] >= 0 && mot.kp[k] > top)
            top = mot.kp[k];
    if (top + 2u > mot.cp_sent && top < mot.cp_sent + 2u && (top || !mot.cp_sent))
        return;
    mo_put(0xD0u | mo_ch(), top, 0);
    mot.cp_sent = (uint8_t)top;
}

/* another of its keys down into its arpeggio, but key `except` */
static int mo_held(uint32_t except)
{
    uint32_t j;
    for (j = 0; j < FRS_NKEYS; j++)
        if (j != except && (mot.own >> j & 1u) && mot.anote[j][0] >= 0)
            return 1;
    return 0;
}
/* another of its keys than `but` has note n into its arpeggio (a chord's: it stays) */
static int mo_held_note(uint32_t but, int n)
{
    uint32_t j, i;
    for (j = 0; j < FRS_NKEYS; j++)
        if (j != but && (mot.own >> j & 1u))
            for (i = 0; i < MAXCHORD; i++)
                if (mot.anote[j][i] == n)
                    return 1;
    return 0;
}
/* another of its keys than `but` has note n out on channel ch (a chord's, or two keys on one note of the scale: one
 * note on, its note off with the last of them, card #194: app.c mout_has) */
static int mo_has(uint32_t but, int n, uint32_t ch)
{
    uint32_t j, i;
    for (j = 0; j < FRS_NKEYS; j++)
        if (j != but && (mot.own >> j & 1u) && mot.kch[j] == ch)
            for (i = 0; i < MAXCHORD; i++)
                if (mot.note[j][i] == n)
                    return 1;
    return 0;
}
/* key k's notes out let go, each but one another key still has out (on its channel) */
static void mo_key_offs(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < MAXCHORD; i++) {
        int n = mot.note[k][i];
        mot.note[k][i] = -1;
        if (n >= 0 && !mo_has(k, n, mot.kch[k]))
            mo_put(0x80u | mot.kch[k], (uint32_t)n, 0);
    }
}
/* what a key sends (its KEYS, card #194: app/mseq.c md_key_notes): HARMONY OFF, the note as played; SINGLE NOTE, or
 * its SEQ (one note a step), the scale's note at or below it; CHORD, HARMONY's chord on it, its root first */
static void mo_notes(int note, chord_t *c)
{
    touch_t t;
    int root;
    t.harm = mo_v[FRS_MO_HARM];
    t.chord = mo_v[FRS_MO_NOTES];
    t.latch = t.stack = 0;
    key_notes(1u, t, note, mo_v[FRS_MO_TYPE] == SQT_SEQ, c, &root);
}

/* ------------------------------------------------------------- API --- */
void frs_mout_key(uint32_t k, int note, uint32_t vel, int down)
{
    chord_t c;
    uint32_t i;
    if (k >= FRS_NKEYS)
        return;
    if (!down) {
        if (!(mot.own >> k & 1u))
            return;
        if (!marp.latch)                 /* (its notes out of the arpeggio, but one another key down still has) */
            for (i = 0; i < MAXCHORD; i++)
                if (mot.anote[k][i] >= 0 && !mo_held_note(k, mot.anote[k][i]))
                    arp_remove(&marp, mot.anote[k][i]);
        memset(mot.anote[k], -1, sizeof mot.anote[k]);
        mo_key_offs(k);                  /* (each note off, unless another key holds the same note on its channel) */
        mot.own &= ~(1u << k);
        mot.kp[k] = 0;
        if (mo_v[FRS_MO_PRS] == MO_PRS_CHANNEL)
            mo_channel_prs();
        return;
    }
    if (mot.own >> k & 1u || note < 0 || note > 127)   /* (its own keys alone: the engine's key[] never touched, a
                                                      * replay's or the shell's other keys whatever they are) */
        return;
    mot.own |= 1u << k;
    memset(mot.note[k], -1, sizeof mot.note[k]);
    memset(mot.anote[k], -1, sizeof mot.anote[k]);
    vel = vel < 1u ? 1u : vel > 127u ? 127u : vel;
    mo_notes(note, &c);                  /* (its KEYS: the note as played, the scale's, or HARMONY's chord) */
    if (mo_v[FRS_MO_TYPE] == SQT_ARP) {  /* into its arpeggio: nothing out now, its steps (arp_key's way: latched, a new
                                          * hand a new set; its other keys down MIDI's own, mo_held) */
        if (marp.latch && arp_new_hand(&marp, mo_held(k))) {
            marp.n = 0;
            marp.dirty = 1;
        }
        for (i = 0; i < c.k; i++) {
            arp_add(&marp, c.n[i], vel);
            mot.anote[k][i] = c.n[i];
        }
        marp.hand_ms = eng_ms();
        marp.paused = 0;
        return;
    }
    vel = mo_curve(vel);
    mot.kch[k] = (uint8_t)mo_ch();
    for (i = 0; i < c.k; i++) {
        int n = clamp(lk_note(LK_MIDI, c.n[i]), 0, 127);   /* (TRACKS LINK on MIDI, card #212: out where it moves it,
                                                          * its off there) */
        if (!mo_has(k, n, mot.kch[k]))   /* (out already by another key: on, not again) */
            mo_put(0x90u | mot.kch[k], (uint32_t)n, vel);
        mot.note[k][i] = (int8_t)n;
    }
    mot.kp[k] = mot.kp_sent[k] = 0;
    mot.last = (uint8_t)mot.note[k][0];
    mot.last_vel = (uint8_t)vel;
}

void frs_mout_pressure(uint32_t k, uint32_t p)
{
    if (k >= FRS_NKEYS || !(mot.own >> k & 1u))
        return;
    p = p > 127u ? 127u : p;
    if (mot.anote[k][0] >= 0) {          /* (its arpeggio's notes: their steps louder, as GLINT's; never arp_press,
                                          * whose other arpeggio is the bass's sound) */
        uint32_t i, j;
        float f = (float)p * (1.0f / 127.0f);
        for (j = 0; j < MAXCHORD && mot.anote[k][j] >= 0; j++) {
            for (i = 0; i < marp.n; i++)
                if (marp.held[i] == mot.anote[k][j])
                    marp.prs[i] = f;
            for (i = 0; i < marp.pn; i++)
                if (marp.play[i] == mot.anote[k][j])
                    marp.pprs[i] = f;
        }
        return;
    }
    if (mot.note[k][0] < 0)
        return;
    mot.kp[k] = (uint8_t)p;
    if (mo_v[FRS_MO_PRS] == MO_PRS_CHANNEL) {
        mo_channel_prs();
    } else if (mo_v[FRS_MO_PRS] == MO_PRS_POLY && (p > mot.kp_sent[k] + 1u || p + 1u < mot.kp_sent[k] || (!p && mot.kp_sent[k]))) {
        uint32_t i;
        for (i = 0; i < MAXCHORD && mot.note[k][i] >= 0; i++)   /* (a chord: each of its notes) */
            mo_put(0xA0u | mot.kch[k], (uint32_t)mot.note[k][i], p);
        mot.kp_sent[k] = (uint8_t)p;
    }
}

void frs_mout_set(uint32_t i, uint32_t v)
{
    uint32_t was;
    if (i >= FRS_MO_N)
        return;
    v = v < MO_AS[i].min ? MO_AS[i].min : v > MO_AS[i].max ? MO_AS[i].max : v;
    was = mo_v[i];
    if (v == was)
        return;
    if (i == FRS_MO_TYPE && was == SQT_SEQ)
        mos_off();
    if (i == FRS_MO_CH || i == FRS_MO_PRS) {   /* (every key's note out ends on the channel it went out on: kch; the
                                                * pressure it sent let go, CHANNEL's and POLY's) */
        uint32_t k;
        if (mot.cp_sent) {
            mo_put(0xD0u | mo_ch(), 0, 0);
            mot.cp_sent = 0;
        }
        for (k = 0; k < FRS_NKEYS; k++)
            if (mot.own >> k & 1u && mot.note[k][0] >= 0 && mot.kp_sent[k]) {
                uint32_t j;
                for (j = 0; j < MAXCHORD && mot.note[k][j] >= 0; j++)
                    mo_put(0xA0u | mot.kch[k], (uint32_t)mot.note[k][j], 0);
                mot.kp_sent[k] = 0;
            }
    }
    mo_v[i] = (uint8_t)v;
    if (i == FRS_MO_LATCH) {             /* LATCH off: the arpeggio keeps the keys still down (each note of theirs) */
        uint32_t j = 0;
        marp.latch = (uint8_t)v;
        while (!v && j < marp.n) {
            if (mo_held_note(FRS_NKEYS, marp.held[j]))
                j++;
            else
                arp_remove(&marp, marp.held[j]);
        }
    }
    if (i == FRS_MO_TYPE && v != SQT_ARP)   /* (off its arpeggio: what it plays let go, its notes kept as GLINT's) */
        ma_flush();
}

uint32_t frs_mout_get(uint32_t i) { return i < FRS_MO_N ? mo_v[i] : 0u; }
uint32_t frs_mout_max(uint32_t i) { return i < FRS_MO_N ? MO_AS[i].max : 0u; }
uint32_t frs_mout_min(uint32_t i) { return i < FRS_MO_N ? MO_AS[i].min : 0u; }

void frs_mout_text(uint32_t i, uint32_t v, char *out)
{
    static const char *const VEL[] = {"SOFT", "NORMAL", "HARD", "FIXED"}, *const PRS[] = {"OFF", "CHANNEL", "POLY"};
    out[0] = 0;
    if (i >= FRS_MO_N)
        return;
    if (MO_WORDS[i] < NPARAM) {
        value_text((int)MO_WORDS[i], (uint8_t)v, out);
    } else if (i == FRS_MO_AGATE) {
        if (v >= MO_LEGATO)
            text_cat(out, "LEGATO");
        else {
            int_text((int32_t)v, out);
            text_cat(out, " %");
        }
    } else if (i == FRS_MO_AVEL) {
        if (!v)
            text_cat(out, "AS PLAYED");
        else
            int_text((int32_t)v, out);
    } else if (i == FRS_MO_SWING) {
        if (!v)
            text_cat(out, "OFF");
        else {
            int_text((int32_t)v + 50, out);
            text_cat(out, " %");
        }
    } else if (i == FRS_MO_CH) {
        int_text((int32_t)v + 1, out);
    } else if (i == FRS_MO_VEL) {
        text_cat(out, VEL[v % 4u]);
    } else if (i == FRS_MO_PRS) {
        text_cat(out, PRS[v % 3u]);
    } else {
        text_cat(out, v ? "ON" : "OFF");
    }
}

void frs_mout_play(int on)
{
    if (mo_v[FRS_MO_TYPE] == SQT_SEQ) {
        if (on && !mos.run)
            mos_begin(1);                /* (from its first step at the grid's next step of its RATE) */
        else if (!on)
            mos_stop();
    } else if (mo_v[FRS_MO_TYPE] == SQT_ARP) {
        arp_wait(&marp, !on);
    }
}

int frs_mout_playing(void)
{
    return mo_v[FRS_MO_TYPE] == SQT_SEQ ? mos.run : mo_v[FRS_MO_TYPE] == SQT_ARP ? marp.n && !marp.paused : 0;
}

void frs_mout_steps(const uint8_t *in)
{
    memcpy(mo_seq, in, SC_SEQ);
}

void frs_mout_step(uint32_t i, uint32_t b)
{
    if (i < SC_SEQ)
        mo_seq[i] = (uint8_t)b;
}

void frs_mout_clear(void)
{
    mos_off();
    memset(mo_seq, SQ_REST, sizeof mo_seq);
    marp.n = marp.pn = 0;
    marp.dirty = 1;
    arp_stop(&marp);
    ma_flush();
}

void frs_mout_view(frs_mout_view_t *v)
{
    uint32_t i;
    memcpy(v->v, mo_v, sizeof v->v);
    memcpy(v->seq, mo_seq, sizeof v->seq);
    memcpy(v->out, mot.out, sizeof v->out);
    v->pos = mo_v[FRS_MO_TYPE] == SQT_SEQ && mos.run ? mos.pos : -1;
    v->playing = (uint8_t)frs_mout_playing();
    v->last = mot.last;
    v->last_vel = mot.last_vel;
    v->narp = marp.n;
    for (i = 0; i < 16u; i++)
        v->arp[i] = i < marp.n ? marp.held[i] : -1;
    v->sounding = ma.note[0];
    v->sent = mot.sent;
}

/* the frames handed out since frs_init (frs_render's): frame f is the engine's millisecond f * 1000 / its rate, the
 * clock a message's ms counts on (the shell places each in its render) */
uint64_t frs_mout_frames(void) { return out_frames; }

static void mo_skipped(void)             /* (after frs_skip_to: the clock and the sequence from the grid as it is, no
                                          * burst catching up) */
{
    mck.g = mos.g = grid.tick;
}

/* frs_skip_to: nothing out meanwhile but what lets go (mo_held_back); after it, CLOCK OUT's transport as it is there,
 * told: running there and not before, its Start; stopped there and running before, its Stop; running before and there
 * with a Start the skip went past (a Stop, a Start: PLAY, a stream's take begun again), Stop then Start, the DAW from
 * its top as it would have been. Its clocks from there on, none to catch up */
static void mo_skip_begin(void)
{
    mo_skip = 1;
    mck.told = mck.run;
    mck.started = 0;
}
static void mo_skip_end(void)
{
    mo_skip = 0;
    mo_skipped();
    if (mck.run && (!mck.told || mck.started)) {
        if (mck.told)
            mo_put(0xFCu, 0, 0);
        mo_put(0xFAu, 0, 0);
    } else if (!mck.run && mck.told) {
        mo_put(0xFCu, 0, 0);
    }
    mck.started = 0;
}

uint32_t frs_mout_read(frs_mout_ev_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (mot.r != mot.w && n < max) {
        out[n].ms = mot.ring[mot.r % MO_RING].ms;
        out[n].status = mot.ring[mot.r % MO_RING].s;
        out[n].d1 = mot.ring[mot.r % MO_RING].d1;
        out[n].d2 = mot.ring[mot.r % MO_RING].d2;
        mot.r++;
        n++;
    }
    return n;
}

static void mo_init(void)                /* (frs_init's: every setting its default, no steps, nothing out but what the
                                          * shell has not read yet: frs_close's panic's note-offs, at its 0, out at once) */
{
    static mo_ev keep[MO_RING];
    uint32_t i, n;
    if (mck.run)
        mo_put(0xFCu, 0, 0);
    n = mot.w - mot.r > MO_RING ? MO_RING : mot.w - mot.r;
    for (i = 0; i < n; i++) {
        keep[i] = mot.ring[(mot.w - n + i) % MO_RING];
        keep[i].ms = 0;
    }
    for (i = 0; i < FRS_MO_N; i++)
        mo_v[i] = MO_AS[i].def;
    memset(mo_seq, SQ_REST, sizeof mo_seq);
    memset(&mot, 0, sizeof mot);
    memcpy(mot.ring, keep, n * sizeof keep[0]);
    mot.w = n;
    memset(mot.note, -1, sizeof mot.note);
    memset(mot.anote, -1, sizeof mot.anote);
    mos.run = mos.wait = 0;
    mos.pos = -1;
    mos.sounding = -1;
    ma.note[0] = ma.note[1] = ma.due = -1;
    marp.latch = 1;
    mck.run = mck.told = mck.started = 0;
    no.on = 0;                           /* (NOTES OUT off, nothing of it out) */
    for (i = 0; i < NGEN; i++)
        no.at[i] = no.out[i] = -1;
    no.again = -1;
}
