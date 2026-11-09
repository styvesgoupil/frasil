/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The engine's running state (Perform LIVE, card #131): what a start (a scene, the state, the moves) does not hold, so
 * that an engine opened on it goes on as the one it was taken from. Part of core.c (its end: every one of the brain's
 * statics in sight); frasil_engine.c's frs_live_save and frs_live_load call it, so does the FM-1's STATE (app.c).
 *   The brain whole: the notes held and latched and by what, the keys down, the arpeggiators (their notes, their step,
 *   their draws), the three sequences (where they are, their takes), the grid and its clock, the LFOs (where in their
 *   turns), SPARKLE's next glint, the metronome, COUNT-IN, MORPH and ANCHOR under way, SWELL, LOFI TAPE II's speed,
 *   PULSE, RESTORE's notes, UNDO's sequences, the sound as saved (COMPARE), TUNE A4. The sound's (frasil_lv: dsp/), at
 *   half precision: the generators, the tide, each voice's drift, envelope, pressure and phases, the bass, the cloud's
 *   sways, PUMP's and the tape's phases, the notes queued for the next block. Not the buffers: the cloud's room, GLINT's
 *   echoes, the tape's loop, a filter's state (heard again within a moment: a listener's own, or from nothing).
 *   The engine alone's own beside it (frasil_engine.c's, its `alone` part: the tracks stopped, the fingers), for an
 *   engine alone's state; the FM-1's has none.
 * The bytes: 16 of a head ("FRlv", LIVE_V, the parts told, the scene's length, NPARAM, the millisecond the engine runs
 * next, its keys, the body's length), then the body, its fields in this file's order, little-endian, each byte of the
 * scenes it keeps (RESTORE's, ANCHOR's, the sound as saved, the sequences) XORed with the start's scene's (nearly all
 * nought), then nought bytes in runs: a token under 0x80 is that many plus one bytes as they are, from 0x80 that many
 * less 0x7F noughts. Loaded only by the engine it came from (LIVE_V, the layout: another's refused). */
#define LIVE_V 1u
#define LIVE_HEAD 16u
#define LIVE_F_DSP 1u                    /* the sound's part (frasil_lv) is in it */
#define LIVE_F_ALONE 2u                  /* the engine alone's own part */

typedef struct {
    br_live c;                           /* (first: the visitor handed to the DSP is this) */
    uint8_t *o;
    const uint8_t *in;
    uint32_t n, max;                     /* where in the body, and its room (saving) or its length (loading) */
    uint32_t zeros, lit_at, lit_n;       /* saving: noughts still to write, the run of bytes as they are open (its token's
                                          * place, its length so far: 0 none) */
    uint32_t z_left, l_left;             /* loading: noughts still to give, bytes as they are still to read */
    const uint8_t *ref;                  /* the start's scene (FRS_SCENE_BYTES), or none */
    int bad;
} rs_io;

static void rs_zeros(rs_io *s)
{
    while (s->zeros && !s->bad) {
        uint32_t k = s->zeros > 128u ? 128u : s->zeros;
        if (s->n >= s->max) {
            s->bad = 1;
            return;
        }
        s->o[s->n++] = (uint8_t)(0x7Fu + k);
        s->zeros -= k;
    }
}
static void rs_put(rs_io *s, uint8_t b)
{
    if (s->bad)
        return;
    if (!b) {
        s->zeros++;
        s->lit_n = 0;
        return;
    }
    rs_zeros(s);
    if (!s->lit_n || s->lit_n == 128u) {
        if (s->n >= s->max) {
            s->bad = 1;
            return;
        }
        s->lit_at = s->n++;
        s->lit_n = 0;
    }
    if (s->n >= s->max) {
        s->bad = 1;
        return;
    }
    s->o[s->n++] = b;
    s->o[s->lit_at] = (uint8_t)s->lit_n++;
}
static uint8_t rs_get(rs_io *s)
{
    while (!s->bad) {
        if (s->z_left) {
            s->z_left--;
            return 0;
        }
        if (s->l_left) {
            if (s->n >= s->max)
                break;
            s->l_left--;
            return s->in[s->n++];
        }
        if (s->n >= s->max)
            break;
        {
            uint8_t t = s->in[s->n++];
            if (t & 0x80u)
                s->z_left = t - 0x7Fu;
            else
                s->l_left = t + 1u;
        }
    }
    s->bad = 1;
    return 0;
}

static void rs_u8(br_live *c, uint8_t *x)
{
    rs_io *s = (rs_io *)c;
    if (c->save)
        rs_put(s, *x);
    else
        *x = rs_get(s);
}
static void rs_u32(br_live *c, uint32_t *x)
{
    uint32_t i, v = *x;
    uint8_t b[4];
    for (i = 0; i < 4u; i++) {
        b[i] = (uint8_t)(v >> (8u * i));
        rs_u8(c, &b[i]);
    }
    if (!c->save)
        *x = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}
static void rs_u16(br_live *c, uint16_t *x)
{
    uint8_t lo = (uint8_t)*x, hi = (uint8_t)(*x >> 8);
    rs_u8(c, &lo);
    rs_u8(c, &hi);
    if (!c->save)
        *x = (uint16_t)(lo | hi << 8);
}
static void rs_f32(br_live *c, float *x)
{
    uint32_t u;
    memcpy(&u, x, 4);
    rs_u32(c, &u);
    if (!c->save)
        memcpy(x, &u, 4);
}
/* IEEE half precision, rounded to the nearest (the DSP's fields: within a few thousandths of where they are) */
static uint16_t rs_half(float f)
{
    uint32_t x, s, m;
    int32_t e;
    memcpy(&x, &f, 4);
    s = x >> 16 & 0x8000u;
    m = x & 0x7FFFFFu;
    if ((x >> 23 & 0xFFu) == 0xFFu)
        return (uint16_t)(s | 0x7C00u | (m ? 0x200u : 0u));
    e = (int32_t)(x >> 23 & 0xFFu) - 112;
    if (e >= 31)
        return (uint16_t)(s | 0x7BFFu);  /* (the largest: never infinite) */
    if (e <= 0) {
        uint32_t sh;
        if (e < -10)
            return (uint16_t)s;
        m |= 0x800000u;
        sh = (uint32_t)(14 - e);
        return (uint16_t)(s | ((m >> sh) + (m >> (sh - 1u) & 1u)));
    }
    return (uint16_t)((s | (uint32_t)e << 10 | m >> 13) + (m >> 12 & 1u));
}
static float rs_float(uint16_t h)
{
    uint32_t s = (uint32_t)(h & 0x8000u) << 16, e = h >> 10 & 0x1Fu, m = h & 0x3FFu, x;
    float f;
    if (!e) {
        if (!m)
            x = s;
        else {
            uint32_t k = 0;
            while (!(m & 0x400u)) {
                m <<= 1;
                k++;
            }
            x = s | (113u - k) << 23 | (m & 0x3FFu) << 13;
        }
    } else if (e == 31u)
        x = s | 0x7F800000u | m << 13;
    else
        x = s | (e + 112u) << 23 | m << 13;
    memcpy(&f, &x, 4);
    return f;
}
static void rs_f16(br_live *c, float *x)
{
    uint16_t h = c->save ? rs_half(*x) : 0;
    rs_u16(c, &h);
    if (!c->save)
        *x = rs_float(h);
}

#define V8(x) rs_u8(&s->c, (uint8_t *)&(x))
#define V16(x) rs_u16(&s->c, (uint16_t *)&(x))
#define V32(x) rs_u32(&s->c, (uint32_t *)&(x))
#define VF(x) rs_f32(&s->c, &(x))
_Static_assert(sizeof(int) == 4u && sizeof(float) == 4u, "LIVE's fields: int and float are four bytes");

/* n bytes, each XORed with ref's (none: as they are) */
static void rs_bytes(rs_io *s, void *p, uint32_t n, const uint8_t *ref)
{
    uint8_t *b = p;
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint8_t x = (uint8_t)(b[i] ^ (ref ? ref[i] : 0u));
        V8(x);
        if (!s->c.save)
            b[i] = (uint8_t)(x ^ (ref ? ref[i] : 0u));
    }
}
#define VB(a) rs_bytes(s, (a), sizeof(a), 0)
#define VREF(a, field) rs_bytes(s, (a), sizeof(a), s->ref ? (const uint8_t *)&((const scene_t *)s->ref)->field : 0)
static void rs_u32s(rs_io *s, uint32_t *a, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        V32(a[i]);
}
static void rs_floats(rs_io *s, float *a, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        VF(a[i]);
}
#define VU32S(a) rs_u32s(s, (uint32_t *)(a), sizeof(a) / 4u)
#define VFS(a) rs_floats(s, (a), sizeof(a) / 4u)

static void rs_chord(rs_io *s, chord_t *ch)
{
    uint8_t k = ch->k, i;
    V8(k);
    if (k > MAXCHORD) {
        s->bad = 1;
        return;
    }
    if (!s->c.save) {
        memset(ch, 0, sizeof *ch);
        ch->k = k;
    }
    for (i = 0; i < k; i++)
        V8(ch->n[i]);
}
static void rs_chords(rs_io *s, chord_t *a, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n && !s->bad; i++)
        rs_chord(s, &a[i]);
}

static void rs_scene(rs_io *s, scene_t *sc) { rs_bytes(s, sc, sizeof *sc, s->ref); }

static void rs_arp(rs_io *s, arp_t *a)
{
    uint32_t i;
    V8(a->n);
    V8(a->pn);
    if (a->n > 16u || a->pn > 16u) {
        s->bad = 1;
        return;
    }
    for (i = 0; i < 16u; i++)
        if (i < a->n) {
            V8(a->held[i]);
            V16(a->in[i]);
            VF(a->prs[i]);
        } else if (!s->c.save) {
            a->held[i] = 0;
            a->in[i] = 0;
            a->prs[i] = 0.0f;
        }
    for (i = 0; i < 16u; i++)
        if (i < a->pn) {
            V8(a->play[i]);
            VF(a->pprs[i]);
        } else if (!s->c.save) {
            a->play[i] = 0;
            a->pprs[i] = 0.0f;
        }
    V16(a->ins);
    V8(a->dirty);
    V32(a->step_n);
    V8(a->src);
    V8(a->vel);
    V32(a->idx);
    V32(a->dir);
    V16(a->sounding);
    V32(a->off_ms);
    V32(a->steps);
    V8(a->latch);
    V8(a->paused);
    V32(a->rng);
    V32(a->chg_ms);
    V32(a->hand_ms);
    V32(a->b_ms);
    V32(a->b_step);
    V16(a->b_late);
    V8(a->fresh);
    V8(a->gap);
    V8(a->owe);
}

static void rs_take(rs_io *s, take_t *t)
{
    V8(t->on);
    V8(t->left);
    V32(t->wrote);
}

/* the brain, field by field */
static void rs_brain(rs_io *s)
{
    uint32_t i;
    uint8_t nk = FRS_NKEYS;
    /* the settings, the sound's XORed with the scene's */
    rs_bytes(s, st.v, NSOUND, s->ref ? ((const scene_t *)s->ref)->v : 0);
    rs_bytes(s, st.v + NSOUND, NPARAM - NSOUND, 0);
    V8(st.glint);
    V8(st.bass);
    V8(st.dirty);
    V8(st.sustain);
    VF(st.swell_btn);
    VF(st.swell_midi);
    VF(st.swell_sent);
    V8(st.kb_rec);
    V8(st.rec_took);
    V8(st.loading_morph);
    V8(st.compare);
    V8(st.metro);
    V32(st.clicks);
    V32(st.hiclicks);
    /* the notes */
    VB(held_by);
    rs_chords(s, lat, 128u);
    rs_chords(s, mid, 128u);
    VU32S(lit);
    VU32S(mid_on);
    VU32S(sus);
    V8(bass_note);
    VU32S(mgl);
    VU32S(mgs);
    VB(mgl_base);
    VB(mbass);
    V32(mb_n);
    /* the keys down (the FM-1's 27, an engine alone's 32: as many as both have) */
    V8(nk);
    if (!s->c.save && nk > FRS_NKEYS)
        s->bad = 1;
    for (i = 0; i < FRS_NKEYS && !s->bad; i++) {
        uint8_t kind = i < nk ? key[i].kind : 0u;
        if (i < nk)
            V8(kind);
        if (!kind) {
            if (!s->c.save)
                memset(&key[i], 0, sizeof key[i]);
            continue;
        }
        key[i].kind = kind;
        V8(key[i].root);
        V8(key[i].gbase);
        V8(key[i].gsound);
        V8(key[i].hold);
        rs_chord(s, &key[i].chord);
        V8(key[i].was_lit);
        V8(key[i].pressed_in);
        V8(key[i].prs);
        V8(key[i].drec);
        V32(key[i].t);
    }
    /* the arpeggiators, the ring, THE KEYS' HOLD */
    rs_arp(s, &arp);
    rs_arp(s, &barp);
    VB(rung.note);
    V8(rung.n);
    VU32S(hold_k);
    VU32S(hold_a);
    VU32S(hold_ba);
#ifdef FRASIL_SHELL                      /* (THE KEYS' HOLD: the fingers' alone, none on the FM-1: its 640 bytes never read
                                          * there, so the compiler keeps none) */
    for (i = 0; i < 128u; i++) {
        uint8_t k0 = 0;
        V8(k0);
    }
#else
    rs_chords(s, hold_c, 128u);
#endif
    V8(hold_b);
    V8(hold_bn);
    V8(metro_free);
    V8(keys_touch);
    /* the grid, TAPE SPEED */
    VF(grid.tick);
    V32(grid.wraps);
    V32(grid.last_ms);
    V32(grid.clk_new);
    V32(grid.clk);
    V32(grid.clk_at);
    VF(grid.clk_ms);
    VF(grid.clk_due);                    /* (#123's clock: when the next is due, its clocks, the beat measured, how far) */
    V32(grid.clk_n);
    V32(grid.clk_beat);
    V32(grid.clk_far);
    VF(tape_l2);
    VF(tape_sent);
    VF(tape_tk);
    V32(tape_at);
    V8(tape_clocked);
    V8(tape_fresh);
    /* GLINT's sequence, the bass's, the drone's */
    VREF(seq, seq);
    V8(sq.run);
    V8(sq.restart);
    V8(sq.off);
    V8(sq.at_trans);
    V8(sq.len);
    VF(sq.at);
    V32(sq.start);
    V32(sq.n);
    V32(sq.pos);
    V32(sq.skip);
    V8(sq.near);
    V16(sq.sounding);
    V32(sq.off_ms);
    V32(sq.steps);
    VREF(bnote, bnote);
    V8(bq.run);
    V8(bq.len);
    V8(bq.wait);
    V8(bq.pos);
    VF(bq.t);
    VF(bq.g);
    V8(bq.tn);
    V8(bq.ta);
    V8(bq.tc);
    VF(bq.tp);
    VF(bq.te);
    V8(bq.skip);
    V8(bq.sounding);
    V8(bq.acc);
    V32(bq.steps);
    VREF(dnote, dnote);
    V8(dq.run);
    V8(dq.wait);
    V8(dq.pos);
    VF(dq.t);
    VF(dq.g);
    VB(dq.snd);
    V8(dq.nsnd);
    V8(dq.keys);
    V8(dq.adds);
    V32(dq.turn);
    V8(dq.len);
    V32(dq.rng);
    V32(dq.steps);
    VU32S(drec_f);
    VU32S(dt_lat);
    V8(overdub);
    V8(step_hand);
    V8(step_stays);
    V8(rec_was.on);
    V8(rec_was.layer);
    V8(rec_was.plays);
    V8(rec_was.wrote);
    V8(cnt.run);
    V8(cnt.beat);
    V8(cnt.glint);
    VF(cnt.t);
    V32(cnt.at);
    rs_take(s, &gtk);
    rs_take(s, &btk);
    rs_take(s, &dtk);
    V8(dh.q);
    V32(dh.turn);
    VB(dh.was);
    V32(dh.wrote);
    VREF(seq_prev, seq);
    V8(seq_had);
    VB(undone);
    rs_bytes(s, bq_prev, SC_BSEQ, s->ref ? ((const scene_t *)s->ref)->bnote : 0);
    V8(bq_prev[SC_BSEQ]);
    V8(bq_had);
    VREF(dq_prev, dnote);
    V8(dq_prev_len);
    V8(dq_had);
    VB(dq_memo);
    V8(dq_memo_at);
    V8(dq_view_n);
    V8(dq_view_note);
    /* the engine's own: the delay's time, the sound as saved (COMPARE, what changed), TUNE A4, PULSE, the drone's own
     * preset */
    VF(delay_s_now);
    V8(saved_bsp);
    V8(saved_gv);
    VREF(saved_tp, tp_flutter);
    VREF(saved_amt, lfo_amt);
    V8(saved_sw);
    VREF(saved_v, v);
    VREF(saved_seq, seq);
    VREF(saved_bnote, bnote);
    VREF(saved_bq, bq_on);
    VREF(saved_dnote, dnote);
    VREF(saved_dq, dq_on);
    VF(a4_l2);
    VF(bp.g);
    VF(bp.ng);
    VF(bp.at);
    V8(bp.fresh);
    V32(bp.n);
    VB(custom);
    V32(seq_shift);
    V32(bseq_shift);
    /* MORPH, ANCHOR, the LFOs' offsets, RESTORE */
    V8(mo.on);
    V8(mo.anc);
    V32(mo.t0);
    V32(mo.dur);
    VFS(mo.d);
    VB(mo.name);
    VFS(morph_off);
    VFS(mod);                            /* (not mod_tp, LOFI TAPE II's four: lfo_run's next writes them, within 10 ms) */
    V32(morph_at);
    V8(anc.on);
    V8(anc.due);
    V8(anc.seqs);
    V32(anc.half);
    VU32S(anc.pend);
    if (anc.on)                          /* (its scene only while it is set; RESTORE's while it has one) */
        rs_scene(s, &anc.sc);
    else if (!s->c.save)
        memset(&anc.sc, 0, sizeof anc.sc);
    V32(aud_root);
    V8(rst_valid);
    if (rst_valid)
        rs_scene(s, &rst);
    else if (!s->c.save)
        memset(&rst, 0, sizeof rst);
    /* the LFOs, the metronome, SPARKLE, SWELL, COUNT-IN's, THE KEYS edited */
    for (i = 0; i < NLFO; i++) {
        VF(lfo[i].ph);
        V32(lfo[i].cyc);
        VF(lfo[i].out);
        VF(lfo[i].peak);
        V8(lfo[i].on);
    }
    V32(lfo_at);
    VF(lfo_grid);
    V8(lfo_any);
    V8(lfo_restart);
    VF(mtr.at);
    VF(mtr.t);
    VF(mtr.g);
    V8(mtr.src);
    VF(mtr.ck);
    V32(metro_beat);
    V32(metro_n);
    V8(spk.armed);
    V32(spk.next);
    VB(spk.note);
    VU32S(spk.off);
    V32(spk.n);
    V32(swell_at);
    V8(rec_counts);
    V8(keys_edited);
}

/* the running state into out (max bytes at most): its length, or 0 (it would not fit). ref: the start's scene it goes
 * with (its bytes, as the start has them), or 0; held: ENV held now (the runner's: SWELL rising); alone: the engine
 * alone's own part (frasil_engine.c), or 0; dsp: the sound's too. On the FM-1 the sound's is read between two of its
 * blocks (frasil_renders before and after: a block rendered meanwhile, read again; three times at most, else left
 * out) */
static uint32_t live_save(uint8_t *out, uint32_t max, const uint8_t *ref, int held, void (*alone)(br_live *), int dsp)
{
    rs_io s;
    uint32_t i, ms = eng_ms();
    uint8_t flags = 0, h = (uint8_t)(held != 0);
    if (max < LIVE_HEAD + 1u)
        return 0;
#ifdef FRASIL_SHELL
    ms += 1u;                            /* (the FM-1's main loop has run this millisecond's: its next one) */
#endif
    memset(&s, 0, sizeof s);
    s.c.save = 1;
    s.c.u8 = rs_u8;
    s.c.u32 = rs_u32;
    s.c.f32 = rs_f32;
    s.c.f16 = rs_f16;
    s.o = out + LIVE_HEAD;
    s.max = max - LIVE_HEAD;
    s.ref = ref;
    rs_brain(&s);
    rs_u8(&s.c, &h);
    if (dsp && !s.bad) {
        rs_io was = s;
        for (i = 0; i < 3u; i++) {
            uint32_t r = frasil_renders();
            frasil_lv(&s.c);
            if (frasil_renders() == r || s.bad)
                break;
            s = was;
        }
        if (i < 3u && !s.bad)
            flags |= LIVE_F_DSP;
        else
            s = was;
    }
    if (alone && !s.bad) {
        alone(&s.c);
        flags |= LIVE_F_ALONE;
    }
    rs_zeros(&s);
    if (s.bad)
        return 0;
    out[0] = 'F', out[1] = 'R', out[2] = 'l', out[3] = 'v';
    out[4] = LIVE_V;
    out[5] = flags;
    out[6] = (uint8_t)(sizeof(scene_t) & 0xFFu);
    out[7] = (uint8_t)(sizeof(scene_t) >> 8);
    out[8] = (uint8_t)(NPARAM & 0xFFu);
    out[9] = (uint8_t)(NPARAM >> 8);
    for (i = 0; i < 4u; i++)
        out[10u + i] = (uint8_t)(ms >> (8u * i));
    out[14] = (uint8_t)(s.n & 0xFFu);
    out[15] = (uint8_t)(s.n >> 8);
    return LIVE_HEAD + s.n;
}

#ifndef FRASIL_SHELL                     /* (an engine alone's: the FM-1 only tells its own) */
/* the head's millisecond (the one the engine runs next), or 0: not a state this engine reads */
static int live_head(const uint8_t *in, uint32_t n, uint32_t *ms, uint8_t *flags)
{
    if (n < LIVE_HEAD || in[0] != 'F' || in[1] != 'R' || in[2] != 'l' || in[3] != 'v' || in[4] != LIVE_V ||
        (in[6] | in[7] << 8) != sizeof(scene_t) || (in[8] | in[9] << 8) != NPARAM ||
        LIVE_HEAD + (uint32_t)(in[14] | in[15] << 8) != n)
        return 0;
    *ms = (uint32_t)in[10] | (uint32_t)in[11] << 8 | (uint32_t)in[12] << 16 | (uint32_t)in[13] << 24;
    *flags = in[5];
    return 1;
}

/* a running state into this engine (live_save's, from the same start's scene ref): its flags (LIVE_F_*), or -1 (not
 * this engine's, or cut short: then what it had read is in, the caller opens again). The DSP's part as frasil_lv loads
 * it; the sound's settings pushed, heard where they are (no glide); *held ENV held; alone the engine alone's own part
 * (0, or none in it: the caller's own) */
static int live_load(const uint8_t *in, uint32_t n, const uint8_t *ref, uint8_t *held, void (*alone)(br_live *))
{
    rs_io s;
    uint32_t ms;
    uint8_t flags, h = 0;
    if (!live_head(in, n, &ms, &flags))
        return -1;
    memset(&s, 0, sizeof s);
    s.c.save = 0;
    s.c.u8 = rs_u8;
    s.c.u32 = rs_u32;
    s.c.f32 = rs_f32;
    s.c.f16 = rs_f16;
    s.in = in + LIVE_HEAD;
    s.max = n - LIVE_HEAD;
    s.ref = ref;
    rs_brain(&s);
    rs_u8(&s.c, &h);
    if (!s.bad && (flags & LIVE_F_DSP))
        frasil_lv(&s.c);
    if (!s.bad && (flags & LIVE_F_ALONE) && alone)
        alone(&s.c);
    if (!s.bad && (!(flags & LIVE_F_ALONE) || alone) && (s.n != s.max || s.z_left || s.l_left))
        s.bad = 1;                       /* (bytes left over: another engine's fields, read wrong) */
    if (s.bad)
        return -1;
    *held = h;
    engine_push();
    frasil_live_done();                  /* (the sound's settings where they are: no glide to them) */
    return flags;
}
#endif
