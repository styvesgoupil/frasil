/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* SHARD STREAM: see frasil_stream.h */
#include "frasil_stream.h"
#include <stdlib.h>
#include <string.h>

#define MAGIC "FSTR"
#define JMAGIC "FSTJ"
#define HEAD_BYTES 39u

/* ------------------------------------------------------------------ bytes --- */
static void w16(uint8_t *p, uint32_t x) { p[0] = (uint8_t)x; p[1] = (uint8_t)(x >> 8); }
static void w32(uint8_t *p, uint32_t x) { w16(p, x & 0xffffu); w16(p + 2, x >> 16); }
static void w64(uint8_t *p, uint64_t x) { w32(p, (uint32_t)x); w32(p + 4, (uint32_t)(x >> 32)); }
static uint32_t r16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t r32(const uint8_t *p) { return r16(p) | r16(p + 2) << 16; }
static uint64_t r64(const uint8_t *p) { return (uint64_t)r32(p) | (uint64_t)r32(p + 4) << 32; }

static uint32_t varint_put(uint8_t *o, uint32_t x)
{
    uint32_t n = 0;
    while (x >= 0x80u) {
        o[n++] = (uint8_t)(x | 0x80u);
        x >>= 7;
    }
    o[n++] = (uint8_t)x;
    return n;
}

/* its bytes read, or 0: runs past n, or too long */
static uint32_t varint_get(const uint8_t *b, size_t n, uint32_t *x)
{
    uint32_t v = 0, i;
    for (i = 0; i < 5u && i < n; i++) {
        v |= (uint32_t)(b[i] & 0x7fu) << (7u * i);
        if (!(b[i] & 0x80u)) {
            *x = v;
            return i + 1u;
        }
    }
    return 0;
}

uint32_t fst_crc(const uint8_t *b, size_t n)
{
    uint32_t c = 0xffffffffu;
    size_t i;
    for (i = 0; i < n; i++) {
        int k;
        c ^= b[i];
        for (k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}

/* an op's args: their length from its top bits. -1: runs past n (a length byte missing); *skip: the bytes before them */
static int op_len(uint8_t op, const uint8_t *b, size_t n, uint32_t *skip)
{
    *skip = 0;
    switch (op >> 6) {
    case 0: return 0;
    case 1: return 2;
    case 2: return 4;
    default:
        if (n < 1u)
            return -1;
        *skip = 1;
        return b[0];
    }
}

/* ------------------------------------------------------------------ the start --- */
_Static_assert(sizeof(fst_ev) == 20u, "a ring's slot, 20 bytes");
_Static_assert(FRS_SCENE_BYTES == 619u, "FST_SCENE_LAYOUT names this size");

/* the older layouts a stream may have, each its scenes' bytes (made over as they are played; not "FRS|": the same bytes
 * as FRS}'s, its AMOUNTs another scale everywhere in the stream) */
static const struct {
    const char *name;
    uint16_t bytes;
} OLD_LAYOUT[] = {{"FRS{", 590u}, {"FRSz", 586u}};
#define SC_SWELL (FRS_SCENE_BYTES - 1u)  /* (SWELL's byte, the scene's last: params.c scene_t) */

/* a scene of sbytes (FRS_SCENE_BYTES, or an older layout's: made over, SWELL at swell) copied into o, FRS_SCENE_BYTES
 * long */
static void scene_copy(uint8_t *o, const uint8_t *s, uint32_t sbytes, uint8_t swell)
{
    memcpy(o, s, sbytes);
    if (sbytes != FRS_SCENE_BYTES) {
        frs_scene_over(o, sbytes);
        o[SC_SWELL] = swell;
    }
}

void fst_capture(fst_start *s, const uint8_t *from)
{
    frs_view_t v;
    uint32_t p, n = frs_param_count();
    frs_view(&v);
    s->tracks = v.playing;
    frs_scene_store(s->scene, from);
    s->state_n = frs_state_save(s->state);
    s->moves_n = 0;
    for (p = 0; p < n && p < 256u && s->moves_n < FST_MOVES; p++)
        if (frs_param_kind(p) == FRS_P_MOMENT) {
            s->moves[2u * s->moves_n] = (uint8_t)p;
            s->moves[2u * s->moves_n + 1u] = (uint8_t)frs_param_get(p);
            s->moves_n++;
        }
}

int fst_fresh(const fst_start *s, float fs)
{
    uint32_t i;
    float l, r;
    frs_close();
    if (frs_init(fs))
        return -1;
    frs_keys_touch(1);
    frs_step_stays(1);
    if (s->state_n)                      /* (a state from before ARP SNAP has none: OFF, as its engine played; a later one's
                                          * sets it. No state, a shard alone on air (shard_stream): the engine's own) */
        frs_param_set(P_ARP_SNAP, 0);
    frs_state_load(s->state, s->state_n);
    frs_scene_take(s->scene);
    for (i = 0; i < s->moves_n; i++)
        frs_param_set(s->moves[2u * i], s->moves[2u * i + 1u]);
    if (s->tracks == 0u)
        frs_play_all(0);
    else
        for (i = 0; i < 3u; i++)
            if (!(s->tracks >> i & 1u))
                frs_track_play(i, 0);
    frs_render(&l, &r, 1);               /* (the first block: from here each block has a millisecond of its own) */
    return 0;
}

/* the engine's frs_midi_at, its first millisecond at or after the frame */
uint8_t fst_midi_ahead(uint64_t frames, uint32_t offset)
{
    double x = (double)(frames + offset) * 1000.0 / (double)frs_rate();
    uint64_t at = (uint64_t)x, now = frs_ms();
    at = (double)at < x ? at + 1u : at;
    return at <= now ? 0u : at - now > 255u ? 255u : (uint8_t)(at - now);
}

/* ------------------------------------------------------------------ the journal --- */
static uint32_t start_put(const fst_start *s, uint8_t *o)
{
    uint32_t n = 0;
    o[n++] = s->tracks;
    o[n++] = (uint8_t)s->state_n;
    o[n++] = s->moves_n;
    memcpy(o + n, s->scene, FRS_SCENE_BYTES);
    n += FRS_SCENE_BYTES;
    memcpy(o + n, s->state, s->state_n);
    n += s->state_n;
    memcpy(o + n, s->moves, 2u * s->moves_n);
    return n + 2u * s->moves_n;
}

/* its bytes, or 0: runs past n */
static uint32_t start_get(const uint8_t *b, size_t n, fst_start *s)
{
    uint32_t need;
    if (n < 3u + FRS_SCENE_BYTES)
        return 0;
    if (b[1] > FRS_STATE_BYTES || b[2] > FST_MOVES)
        return 0;
    need = 3u + FRS_SCENE_BYTES + b[1] + 2u * (uint32_t)b[2];
    if (n < need)
        return 0;
    s->tracks = b[0];
    s->state_n = b[1];
    s->moves_n = b[2];
    memcpy(s->scene, b + 3, FRS_SCENE_BYTES);
    memcpy(s->state, b + 3 + FRS_SCENE_BYTES, s->state_n);
    memcpy(s->moves, b + 3 + FRS_SCENE_BYTES + s->state_n, 2u * s->moves_n);
    return need;
}

uint32_t fst_jhead(const fst_start *s, uint32_t rate, int64_t created, uint8_t *o)
{
    uint32_t n = 0;
    memcpy(o, JMAGIC, 4);
    n = 4;
    o[n++] = FST_VERSION;
    o[n++] = 0;
    w16(o + n, FRS_SCENE_BYTES);
    n += 2;
    w32(o + n, rate);
    n += 4;
    w64(o + n, (uint64_t)created);
    n += 8;
    memset(o + n, 0, 8);
    {   /* (what made it) */
        const char *v = frs_version();
        size_t l = strlen(v);
        memcpy(o + n, v, l > 8u ? 8u : l);
    }
    n += 8;
    w16(o + n, frs_param_count());
    n += 2;
    return n + start_put(s, o + n);
}

uint32_t fst_jevent(const fst_ev *e, uint32_t *last, const void *scene, uint32_t lost, uint8_t *o)
{
    uint32_t n = varint_put(o, e->ms - *last);
    *last = e->ms;
    o[n++] = e->op;
    switch (e->op) {
    case FSTJ_SCENE:
        o[n++] = e->a[0];
        o[n++] = e->a[2];
        memcpy(o + n, scene, FRS_SCENE_BYTES);
        return n + FRS_SCENE_BYTES;
    case FSTJ_OPEN:
        w32(o + n, r32(e->a));
        n += 4;
        return n + start_put((const fst_start *)scene, o + n);
    case FSTJ_END:
        w32(o + n, lost);
        return n + 4u;
    default:
        if (e->op >> 6 == 3u)
            o[n++] = e->n;
        memcpy(o + n, e->a, e->n);
        return n + e->n;
    }
}

/* the scenes a stream names, none twice */
typedef struct { uint8_t *b; uint32_t n, cap; } scenes_t;
static int scene_index(scenes_t *t, const uint8_t *s)
{
    uint32_t i;
    for (i = 0; i < t->n; i++)
        if (!memcmp(t->b + i * FRS_SCENE_BYTES, s, FRS_SCENE_BYTES))
            return (int)i;
    if (t->n == t->cap) {
        uint32_t cap = t->cap ? 2u * t->cap : 8u;
        uint8_t *b = realloc(t->b, (size_t)cap * FRS_SCENE_BYTES);
        if (!b)
            return -1;
        t->b = b;
        t->cap = cap;
    }
    memcpy(t->b + (size_t)t->n * FRS_SCENE_BYTES, s, FRS_SCENE_BYTES);
    return (int)t->n++;
}

static uint8_t *chunk(uint8_t *o, const char *tag, const void *b, uint32_t n)
{
    memcpy(o, tag, 4);
    w32(o + 4, n);
    if (n)
        memcpy(o + 8, b, n);
    return o + 8 + n;
}

uint8_t *fst_finish(const uint8_t *j, size_t n, size_t *out_n)
{
    fst_start st;
    scenes_t sc = {0, 0, 0};
    uint8_t *ev, *out = 0, *o, head[HEAD_BYTES], dens[FST_DENS], flags = FST_F_FRESH | FST_F_HANDS;
    uint32_t *times = 0;
    uint32_t rate, nparam, used, t = 0, nev = 0, nev_cap, lost = 0, total = 0, cap;
    size_t at = 36, evn = 0;
    int64_t created;
    int ended = 0;

    *out_n = 0;
    if (n < at || memcmp(j, JMAGIC, 4) || j[4] != FST_VERSION || r16(j + 6) != FRS_SCENE_BYTES)
        return 0;
    rate = r32(j + 8);
    created = (int64_t)r64(j + 12);
    nparam = r16(j + 28);
    at = 30;
    used = start_get(j + at, n - at, &st);
    if (!used)
        return 0;
    at += used;
    /* an event in the file takes at most the journal's bytes for it, or 8 more when it refers to a scene */
    ev = malloc(n + 64u);
    nev_cap = 4096;
    times = malloc(nev_cap * sizeof *times);
    if (!ev || !times)
        goto done;
    while (at < n && !ended) {
        uint32_t dt, l, skip;
        uint8_t op;
        size_t p = at;
        int len;
        l = varint_get(j + p, n - p, &dt);
        if (!l || p + l >= n)
            break;
        p += l;
        op = j[p++];
        if (op == FSTJ_END) {
            if (n - p < 4u)
                break;
            lost = r32(j + p);
            total = t + dt;
            ended = 1;
            at = p + 4u;
            break;
        }
        if (nev == nev_cap) {
            uint32_t *w = realloc(times, 2u * (size_t)nev_cap * sizeof *times);
            if (!w)
                goto done;
            times = w;
            nev_cap *= 2u;
        }
        if (op == FSTJ_SCENE) {
            uint8_t real, sl;
            int i;
            if (n - p < 2u + FRS_SCENE_BYTES)
                break;
            real = j[p];
            sl = j[p + 1];
            p += 2;
            if (real == FST_STATE) {     /* (a state: its bytes inline, not a scene) */
                if (sl > FRS_STATE_BYTES)
                    break;
                evn += varint_put(ev + evn, dt);
                ev[evn++] = FST_STATE;
                ev[evn++] = sl;
                memcpy(ev + evn, j + p, sl);
                evn += sl;
            } else {
                i = scene_index(&sc, j + p);
                if (i < 0)
                    goto done;
                evn += varint_put(ev + evn, dt);
                ev[evn++] = real;
                w16(ev + evn, (uint32_t)i);
                evn += 2;
                if (real == FST_STORE) {
                    ev[evn++] = 0;
                    ev[evn++] = 0;
                }
            }
            p += FRS_SCENE_BYTES;
        } else if (op == FSTJ_OPEN) {
            fst_start s2;
            uint32_t r2;
            int i;
            if (n - p < 4u)
                break;
            r2 = r32(j + p);
            l = start_get(j + p + 4, n - p - 4, &s2);
            if (!l)
                break;
            p += 4u + l;
            i = scene_index(&sc, s2.scene);
            if (i < 0)
                goto done;
            if (r2 != rate)
                flags |= FST_F_RATE;
            evn += varint_put(ev + evn, dt);
            ev[evn++] = FST_OPEN;
            ev[evn++] = (uint8_t)(9u + s2.state_n + 2u * s2.moves_n);
            w16(ev + evn, (uint32_t)i);
            evn += 2;
            ev[evn++] = s2.tracks;
            w32(ev + evn, r2);
            evn += 4;
            ev[evn++] = (uint8_t)s2.state_n;
            memcpy(ev + evn, s2.state, s2.state_n);
            evn += s2.state_n;
            ev[evn++] = s2.moves_n;
            memcpy(ev + evn, s2.moves, 2u * s2.moves_n);
            evn += 2u * s2.moves_n;
        } else {
            if (op == 0u || op >= 0xE0u)     /* (nothing a journal writes: what follows isn't one) */
                break;
            len = op_len(op, j + p, n - p, &skip);
            if (len < 0 || n - p < skip + (uint32_t)len)
                break;
            if (op == FST_MIDI)
                flags &= (uint8_t)~FST_F_HANDS;
            evn += varint_put(ev + evn, dt);
            ev[evn++] = op;
            memcpy(ev + evn, j + p, skip + (uint32_t)len);
            evn += skip + (uint32_t)len;
            p += skip + (uint32_t)len;
        }
        t += dt;
        times[nev++] = t;
        at = p;
    }
    if (!ended)
        total = t;                       /* (a journal left by a crash: up to its last whole event) */
    if (!nev)
        goto done;
    memset(dens, 0, sizeof dens);
    {
        uint32_t count[FST_DENS], most = 1, i;
        memset(count, 0, sizeof count);
        for (i = 0; i < nev; i++) {
            uint64_t k = (uint64_t)times[i] * FST_DENS / ((uint64_t)total + 1u);
            count[k < FST_DENS ? k : FST_DENS - 1u]++;   /* (a damaged journal's times can pass its end) */
        }
        for (i = 0; i < FST_DENS; i++)
            most = count[i] > most ? count[i] : most;
        for (i = 0; i < FST_DENS; i++) {   /* (a square root: a few touches still show beside a sweep) */
            uint32_t x = count[i] * 65025u / most, r = 0;
            while ((r + 1u) * (r + 1u) <= x)
                r++;
            dens[i] = (uint8_t)r;
        }
    }
    memset(head, 0, sizeof head);
    w32(head, rate);
    w32(head + 4, total);
    w32(head + 8, nev);
    memcpy(head + 12, j + 20, 8);
    memcpy(head + 20, FST_SCENE_LAYOUT, 4);
    w16(head + 24, nparam);
    head[26] = st.tracks;
    w32(head + 27, lost);
    w64(head + 31, (uint64_t)created);
    cap = 8u + 8u * 8u + HEAD_BYTES + FRS_SCENE_BYTES + st.state_n + 2u * st.moves_n + FST_DENS + 2u +
          sc.n * FRS_SCENE_BYTES + (uint32_t)evn + 4u;
    out = malloc(cap);
    if (!out)
        goto done;
    memcpy(out, MAGIC, 4);
    out[4] = FST_VERSION;
    out[5] = flags;
    out[6] = out[7] = 0;
    o = chunk(out + 8, "HEAD", head, HEAD_BYTES);
    o = chunk(o, "SCN0", st.scene, FRS_SCENE_BYTES);
    o = chunk(o, "STAT", st.state, st.state_n);
    o = chunk(o, "MOVE", st.moves, 2u * st.moves_n);
    o = chunk(o, "DENS", dens, FST_DENS);
    memcpy(o, "SCNS", 4);
    w32(o + 4, 2u + sc.n * FRS_SCENE_BYTES);
    w16(o + 8, sc.n);
    if (sc.n)
        memcpy(o + 10, sc.b, (size_t)sc.n * FRS_SCENE_BYTES);
    o += 10u + sc.n * FRS_SCENE_BYTES;
    o = chunk(o, "EVTS", ev, (uint32_t)evn);
    {
        uint8_t crc[4];
        w32(crc, fst_crc(out, (size_t)(o - out)));
        o = chunk(o, "CRC_", crc, 4);
    }
    *out_n = (size_t)(o - out);
done:
    free(ev);
    free(times);
    free(sc.b);
    return out;
}

/* ------------------------------------------------------------------ reading --- */
/* every event walked: its bytes within, the count HEAD says, the scenes it names there */
static int walk(const fst_file *f)
{
    size_t at = 0, n = f->evts_n;
    const uint8_t *b = f->evts;
    uint32_t count = 0, work = 0;
    uint64_t t = 0, window = 0;
    while (at < n) {
        uint32_t dt, skip, l = varint_get(b + at, n - at, &dt);
        uint8_t op;
        int len;
        if (!l || at + l >= n)
            return FST_BAD;
        at += l;
        op = b[at++];
        len = op_len(op, b + at, n - at, &skip);
        if (len < 0 || n - at < skip + (uint32_t)len)
            return FST_BAD;
        if (op == FST_SCENE || op == FST_SCENE_NOW || op == FST_STORE || op == FST_ANCHOR) {
            if (r16(b + at) >= f->nscns)
                return FST_BAD;
        } else if (op == FST_DSTEP) {
            if (len < 1 || len > 9)
                return FST_BAD;
        } else if (op == FST_STATE) {
            if (len > (int)FRS_STATE_BYTES)
                return FST_BAD;
        } else if (op == FST_OPEN) {
            uint32_t sn, mn;
            if (len < 9 || r16(b + at + 1) >= f->nscns)
                return FST_BAD;
            sn = b[at + 1 + 7];
            if (sn > FRS_STATE_BYTES || (uint32_t)len < 9u + sn)
                return FST_BAD;
            mn = b[at + 1 + 8 + sn];
            if (mn > FST_MOVES || (uint32_t)len != 9u + sn + 2u * mn)
                return FST_BAD;
        }
        at += skip + (uint32_t)len;
        t += dt;
        count++;
        if (t > (uint64_t)f->ms + 1000u)
            return FST_BAD;
        if (t >= window + 4u) {          /* (a block's worth of time, about) */
            window = t;
            work = 0;
        }
        work += op == FST_OPEN ? 512u
                : op == FST_SCENE || op == FST_SCENE_NOW || op == FST_STORE || op == FST_PANIC || op == FST_CUT || op == FST_CUT_STOP || op == FST_STATE ||
                  op == FST_ANCHOR || op == FST_ANCHOR_BACK ? 16u : 1u;
        if (work > FST_BUDGET)           /* (the audio thread would stall on it) */
            return FST_BAD;
    }
    return count == f->events ? FST_OK : FST_BAD;
}

int fst_read(const uint8_t *b, size_t n, fst_file *f)
{
    size_t at = 8;
    int have = 0, r;
    uint32_t scn0_l = 0, scns_l = 0, i;
    memset(f, 0, sizeof *f);
    if (n < 8u || memcmp(b, MAGIC, 4))
        return FST_BAD;
    if (b[4] != FST_VERSION)
        return FST_VERSIONED;            /* (a newer one: its chunks aren't this reader's to judge) */
    f->flags = b[5];
    while (at + 8u <= n) {
        const char *tag = (const char *)(b + at);
        uint32_t l = r32(b + at + 4);
        const uint8_t *p = b + at + 8;
        if (l > n - at - 8u)
            return FST_BAD;
        if (!memcmp(tag, "HEAD", 4)) {
            if (l < HEAD_BYTES)
                return FST_BAD;
            f->rate = r32(p);
            f->ms = r32(p + 4);
            f->events = r32(p + 8);
            memcpy(f->engine, p + 12, 8);
            memcpy(f->layout, p + 20, 4);
            f->nparam = (uint16_t)r16(p + 24);
            f->tracks = p[26];
            f->lost = r32(p + 27);
            f->created = (int64_t)r64(p + 31);
            have |= 1;
        } else if (!memcmp(tag, "SCN0", 4)) {
            f->scene = p;
            have |= 2;
            scn0_l = l;
        } else if (!memcmp(tag, "STAT", 4)) {
            if (l > FRS_STATE_BYTES)
                return FST_BAD;
            f->state = p;
            f->state_n = l;
        } else if (!memcmp(tag, "MOVE", 4)) {
            if (l & 1u || l > 2u * FST_MOVES)
                return FST_BAD;
            f->moves = p;
            f->moves_n = l / 2u;
        } else if (!memcmp(tag, "DENS", 4)) {
            if (l >= FST_DENS)
                f->dens = p;
        } else if (!memcmp(tag, "SCNS", 4)) {
            if (l < 2u)
                return FST_BAD;
            f->nscns = r16(p);
            f->scns = p + 2;
            scns_l = l - 2u;
            have |= 128;
        } else if (!memcmp(tag, "EVTS", 4)) {
            f->evts = p;
            f->evts_n = l;
            have |= 4;
        } else if (!memcmp(tag, "CRC_", 4)) {
            if (l != 4u)
                return FST_BAD;
            if (r32(p) != fst_crc(b, at))
                return FST_CRC;
            have |= 8;
        }
        at += 8u + (size_t)l;
    }
    if ((have & 15) != 15 || at != n)
        return FST_BAD;
    if (!memcmp(f->layout, FST_SCENE_LAYOUT, 4))
        f->sbytes = FRS_SCENE_BYTES;
    for (i = 0; !f->sbytes && i < sizeof OLD_LAYOUT / sizeof OLD_LAYOUT[0]; i++)
        if (!memcmp(f->layout, OLD_LAYOUT[i].name, 4))
            f->sbytes = OLD_LAYOUT[i].bytes;   /* (an older one's: made over as they are played) */
    if (!f->sbytes || scn0_l != f->sbytes || (have & 128 && (size_t)f->nscns * f->sbytes != scns_l))
        return FST_LAYOUT;               /* (another layout's scenes: not this engine's to play) */
    if (f->nparam > frs_param_count())
        return FST_LAYOUT;
    r = walk(f);
    return r;
}

void fst_start_of(const fst_file *f, fst_start *s)
{
    scene_copy(s->scene, f->scene, f->sbytes, 0);   /* (an older layout's SWELL: its MOVE's, after it) */
    s->state_n = f->state_n;
    if (f->state_n)
        memcpy(s->state, f->state, f->state_n);
    s->moves_n = (uint8_t)f->moves_n;
    if (f->moves_n)
        memcpy(s->moves, f->moves, 2u * f->moves_n);
    s->tracks = f->tracks;
}

/* ------------------------------------------------------------------ playing --- */
void fst_begin(fst_player *p, const fst_file *f)
{
    p->f = *f;
    p->at = p->t = p->missed = 0;
    p->cut_stop = 0;
    p->have = 0;
}

/* the next event decoded (fst_read walked them all: its bytes are there) */
static int decode(fst_player *p)
{
    const uint8_t *b = p->f.evts;
    size_t n = p->f.evts_n, at = p->at;
    uint32_t dt, l, skip;
    int len;
    uint8_t op;
    if (at >= n)
        return 0;
    l = varint_get(b + at, n - at, &dt);
    if (!l || at + l >= n)
        return 0;
    at += l;
    op = b[at++];
    len = op_len(op, b + at, n - at, &skip);
    if (len < 0 || n - at < skip + (uint32_t)len)
        return 0;
    p->t += dt;
    p->next.ms = p->t;
    p->next.op = op;
    p->next.n = (uint8_t)len;
    p->next.a = b + at + skip;
    p->at = (uint32_t)(at + skip + (uint32_t)len);
    p->have = 1;
    return 1;
}

static int known(uint8_t op)
{
    switch (op) {
    case FST_CUT: case FST_CUT_STOP: case FST_PLAY: case FST_PANIC: case FST_PARAM: case FST_HOLD_OFF: case FST_CLEAR: case FST_TRACK_PLAY:
    case FST_PLAY_ALL: case FST_KEY_UP: case FST_SCENE: case FST_SCENE_NOW: case FST_LATCH: case FST_HOLD: case FST_KEY_DOWN:
    case FST_MIDI: case FST_STORE: case FST_RECORDING: case FST_DSTEP: case FST_STATE: case FST_OPEN:
    case FST_ANCHOR: case FST_ANCHOR_BACK: case FST_ANCHOR_DROP:
    case FST_RELEASE: case FST_RELEASE_ALL: case FST_KEY_PRESSURE: case FST_SWELL: case FST_COMPARE: case FST_RECORD:
    case FST_KEY_SINGLE:
        return 1;
    default:
        return 0;
    }
}

int fst_next(fst_player *p, uint32_t now, fst_dec *e)
{
    for (;;) {
        if (!p->have && !decode(p))
            return 0;
        if (p->next.ms > now)
            return 0;
        p->have = 0;
        if (known(p->next.op)) {
            *e = p->next;
            return 1;
        }
        p->missed++;
    }
}

int fst_more(const fst_player *p) { return p->have || p->at < p->f.evts_n; }

static const uint8_t *scene_at(fst_player *p, uint32_t i)
{
    const uint8_t *s = p->f.scns + (size_t)i * p->f.sbytes;
    if (p->f.sbytes == FRS_SCENE_BYTES)
        return s;
    scene_copy(p->conv, s, p->f.sbytes, (uint8_t)frs_param_get(P_SWELL));   /* (an older layout's: made over, valid
                                                                             * till the next; SWELL as it is, as their
                                                                             * engines left it through a scene) */
    return p->conv;
}

int fst_apply(fst_player *p, const fst_dec *e, uint64_t frames)
{
    const uint8_t *a = e->a;
    int cs = p->cut_stop;
    p->cut_stop = e->op == FST_CUT_STOP;
    switch (e->op) {
    case FST_CUT:
        if (!cs)                         /* (after an FST_CUT_STOP: its old readers' CUT, done already) */
            frs_cut();
        break;
    case FST_CUT_STOP: frs_cut_stop(); break;
    case FST_PLAY: frs_play(); break;
    case FST_PANIC: frs_panic(); break;
    case FST_PARAM: frs_param_set(a[0], a[1]); break;
    case FST_HOLD_OFF: frs_hold_off(a[0]); break;
    case FST_CLEAR: frs_clear(a[0]); break;
    case FST_TRACK_PLAY: frs_track_play(a[0], a[1]); break;
    case FST_PLAY_ALL: frs_play_all(a[0]); break;
    case FST_KEY_UP: frs_key_up(a[0]); break;
    case FST_SCENE: frs_scene_go(scene_at(p, r16(a))); break;
    case FST_SCENE_NOW: frs_scene_take(scene_at(p, r16(a))); break;
    case FST_LATCH: frs_latch(a[0], a[1], (int8_t)a[2]); break;
    case FST_HOLD: frs_hold(a[0], a[1], a[2], (int8_t)a[3]); break;
    case FST_KEY_DOWN:                   /* (the keys play the track: KEYS' P_KB, set before each) */
    case FST_KEY_SINGLE:
        frs_param_set(P_KB, a[2]);
        frs_key_down(a[0], a[1], a[3] ? a[3] : 100u, e->op == FST_KEY_SINGLE);   /* (its velocity; 0, a touch's: 100) */
        break;
    case FST_KEY_PRESSURE: frs_key_pressure(a[0], a[1]); break;
    case FST_RELEASE: frs_release(); break;
    case FST_RELEASE_ALL: frs_release_all(); break;
    case FST_SWELL: frs_swell(a[0]); break;
    case FST_COMPARE: frs_compare(a[0]); break;
    case FST_RECORD: frs_record_layer(a[0]); break;
    case FST_MIDI: {                     /* (taken at the millisecond it was: the frame that says so, or at once) */
        double fs = (double)frs_rate(), at = (double)(frs_ms() + (uint32_t)a[3]), f2 = (double)(uint64_t)((at - 1.0) * fs / 1000.0) + 1.0;
        frs_midi_at(f2 > (double)frames ? (uint32_t)(f2 - (double)frames) : 0u, a[0], a[1], a[2]);
        break;
    }
    case FST_STORE: frs_scene_store(p->scratch, scene_at(p, r16(a))); break;
    case FST_RECORDING:
        frs_count_in(a[0]);
        frs_overdub(a[1]);
        frs_record_metronome(0);         /* (a take's ticks: a listening aid, heard live alone) */
        break;
    case FST_DSTEP: frs_dseq_step(a[0], a + 1, e->n - 1u); break;
    case FST_STATE: frs_state_load(a, e->n); break;
    case FST_ANCHOR: frs_anchor_put(scene_at(p, r16(a))); break;
    case FST_ANCHOR_BACK:
        if (p->f.sbytes != FRS_SCENE_BYTES && frs_anchor_get(p->conv)) {   /* (an older layout's anchor: SWELL as it is now,
                                                                             * as their engines left it) */
            p->conv[SC_SWELL] = (uint8_t)frs_param_get(P_SWELL);
            frs_anchor_put(p->conv);
        }
        frs_anchor_back(r16(a));
        break;
    case FST_ANCHOR_DROP: frs_anchor_drop(); break;
    case FST_OPEN: {
        fst_start s;
        uint32_t sn = a[7], mn = a[8 + sn];
        memcpy(s.scene, scene_at(p, r16(a)), FRS_SCENE_BYTES);
        if (p->f.sbytes != FRS_SCENE_BYTES)
            s.scene[SC_SWELL] = 0;       /* (as fst_start_of's: its MOVE's, after it) */
        s.tracks = a[2];
        s.state_n = sn;
        memcpy(s.state, a + 8, sn);
        s.moves_n = (uint8_t)mn;
        memcpy(s.moves, a + 9 + sn, 2u * mn);
        fst_fresh(&s, frs_rate());
        return 1;
    }
    default: break;
    }
    return 0;
}

/* ------------------------------------------------------------------ on air --- */
static void air_quiet(void)
{
    frs_record_metronome(0);             /* (a take's ticks: heard live alone) */
    frs_metronome(0);
}

/* the events due, first in a millisecond of the brain. MIDI by its millisecond: no frame is at hand, here or in a skip */
static void air_tick(void *user)
{
    fst_air *a = user;
    fst_dec e;
    if (a->due)
        return;
    while (fst_next(&a->p, fst_air_ms(a), &e)) {
        if (e.op == FST_OPEN) {          /* (never inside a tick) */
            a->open = e;
            a->due = 1;
            return;
        }
        if (e.op == FST_MIDI)
            frs_midi_ms((uint64_t)frs_ms() + e.a[3], e.a[0], e.a[1], e.a[2]);
        else
            fst_apply(&a->p, &e, 0);
        if (e.op == FST_STATE)
            air_quiet();
    }
}

uint32_t fst_air_ms(const fst_air *a) { return frs_ms() - a->t0 + a->base; }

int fst_air_open(fst_air *a, const fst_file *f, float fs)
{
    fst_start s;
    fst_dec e;
    fst_start_of(f, &s);
    if (fst_fresh(&s, fs))
        return -1;
    frs_count_in(0);
    air_quiet();
    fst_begin(&a->p, f);
    {
        fst_player q = a->p;
        a->opens = 0;
        while (!a->opens && fst_next(&q, 0xffffffffu, &e))
            a->opens = e.op == FST_OPEN;
    }
    a->t0 = frs_ms();
    a->base = 0;
    a->due = 0;
    frs_tick_hook(air_tick, a);
    return 0;
}

void fst_air_after(fst_air *a)
{
    if (!a->due)
        return;
    a->due = 0;
    fst_apply(&a->p, &a->open, 0);
    air_quiet();
    a->t0 = frs_ms();
    a->base = a->open.ms;
    frs_tick_hook(air_tick, a);
}

void fst_air_skip(fst_air *a, uint32_t ms)
{
    for (;;) {
        uint32_t at;
        fst_air_after(a);
        at = fst_air_ms(a);
        if (at >= ms)
            return;
        frs_skip_to((uint64_t)frs_ms() + (a->opens ? 1u : ms - at));   /* (with an opening ahead: to its very millisecond) */
    }
}

/* ------------------------------------------------------------------ live --- */
static void live_quiet(void)             /* (as the start opened it: the artist's ticks and count are not the listener's) */
{
    frs_count_in(0);
    air_quiet();
}

/* the running state waiting, at its place: loaded, or left (cmp: this engine's own the very same) */
static void live_state_here(fst_live *l)
{
    l->st_on = 0;
    if (l->st_cmp) {
        uint32_t n = frs_live_save(l->tmp, sizeof l->tmp, l->st_scene);
        if (n == l->st_n && !memcmp(l->tmp, l->st, n)) {
            l->same++;
            return;
        }
    }
    if (frs_live_load(l->st, l->st_n, l->st_scene)) {
        l->refused++;
        return;
    }
    live_quiet();
    l->loads++;
}

/* first in each millisecond: what is due by now, in the order given */
static void live_tick(void *user)
{
    fst_live *l = user;
    while (l->qr != l->qw) {
        fst_lq *e = &l->q[l->qr % FST_LQ];
        if (e->ms > frs_ms())
            return;
        l->qr++;
        if (e->kind == 2u) {
            live_state_here(l);
            continue;
        }
        if (e->ms < frs_ms())
            l->late++;
        fst_op(l, e->op, e->a, e->n, e->kind == 1u ? l->scn[e->sc] : 0);
        if (e->kind == 1u)
            l->scn_n--;
    }
}

int fst_live_open(fst_live *l, const fst_start *s, float fs)
{
    memset(l, 0, sizeof *l);
    if (fst_fresh(s, fs))
        return -1;
    live_quiet();
    frs_tick_hook(live_tick, l);
    return 0;
}

int fst_live_state(fst_live *l, const uint8_t *st, uint32_t n, const uint8_t *scene)
{
    if (frs_live_load(st, n, scene)) {
        l->refused++;
        return -1;
    }
    live_quiet();
    l->loads++;
    return 0;
}

int fst_live_at(fst_live *l, uint32_t ms, uint8_t op, const uint8_t *a, uint32_t n, const uint8_t *scene)
{
    fst_lq *e;
    if (l->qw - l->qr >= FST_LQ || n > sizeof e->a || (scene && l->scn_n >= FST_LQ_SCN)) {
        l->full++;
        return -1;
    }
    e = &l->q[l->qw % FST_LQ];
    e->ms = ms;
    e->op = op;
    e->n = (uint8_t)n;
    e->kind = 0;
    if (n)
        memcpy(e->a, a, n);
    if (scene) {
        e->kind = 1;
        e->sc = (uint8_t)(l->scn_w++ % FST_LQ_SCN);
        memcpy(l->scn[e->sc], scene, FRS_SCENE_BYTES);
        l->scn_n++;
    }
    l->qw++;
    return 0;
}

int fst_live_at_state(fst_live *l, uint32_t ms, const uint8_t *st, uint32_t n, const uint8_t *scene, int cmp)
{
    fst_lq *e;
    if (l->st_on || n > sizeof l->st || l->qw - l->qr >= FST_LQ) {
        l->full++;
        return -1;
    }
    memcpy(l->st, st, n);
    l->st_n = n;
    if (scene)
        memcpy(l->st_scene, scene, FRS_SCENE_BYTES);
    else
        memset(l->st_scene, 0, FRS_SCENE_BYTES);
    l->st_on = 1;
    l->st_cmp = (uint8_t)(cmp != 0);
    e = &l->q[l->qw % FST_LQ];
    e->ms = ms;
    e->op = 0;
    e->n = 0;
    e->kind = 2;
    l->qw++;
    return 0;
}

uint32_t fst_live_state_ms(const fst_live *l)
{
    uint32_t i;
    for (i = l->qr; l->st_on && i != l->qw; i++)
        if (l->q[i % FST_LQ].kind == 2u)
            return l->q[i % FST_LQ].ms;
    return 0xFFFFFFFFu;
}

void fst_live_cut(fst_live *l)
{
    uint32_t i;
    for (i = l->qr; i != l->qw; i++)
        if (l->q[i % FST_LQ].kind == 2u)
            break;
    for (; l->qw != i; l->qw--)          /* (from the newest back: the scenes they held given back) */
        if (l->q[(l->qw - 1u) % FST_LQ].kind == 1u) {
            l->scn_n--;
            l->scn_w--;
        }
    l->st_on = 0;
}

int fst_op(fst_live *l, uint8_t op, const uint8_t *a, uint32_t n, const uint8_t *scene)
{
    static const uint8_t first[4];       /* (a scene's index: the one given, SCNS's first) */
    uint32_t need = op >> 6 == 3u ? 1u : op >> 6 == 2u ? 4u : 2u * (op >> 6);   /* (its args in a file: 0, 2, 4, or
                                                                                 * a length's, one at least) */
    fst_dec e;
    if (!known(op)) {
        l->missed++;
        return 1;
    }
    if (op == FST_SCENE || op == FST_SCENE_NOW || op == FST_ANCHOR || op == FST_STORE) {
        if (!scene) {
            l->refused++;
            return -1;
        }
        a = first;
        n = need;
    } else if (n < need || (need && !a) || op == FST_OPEN || (op == FST_DSTEP && n > 9u) ||
               (op == FST_STATE && n > FRS_STATE_BYTES)) {
        l->refused++;
        return -1;
    }
    if (op == FST_MIDI)                  /* (no frame at hand: by its millisecond, as on air) */
        frs_midi_ms((uint64_t)frs_ms() + a[3], a[0], a[1], a[2]);
    else {
        l->p.f.sbytes = FRS_SCENE_BYTES;
        l->p.f.scns = scene;
        l->p.f.nscns = scene ? 1u : 0u;
        e.ms = frs_ms();
        e.op = op;
        e.n = (uint8_t)(op >> 6 == 3u ? n : need);
        e.a = a;
        fst_apply(&l->p, &e, 0);
        l->p.f.scns = 0;
        if (op == FST_STATE || (op == FST_PARAM && a[0] == P_METRO))
            air_quiet();
    }
    l->done++;
    return 0;
}
