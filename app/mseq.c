/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* MIDI's arpeggio and sequence (card #169): MIDI OUT's ARP and SEQ, GLINT's own way, on FRASIL's grid (MIDI clock,
 * else TEMPO), their notes out on MIDI's CHANNEL, never sounding here. The arpeggio is the engine's arpeggiator (marp,
 * engine/core.c: arp_turn, its MODE, RATE, RANGE, CHANCE, LATCH, ARP SNAP), its steps out through mout_arp_on and
 * mout_arp_off. The sequence is MIDI's own: SC_SEQ steps in GLINT's bytes (SQ_REST, a note as played, SQ_TIE, SQ_ACC),
 * SEQ LENGTH of them on RATE's steps, GATE, CHANCE, TRANSPOSE along the scale; written by STEP (at STEP, LENGTH long,
 * then on) or LIVE (playing: on the step nearest the playhead, as long as the key is held, over what was there; the key
 * sounds as played, that step silent this time round; stopped, it sounds alone), one take's UNDO TAKE. It runs with FRASIL's transport (PLAY: from the
 * top with the others, stopped with them; RELEASE ALL and CUT stop it), MIDI Start from its top.
 * Their settings are the app's (no setting in the schema, nothing in a scene record or a shard, nothing told to LIVE):
 * each a knob past NPARAM (K_MD0 + i, pages.c) that borrows a GLINT setting's name, range and words (MD_AS). A scene
 * keeps them and the steps in its own object (OBJ_MSEQ: md_load, md_save), under its record's sum, as OBJ_NAMES does:
 * a scene put there by another (the River, a restore of another's) takes none. Every note out is ended (md_off,
 * ma_flush: its channel as it went), whatever stops it. The arpeggio's own (card #172, for an instrument in a DAW):
 * GATE, VELOCITY, SWING on its II and III pages, its notes out on their time (ma_run). Part of the unity build (app.c
 * includes it after mout) */

enum { MD_TYPE, MD_MODE, MD_RATE, MD_RANGE, MD_CHANCE, MD_LATCH, MD_LEN, MD_GATE, MD_TRANS,   /* a scene keeps these */
       MD_STEP, MD_NOTE, MD_NLEN, MD_ACC, MD_UNDO, MD_REC,                                  /* (of the moment) */
       MD_AGATE, MD_AVEL, MD_SWING, NMD };   /* the arpeggio's own, kept too (card #172: md_own, OBJ_MSEQ's spare bytes) */
#define MD_KEPT 9u
#define MD_OWN 3u                        /* (MD_AGATE .. MD_SWING) */
#define K_MD(i) (K_MD0 + (i))
/* each one's GLINT setting: its name, its range, its default, its words (value_text); the arpeggio's own, none (md_own) */
static const prm_t MD_AS[NMD] = {P_SQ_TYPE, P_ARP_MODE, P_ARP_RATE, P_ARP_RANGE, P_ARP_CHANCE, P_GA_LATCH, P_SQ_LEN,
                                 P_SQ_GATE, P_SQ_TRANS, P_SQ_STEP, P_SQ_NOTE, P_SQ_NLEN, P_SQ_ACC, P_SQ_UNDO, P_SQ_REC,
                                 NPARAM, NPARAM, NPARAM};
/* the arpeggio's own (card #172): GATE 5 .. 100 % of its step (LEGATO past it: held till the next is out), VELOCITY AS
 * PLAYED (0: the key's, through MIDI OUT's VELOCITY) or 1 .. 127 as it goes out, SWING OFF (0) or 51 .. 75 % (a pair of
 * steps' first's share) */
#define MD_LEGATO 101u
static const struct {
    const char *name;
    uint8_t min, max, def;
} MD_OWN_AS[MD_OWN] = {{"GATE", 5, MD_LEGATO, 50}, {"VELOCITY", 0, 127, 0}, {"SWING", 0, 25, 0}};
static int md_own(uint32_t i) { return i >= MD_AGATE && i < NMD; }
static uint32_t md_max(uint32_t i) { return md_own(i) ? MD_OWN_AS[i - MD_AGATE].max : PD[MD_AS[i]].max; }
static uint32_t md_min(uint32_t i) { return md_own(i) ? MD_OWN_AS[i - MD_AGATE].min : 0u; }
static uint32_t md_def(uint32_t i) { return md_own(i) ? MD_OWN_AS[i - MD_AGATE].def : PD[MD_AS[i]].def; }
static const char *md_name(uint32_t i)
{
    return md_own(i) ? MD_OWN_AS[i - MD_AGATE].name : i == MD_NLEN ? "LENGTH" : PD[MD_AS[i]].name;
}
static uint8_t md[NMD];
static uint8_t mseq[SC_SEQ];
enum { K_MIDI = 33 };                    /* (a key's kind: MIDI's arpeggio's, as K_LEAD is the LEAD's) */
static struct {
    uint8_t run;                         /* playing */
    uint8_t wait;                        /* begun: its first step at the grid's next of its RATE (1), or now (2: the top) */
    int8_t pos;                          /* the step playing, -1: none yet */
    int8_t skip;                         /* a step LIVE wrote just ahead of the playhead: it sounded already */
    int16_t sounding;                    /* its note out, -1: none */
    uint8_t sch;                         /* ... on that channel */
    float t, g;                          /* MIDI clocks into the step; the grid at the last run */
    float abs;                           /* steps played since it began (LIVE's lengths) */
    int8_t tq;                           /* LIVE: the take's step (-1: no key down), its key, its byte */
    uint8_t tk, tb;
    float tat;                           /* ... and abs when it went down */
    uint8_t len;                         /* STEP: the LENGTH a key writes */
    uint8_t undo[SC_SEQ], can_undo;      /* UNDO TAKE: the steps as the take found them */
    uint32_t undo_at;                    /* its first detent (the second, within 2 s, undoes) */
    uint32_t at;                         /* a knob of theirs turned (the page's lamp), and which */
    uint8_t at_i;
    uint32_t steps;                      /* notes the sequence sent (the tests) */
} msq = {.pos = -1, .skip = -1, .sounding = -1, .tq = -1};
/* the arpeggio's notes out (card #172): each step's note from the engine (mout_arp_on), out on its time and let go on
 * GATE's, here (ma_run; the engine's own let-go, half a step, nothing): two at most, an overlap's */
static struct {
    int16_t note[2];                     /* the notes out, the newest first, -1: none; their channels */
    uint8_t ch[2];
    uint8_t leg[2];                      /* ... LEGATO's: let go once the next is out */
    uint32_t off[2], on[2];              /* plat_ms each lets go at (LEGATO: none came, half a step past its own), went out */
    int16_t due;                         /* a step's note waiting for its time (SWING's second of a pair), -1: none */
    uint8_t due_vel, due_leg;
    uint32_t due_at, due_len;            /* ... when, and how long it sounds (ms) */
    uint32_t at;                         /* the last went out */
    uint32_t len, ioi[2], over;          /* (the tests: the last let go's length, the last two times between notes out,
                                          * ms; the notes out over another) */
} ma = {.note = {-1, -1}, .due = -1};
static uint32_t ma_steps;                /* (the tests) */

static uint32_t md_len(void) { return md[MD_LEN] + 1u; }
static void md_defaults(void)            /* NEW SHARD's, a scene without its own: KEYS, GLINT's defaults, no steps */
{
    uint32_t i;
    for (i = 0; i < MD_KEPT; i++)
        md[i] = (uint8_t)md_def(i);
    for (i = MD_AGATE; i < NMD; i++)
        md[i] = (uint8_t)md_def(i);
    md[MD_TYPE] = 0;
    memset(mseq, SQ_REST, sizeof mseq);
}
/* SEQ II's NOTE, ACCENT and LENGTH: the step on STEP's (STEP recording: LENGTH the one carried on) */
static void md_view(void)
{
    uint32_t c = md[MD_STEP] % SC_SEQ;
    int32_t s = step_start(mseq, c);
    md[MD_NOTE] = mseq[c] & 127u;
    md[MD_ACC] = mseq[c] >> 7;
    md[MD_NLEN] = (uint8_t)(md[MD_REC] == 1u ? (msq.len ? msq.len : 1u) : s >= 0 ? step_len(mseq, (uint32_t)s, md_len()) : 0u);
}

/* ------------------------------------------------------------ out --- */
static void ma_off(uint32_t i)           /* the arpeggio's note i let go (its channel as it went) */
{
    mout_put(0x80u | ma.ch[i], (uint32_t)ma.note[i], 0);
    ma.len = plat_ms() - ma.on[i];
    if (!i) {
        ma.note[0] = ma.note[1];
        ma.ch[0] = ma.ch[1];
        ma.leg[0] = ma.leg[1];
        ma.off[0] = ma.off[1];
        ma.on[0] = ma.on[1];
    }
    ma.note[1] = -1;
}
static void ma_flush(void)               /* every note of the arpeggio let go now, the one waiting dropped */
{
    while (ma.note[0] >= 0)
        ma_off(0);
    ma.due = -1;
}
static void ma_out(uint32_t now)         /* the step waiting, out now (the same note out already: let go first) */
{
    uint32_t i, vel = ma.due_vel;
    int note = ma.due;
    ma.due = -1;
    for (i = 2u; i-- > 0u;)
        if (ma.note[i] == note || (i == 1u && ma.note[1] >= 0))   /* (a third: the oldest goes) */
            ma_off(i);
    if (!mout_room(MO_ROOM_NOTE))
        return;
    mout_put(0x90u | mout.ch, (uint32_t)note, vel);
    ma.ioi[1] = ma.ioi[0];
    ma.ioi[0] = now - ma.at;
    ma.at = now;
    ma.over += ma.note[0] >= 0;
    ma.note[1] = ma.note[0];
    ma.ch[1] = ma.ch[0];
    ma.leg[1] = ma.leg[0];
    ma.off[1] = ma.off[0];
    ma.on[1] = ma.on[0];
    ma.note[0] = (int16_t)note;
    ma.ch[0] = mout.ch;
    ma.leg[0] = ma.due_leg;
    ma.on[0] = now;
    ma.off[0] = now + ma.due_len;
    if (ma.note[1] >= 0 && ma.leg[1])    /* (LEGATO: the one before let go once this one is out) */
        ma_off(1);
    mout.last = (uint8_t)note;
    mout.last_vel = (uint8_t)vel;
    ma_steps++;
}
/* each pass: those let go on their time, then the step waiting on its (SWING), then LEGATO's left with nothing after */
static void ma_run(void)
{
    uint32_t now = plat_ms(), i;
    if (ma.due >= 0 && (md[MD_TYPE] != SQT_ARP || marp.paused))
        ma.due = -1;                     /* (the arpeggio stopped before it) */
    for (i = 2u; i-- > 0u;)
        if (ma.note[i] >= 0 && !ma.leg[i] && (int32_t)(now - ma.off[i]) >= 0)
            ma_off(i);
    if (ma.due >= 0 && (int32_t)(now - ma.due_at) >= 0)
        ma_out(now);
    for (i = 2u; i-- > 0u;)
        if (ma.note[i] >= 0 && ma.leg[i] && ma.due < 0 && (int32_t)(now - ma.off[i]) >= 0)
            ma_off(i);
}
/* the engine's arp_step, marp's step: its note out on its time (SWING: a pair's second late by the first's share past
 * half), as long as GATE says of its step (a swung pair's steps their own lengths), at VELOCITY (AS PLAYED: the key's,
 * through MIDI OUT's VELOCITY). Times on the grid's tempo now (MIDI clock, else TEMPO) */
static void mout_arp_on(int note, float v)
{
    uint32_t now = plat_ms(), ticks = ARP_TICKS[md[MD_RATE] % 10u];
    float k = (float)ticks, ms = grid_ms_tick(now), s = 0.5f + (float)md[MD_SWING] * 0.01f;
    int32_t n = (int32_t)(grid.tick / k);
    float into = grid.tick - (float)n * k, late = n & 1 ? (2.0f * s - 1.0f) * k : 0.0f;   /* (clocks) */
    float len = (n & 1 ? 2.0f * (1.0f - s) : 2.0f * s) * k * ms;   /* (its step's, ms) */
    if (note < 0 || note > 127)
        return;
    if (ma.due >= 0)                     /* (one still waiting: out first) */
        ma_out(now);
    ma.due = (int16_t)note;
    ma.due_vel = (uint8_t)(md[MD_AVEL] ? md[MD_AVEL] : mout_curve((uint32_t)(v * 127.0f + 0.5f)));
    ma.due_leg = md[MD_AGATE] >= MD_LEGATO;
    ma.due_at = now + (late > into ? (uint32_t)((late - into) * ms + 0.5f) : 0u);
    len *= ma.due_leg ? 1.5f : (float)md[MD_AGATE] * 0.01f;   /* (LEGATO: none after it, half a step on) */
    ma.due_len = len < 1.0f ? 1u : (uint32_t)(len + 0.5f);
    ma_run();
}
static void mout_arp_off(int note)       /* (arp_stop: the engine's half step, its stop; ours on GATE's, ma_run) */
{
    (void)note;
}
static void md_off(void)                 /* the sequence's note let go */
{
    if (msq.sounding >= 0)
        mout_put(0x80u | msq.sch, (uint32_t)msq.sounding, 0);
    msq.sounding = -1;
}
static void md_on(int note, uint32_t vel)
{
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (!mout_room(MO_ROOM_NOTE))
        return;
    vel = mout_curve(vel);
    mout_put(0x90u | mout.ch, (uint32_t)note, vel);
    msq.sounding = (int16_t)note;
    msq.sch = mout.ch;
    mout.last = (uint8_t)note;
    mout.last_vel = (uint8_t)vel;
    msq.steps++;
}

/* --------------------------------------------------------- the take --- */
/* LIVE's key let go (or another taking over, or the take ended): its note laid from its step, as long as it was held
 * (to the step, one at least), over what was there */
static void md_take_end(void)
{
    uint32_t len = md_len(), d;
    float h;
    if (msq.tq < 0)
        return;
    h = msq.abs - msq.tat;
    d = (uint32_t)(h + 0.5f);
    d = d < 1u ? 1u : d > len ? len : d;
    step_lay(mseq, len, (uint32_t)msq.tq % len, d, msq.tb);
    msq.tq = -1;
    md_view();
    st.dirty = 1;
}
static void md_begin(int wait)           /* from its first step: at the grid's next of its RATE (1), or now (2) */
{
    md_take_end();
    md_off();
    msq.run = 1;
    msq.wait = (uint8_t)wait;
    msq.pos = msq.skip = -1;
    msq.abs = 0.0f;
}
static void md_stop(void)
{
    md_take_end();
    md_off();
    msq.run = 0;
    msq.pos = -1;
}
static void md_rec_off(void)
{
    md_take_end();
    md[MD_REC] = 0;
    md_view();
}

/* ------------------------------------------------------- the grid --- */
static void md_step(int32_t i)
{
    uint32_t b = mseq[i];
    msq.pos = (int8_t)i;
    if ((b & 127u) == SQ_TIE)            /* held on through it */
        return;
    md_off();
    if (i == msq.skip) {                 /* (LIVE wrote it a moment ago: it sounded then) */
        msq.skip = -1;
        return;
    }
    msq.skip = -1;
    if ((b & 127u) == SQ_REST || msq.tq >= 0)   /* (a take's key down: the take's alone) */
        return;
    if (a_rand(&marp) % 127u >= md[MD_CHANCE])
        return;
    md_on(scale_shift((int)(b & 127u), (int)md[MD_TRANS] - 14, st.v), b & SQ_ACC ? 127u : 100u);
}
static void md_run(void)
{
    uint32_t ticks = ARP_TICKS[md[MD_RATE] % 10u], len = md_len();
    float g = grid.tick, dg = g - msq.g, k = (float)ticks;
    if (dg < -4800.0f)                   /* (the grid wraps at 9600; back at its top, PLAY held) */
        dg += 9600.0f;
    else if (dg < 0.0f)
        dg = 0.0f;
    msq.g = g;
    if (!msq.run || md[MD_TYPE] != SQT_SEQ) {
        md_off();
        return;
    }
    if (msq.wait == 1) {                 /* at the grid's next step of its RATE */
        if (fm_floorf(g / k) == fm_floorf((g - dg) / k))
            return;
        msq.t = g - k * fm_floorf(g / k);
    } else if (msq.wait == 2) {          /* at the grid's top, now */
        msq.t = g < k ? g : 0.0f;
    } else {
        msq.t += dg;
        msq.abs += dg / k;
    }
    if (msq.wait) {
        msq.wait = 0;
        md_step(0);
    }
    while (msq.t >= k) {
        msq.t -= k;
        md_step(msq.pos + 1 < (int32_t)len ? msq.pos + 1 : 0);
    }
    if (md[MD_GATE] < 127u && msq.sounding >= 0 && msq.pos >= 0 &&   /* GATE: its note let go in its last step */
        (mseq[msq.pos + 1 < (int32_t)len ? msq.pos + 1 : 0] & 127u) != SQ_TIE &&
        msq.t >= k * (0.05f + 0.95f * (float)md[MD_GATE] * (1.0f / 127.0f)))
        md_off();
}

/* ----------------------------------------------------------- keys --- */
/* a key down on MIDI: 1 its arpeggio's (nothing out now: its steps), 0 played through (KEYS; SEQ: RECORD OFF, or
 * written into the sequence, STEP or LIVE, as it sounds) */
static int md_key_down(uint32_t k, int note, uint32_t vel)
{
    uint8_t b = (uint8_t)((note < 1 ? 1 : note > 126 ? 126 : note) | (vel >= 100u ? SQ_ACC : 0u));
    if (md[MD_TYPE] == SQT_ARP) {
        key[k].kind = K_MIDI;
        key[k].chord.k = 1;
        key[k].chord.n[0] = (int8_t)(note < 0 ? 0 : note > 127 ? 127 : note);
        arp_key(&marp, k, K_MIDI, 0, vel);
        return 1;
    }
    if (md[MD_TYPE] != SQT_SEQ || !md[MD_REC])
        return 0;
    if (md[MD_REC] == 1u) {              /* STEP: at STEP, LENGTH long, then on past it */
        uint32_t len = md_len(), c = md[MD_STEP] % len, l = msq.len ? msq.len : 1u;
        l = l < len - c ? l : len - c;
        step_lay(mseq, len, c, l, b);
        md[MD_STEP] = (uint8_t)((c + l) % len);
    } else {                             /* LIVE: on the step nearest the playhead */
        uint32_t len = md_len(), ticks = ARP_TICKS[md[MD_RATE] % 10u];
        int32_t q = 0;
        md_take_end();                   /* (another key taking over: that one laid first) */
        if (!msq.run)                    /* (stopped: it sounds, nothing to put it on, as GLINT's take) */
            return 0;
        if (!msq.wait && msq.pos >= 0) {
            int ahead = msq.t >= 0.5f * (float)ticks;
            q = (msq.pos + ahead) % (int32_t)len;
            if (ahead)
                msq.skip = (int8_t)q;
        }
        mseq[q] = b;
        msq.tq = (int8_t)q;
        msq.tk = (uint8_t)k;
        msq.tb = b;
        msq.tat = msq.abs - (msq.wait || msq.pos < 0 ? 0.0f : msq.t / (float)ticks - (msq.t >= 0.5f * (float)ticks));
    }
    md_view();
    st.dirty = 1;
    return 0;
}
static void md_key_up(uint32_t k, int arp_note)   /* (arp_note: its arpeggio's, else -1) */
{
    if (arp_note >= 0 && !marp.latch)
        arp_remove(&marp, arp_note);
    if (msq.tq >= 0 && msq.tk == k)
        md_take_end();
}
static void md_keep_held(void)           /* LATCH off: the arpeggio keeps the keys still down */
{
    uint32_t i = 0, k;
    while (i < marp.n) {
        int down = 0;
        for (k = 0; k < FRS_NKEYS; k++)
            down |= key[k].kind == K_MIDI && key[k].chord.n[0] == marp.held[i];
        if (down)
            i++;
        else
            arp_remove(&marp, marp.held[i]);
    }
}
static void md_all_stop(void)            /* RELEASE ALL, CUT, an update: the arpeggio emptied, the sequence stopped */
{
    md_stop();
    marp.n = marp.pn = marp.gap = 0;
    marp.dirty = 0;
    marp.paused = 0;
    arp_stop(&marp);
    ma_flush();                          /* (its notes out let go now, not on their time) */
}

/* ---------------------------------------------------------- knobs --- */
static void md_type(uint32_t way)        /* ARP's or SEQ's ON (way), or OFF (0, KEYS) */
{
    if (way == md[MD_TYPE])
        return;
    if (md[MD_TYPE] == SQT_SEQ) {
        md_rec_off();
        md_off();
    }
    md[MD_TYPE] = (uint8_t)way;
    st.dirty = 1;
}
static void md_turn(uint32_t i, int32_t d, int32_t accel_d)
{
    uint32_t len = md_len(), c = md[MD_STEP] % len, max = md_max(i);
    int32_t v;
    if (i == MD_NOTE) {                  /* the step's note: REST, a note, TIE (from a REST up: the scale's middle C) */
        v = (int32_t)(mseq[c] & 127u);
        v = !v && d > 0 ? 60 : v + accel_d;
        v = v < 0 ? 0 : v > 127 ? 127 : v;
        mseq[c] = (uint8_t)((uint32_t)v | (v && v < 127 ? mseq[c] & SQ_ACC : 0u));
    } else if (i == MD_ACC) {            /* the NOTE's at STEP (or the one it carries on) */
        int32_t s = step_start(mseq, c);
        if (s >= 0)
            mseq[s] = (uint8_t)(d > 0 ? mseq[s] | SQ_ACC : mseq[s] & ~SQ_ACC);
    } else if (i == MD_NLEN) {           /* the NOTE's LENGTH (STEP recording: the one a key writes) */
        if (md[MD_REC] == 1u) {
            msq.len = (uint8_t)clamp((int32_t)(msq.len ? msq.len : 1u) + d, 1, (int32_t)SC_SEQ);
        } else {
            int32_t s = step_start(mseq, c);
            if (s >= 0)
                step_stretch(mseq, (uint32_t)s, (uint32_t)clamp((int32_t)step_len(mseq, (uint32_t)s, len) + d, 1, 32), len);
        }
    } else if (i == MD_UNDO) {           /* turned twice within 2 s: the steps as the last take found them */
        if (d <= 0 || !msq.can_undo) {
            if (!msq.can_undo)
                app_say("UNDO TAKE", "NOTHING");
        } else if (msq.undo_at && plat_ms() - msq.undo_at < 2000u) {
            md_take_end();
            memcpy(mseq, msq.undo, sizeof mseq);
            msq.can_undo = 0;
            msq.undo_at = 0;
            app_say("UNDO TAKE", "DONE");
        } else {
            msq.undo_at = plat_ms() | 1u;
            app_say("UNDO TAKE", "TURN AGAIN");
        }
    } else {
        v = clamp((int32_t)md[i] + (max > 16u ? accel_d : d), (int32_t)md_min(i), (int32_t)max);
        if (i == MD_STEP)
            v = clamp(v, 0, (int32_t)len - 1);
        md[i] = (uint8_t)v;
        if (i == MD_LEN && md[MD_STEP] > md[MD_LEN])
            md[MD_STEP] = md[MD_LEN];
        if (i == MD_LATCH) {
            marp.latch = md[MD_LATCH];
            if (!marp.latch)
                md_keep_held();
        }
    }
    if (i != MD_STEP && i != MD_UNDO)
        st.dirty = 1;
    md_view();
    msq.at = plat_ms() | 1u;
    msq.at_i = (uint8_t)i;
}
/* a knob's value as the screen writes it: its GLINT setting's words; NOTE, MIDI's note as played; the arpeggio's own */
static const char *md_text(uint32_t i, char *t)
{
    t[0] = 0;
    if (i == MD_NOTE && md[i] && md[i] < 127u) {
        text_cat(t, NOTE_NAME[md[i] % 12u]);
        int_text((int32_t)md[i] / 12 - 1, t + (t[1] == '#' ? 2 : 1));
    } else if (i == MD_NOTE) {
        text_cat(t, md[i] ? "TIE" : "REST");
    } else if ((i == MD_AGATE && md[i] >= MD_LEGATO) || (i == MD_AVEL && !md[i]) || (i == MD_SWING && !md[i])) {
        text_cat(t, i == MD_AGATE ? "LEGATO" : i == MD_AVEL ? "AS PLAYED" : "OFF");
    } else if (md_own(i)) {
        int_text((int32_t)md[i] + (i == MD_SWING ? 50 : 0), t);
        if (i != MD_AVEL)
            text_cat(t, " %");
    } else {
        value_text((int)MD_AS[i], md[i], t);
    }
    return t;
}
/* a take begins (REC: OFF to LIVE or STEP): UNDO TAKE's copy */
static void md_rec_tap(void)
{
    uint32_t r = md[MD_REC] == 0u ? 2u : md[MD_REC] == 2u ? 1u : 0u;   /* OFF, LIVE, STEP, OFF (GLINT's REC) */
    if (r && !md[MD_REC]) {
        memcpy(msq.undo, mseq, sizeof mseq);
        msq.can_undo = 1;
    }
    md_take_end();
    if (r)
        md_type(SQT_SEQ);
    md[MD_REC] = (uint8_t)r;
    if (r == 1u)
        msq.len = md[MD_NLEN] ? md[MD_NLEN] : 1u;
    md_view();
}
static int md_clear(void)                /* OCT- and REC on its SEQ pages: 1 cleared, 0 empty already */
{
    uint32_t i;
    for (i = 0; i < SC_SEQ && (mseq[i] & 127u) == SQ_REST; i++)
        ;
    if (i == SC_SEQ)
        return 0;
    memcpy(msq.undo, mseq, sizeof mseq);
    msq.can_undo = 1;
    md_take_end();
    md_off();
    memset(mseq, SQ_REST, sizeof mseq);
    md_view();
    st.dirty = 1;
    return 1;
}

/* ------------------------------------------------------ a scene's --- */
/* OBJ_MSEQ: its magic, the device's RETURN (bit 7 set: 0 .. 100; none, 100 %: the settings' record has no byte left),
 * three bytes spare, then each slot's entry: its scene record's sum (rec_sum of OBJ_SCENES' record, as stored with it),
 * its MD_KEPT settings, the arpeggio's own (card #172: each one up, 0 none: 3.3.1's padding, their defaults), its SC_SEQ
 * steps. An entry whose sum is not the slot's record's (the scene stored since by another FRASIL, the River, a
 * restore): none, MIDI's defaults */
#define MDQ_MAGIC 0x514D5246u            /* "FRMQ" */
#define MDQ_HEAD 8u
#define MDQ_ENTRY (4u + 12u + SC_SEQ)
#define MDQ_BYTES (MDQ_HEAD + NSCENES * MDQ_ENTRY)
_Static_assert(MDQ_BYTES <= PLAT_STORE_MAX && MD_KEPT + MD_OWN <= 12u, "MIDI's sequences, every slot's, in one object");
static uint32_t md_sum(uint32_t s) { return rec_sum((const uint8_t *)&scn.s[s], SC_REC); }
static int md_read(uint8_t *b)
{
    return plat_store_load(OBJ_MSEQ, b, MDQ_BYTES) == (int)MDQ_BYTES && rd32(b) == MDQ_MAGIC;
}
static uint8_t md_ret_saved = 0xFFu;      /* (RETURN as OBJ_MSEQ has it: md_dev_save writes it only when it changed) */
static void md_fresh(uint8_t *b)         /* none yet: every slot's none, RETURN 100 % */
{
    memset(b, 0, MDQ_BYTES);
    b[0] = (uint8_t)MDQ_MAGIC;
    b[1] = (uint8_t)(MDQ_MAGIC >> 8);
    b[2] = (uint8_t)(MDQ_MAGIC >> 16);
    b[3] = (uint8_t)(MDQ_MAGIC >> 24);
}
static void md_dev_load(void)            /* RETURN, at power-on and after a restore */
{
    uint8_t b[MDQ_BYTES];
    mout.ret = md_read(b) && b[4] & 0x80u && (b[4] & 127u) <= 100u ? b[4] & 127u : 100u;
    md_ret_saved = mout.ret;
}
static void md_dev_save(void)            /* RETURN kept (with the settings, once nothing sounds): only if it changed */
{
    uint8_t b[MDQ_BYTES];
    if (mout.ret == md_ret_saved)
        return;
    if (!md_read(b))
        md_fresh(b);
    b[4] = (uint8_t)(0x80u | mout.ret);
    if (plat_store_save(OBJ_MSEQ, b, MDQ_BYTES) == 0)
        md_ret_saved = mout.ret;
}
static void md_load(uint32_t s)          /* slot s's (none, NEW SHARD, a factory scene: the defaults) */
{
    uint8_t b[MDQ_BYTES];
    const uint8_t *e = b + MDQ_HEAD + s * MDQ_ENTRY;
    uint32_t i;
    md_take_end();
    md_off();
    md_defaults();
    if (s < NSCENES && (scn.used >> s & 1u) && md_read(b) && rd32(e) == md_sum(s)) {
        for (i = 0; i < MD_KEPT; i++)
            md[i] = e[4u + i] > md_max(i) ? (uint8_t)md_def(i) : e[4u + i];
        for (i = 0; i < MD_OWN; i++) {   /* (0: none, an earlier entry's) */
            uint32_t v = e[4u + MD_KEPT + i] - 1u, j = MD_AGATE + i;
            md[j] = (uint8_t)(v >= md_min(j) && v <= md_max(j) ? v : md_def(j));
        }
        memcpy(mseq, e + 16u, SC_SEQ);
    }
    marp.latch = md[MD_LATCH];
    md[MD_STEP] = 0;
    md[MD_REC] = 0;
    msq.can_undo = 0;
    md_view();
}
static int md_save(uint32_t s)           /* MIDI's now into slot s's entry: 0 written */
{
    uint8_t b[MDQ_BYTES];
    uint8_t *e = b + MDQ_HEAD + s * MDQ_ENTRY;
    uint32_t sum = md_sum(s), i;
    int r;
    if (s >= NSCENES)
        return -1;
    if (!md_read(b))                     /* (none yet: every other slot's none) */
        md_fresh(b);
    b[4] = (uint8_t)(0x80u | mout.ret);
    memset(e, 0, MDQ_ENTRY);
    for (i = 0; i < 4u; i++)
        e[i] = (uint8_t)(sum >> (8u * i));
    memcpy(e + 4u, md, MD_KEPT);
    for (i = 0; i < MD_OWN; i++)
        e[4u + MD_KEPT + i] = (uint8_t)(md[MD_AGATE + i] + 1u);
    memcpy(e + 16u, mseq, SC_SEQ);
    r = plat_store_save(OBJ_MSEQ, b, MDQ_BYTES);
    if (r == 0)
        md_ret_saved = mout.ret;
    return r;
}

/* ---------------------------------------------- each pass, the clock --- */
/* CLOCK OUT (GLO › MIDI II, the device's): the grid's clocks out, 24 a beat (MIDI clock in, else TEMPO), Start as the
 * transport begins, Stop as it ends; off, none */
static struct {
    float g;
    uint8_t run;
    uint32_t sent;                       /* (the tests) */
} mck;
static int md_transport(void) { return transport_on() || (md[MD_TYPE] == SQT_SEQ && msq.run); }
static void md_clock(void)
{
    float g = grid.tick, dg = g - mck.g;
    uint32_t n = 0;
    int on = md_transport();
    if (dg < -4800.0f)
        dg += 9600.0f;
    if (!mout.clk) {
        mck.g = g;
        mck.run = 0;
        return;
    }
    if (on != mck.run) {                 /* Start, Stop */
        mout_put(on ? 0xFAu : 0xFCu, 0, 0);
        mck.run = (uint8_t)on;
    }
    if (dg < 0.0f) {                     /* (the grid back at its top: from there) */
        mck.g = g;
        return;
    }
    while (fm_floorf(mck.g) != fm_floorf(g) && n < 8u) {   /* a clock each whole one the grid passed */
        mck.g = fm_floorf(mck.g) + 1.0f;
        if (mck.g >= 9600.0f)
            mck.g -= 9600.0f;
        if (mout_room(MO_ROOM_NOTE)) {
            mout_put(0xF8u, 0, 0);
            mck.sent++;
        }
        n++;
    }
    mck.g = g;
}

/* every pass, after the engine's (the grid moved): the arpeggio (its steps, then its notes out on their time), the
 * sequence, the clock out */
static void md_pass(void)
{
    uint32_t now = plat_ms(), ticks = ARP_TICKS[md[MD_RATE] % 10u];
    float ms_tick = grid_ms_tick(now);
    marp.latch = md[MD_LATCH];
    arp_turn(&marp, md[MD_TYPE], md[MD_MODE], md[MD_RANGE], md[MD_CHANCE], (int32_t)(grid.tick / (float)ticks), now,
             (uint32_t)((float)ticks * ms_tick), ticks, ARP_SNAP_TICKS[st.v[P_ARP_SNAP] % 6u], ms_tick);
    ma_run();
    md_run();
    md_clock();
}
