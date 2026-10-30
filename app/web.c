/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's own exports in the browser build (app.h APP_WEB; base/web/emu/emu_web.c includes it): the River plays a
 * shard with the app as it runs. The page writes the shard's scene_t (272 bytes, its bass's sequence last) to
 * web_scene_buf, then calls web_scene_play; web_scene_stop lets every note go (web/emu/app_worklet.js: {type: "app",
 * op: "scene" | "stop"}) */
static scene_t web_sc;
static uint8_t web_notes;                /* what the shard plays edited since it came in (THE KEYS) */
static void web_holds_off(void);         /* (THE KEYS' HOLD, below: nothing held any more; what they hold let go) */
static void web_holds_release(void);
__attribute__((used, visibility("default"))) uint8_t *web_scene_buf(void) { return (uint8_t *)&web_sc; }
__attribute__((used, visibility("default"))) uint32_t web_scene_size(void) { return (uint32_t)sizeof web_sc; }
__attribute__((used, visibility("default"))) void web_scene_play(void) { web_notes = 0; web_holds_off(); scene_audition(&web_sc, 1); }
__attribute__((used, visibility("default"))) void web_scene_stop(void) { web_holds_off(); audition_stop(); }
/* the River's editor's own engine (silent): the shard taken as a load takes it, no chord held for one that plays
 * nothing (▶'s only: never saved) */
__attribute__((used, visibility("default"))) void web_scene_take(void) { web_notes = 0; web_holds_off(); scene_audition(&web_sc, 0); }

/* what the shard plays, edited: ▶'s chord on its ROOT let go first (it plays something of its own now), and what
 * HOLD holds (the editor's engine never had it) */
static void web_edits(void)
{
    web_holds_release();
    if (aud_root >= 0 && bit_get(lit, aud_root)) {
        chord_off(&lat[aud_root]);
        bit_put(lit, aud_root, 0);
    }
    aud_root = -1;
    web_notes = 1;
}

/* the River's editor: a sound setting (p < NSOUND) or the scene's TEMPO set to x as a MIDI CC learned on it sets it
 * (param_set: a preset's sound, CUSTOM from a drone's setting turned, GLINT's type...); the sequences' steps as GLINT
 * SEQ's and BASS SEQ's encoders set them (the step on STEP, then its NOTE, ACCENT, STEP LENGTH; CLEAR turned twice:
 * x 2; SEQ STEPS, SEQUENCE, TRANSPOSE; RECORD not: THE KEYS write the steps). Then the shard as it is now
 * (web_scene_edited, web_scene_size() bytes): the one played (web_sc), its sound as STORE keeps it (kept_v: the
 * drone's CUSTOM settings and its preset's number, not the preset's values) and its TEMPO (one it had out of range kept
 * as it was); once what it plays was edited, what plays now as STORE keeps it (scene_notes_take: the chords latched,
 * the arpeggio's notes, the bass held, the sequences). BASS's OCTAVE turned with a note held is one: the bass is kept
 * as it sounds */
__attribute__((used, visibility("default"))) void web_param(uint32_t p, uint32_t x)
{
    if (p < NSOUND || p == P_TEMPO) {
        int b = bass_note >= 0 ? bass_sounding(bass_note) : -1;
        param_set((int)p, (int32_t)x);
        if (b >= 0 && bass_note >= 0 && bass_sounding(bass_note) != b)
            web_notes = 1;
    } else if ((p >= P_SQ_STEP && p <= P_SQ_CLEAR && p != P_SQ_REC) || (p >= P_BQ_STEP && p <= P_BQ_TRANS && p != P_BQ_REC)) {
        uint8_t s0[SC_SEQ], n0[SC_BSEQ], l0[SC_BSEQ], q0[3] = {st.v[P_BQ_ON], st.v[P_BQ_STEPS], st.v[P_BQ_TRANS]};
        web_holds_release();
        memcpy(s0, seq, sizeof s0);
        memcpy(n0, bnote, sizeof n0);
        memcpy(l0, blen, sizeof l0);
        param_set((int)p, (int32_t)x);
        if (p == P_SQ_STEP)              /* (the step's own, whatever STEP was) */
            seq_view();
        else if (p == P_BQ_STEP)
            bseq_view();
        if (memcmp(s0, seq, sizeof s0) || memcmp(n0, bnote, sizeof n0) || memcmp(l0, blen, sizeof l0) ||
            q0[0] != st.v[P_BQ_ON] || q0[1] != st.v[P_BQ_STEPS] || q0[2] != st.v[P_BQ_TRANS])
            web_edits();                 /* (a step written as it was: nothing edited) */
    }
}
__attribute__((used, visibility("default"))) uint8_t *web_scene_edited(void)
{
    static scene_t e;
    uint32_t i;
    e = web_sc;
    if (web_notes)
        scene_notes_take(&e);
    for (i = 0; i < NSOUND; i++)
        e.v[i] = kept_v(i);
    e.tempo = web_sc.tempo <= PD[P_TEMPO].max ? st.v[P_TEMPO] : web_sc.tempo;
    return (uint8_t *)&e;
}

/* THE KEYS, LATCH: track t's note n (as it sounds) latched, or let go if it is (1: latched, 0: let go, -1: nothing,
 * full). 0 the drones: the chord HARMONY makes at n, on its root (eight at most, as a scene keeps them); a chord's own
 * root clicked lets it go, whatever HARMONY makes there now; 1 GLINT's
 * arpeggio: n without OCTAVE among its notes (sixteen at most), played from its next step; 2 the bass: n held (another
 * note: it goes there, PULSE keeping the beat), BASS's OCTAVE off it */
__attribute__((used, visibility("default"))) int web_latch(uint32_t t, uint32_t note)
{
    int n = note > 127u ? 127 : (int)note, root, k = 0, i;
    chord_t c;
    web_holds_release();                 /* (the shard as it is, then the key: as the editor's engine has it) */
    if (t == 0u) {
        chord_build(n, st.v, &c, &root);
        if (bit_get(lit, n) && n != aud_root)
            root = n;
        if (bit_get(lit, root) && root != aud_root) {
            web_edits();
            chord_off(&lat[root]);
            bit_put(lit, root, 0);
            return 0;
        }
        for (i = 0; i < 128; i++)
            k += bit_get(lit, i) && i != aud_root;
        if (k >= (int)SC_CHORDS)
            return -1;
        web_edits();
        lat[root] = c;
        bit_put(lit, root, 1);
        chord_on(&c, 100);
        return 1;
    }
    if (t == 1u) {
        int b = n - 12 * (int)st.v[P_GL_OCTAVE];
        if (b < 1)
            return -1;
        for (i = 0; i < arp.n; i++)
            if (arp.held[i] == b) {
                web_edits();
                arp_remove(b);
                if (!arp.n)
                    arp_stop();
                return 0;
            }
        if (arp.n >= SC_ARP)
            return -1;
        web_edits();
        arp_add(b, 100);
        arp.latch = 1;
        arp.paused = 0;
        return 1;
    }
    if (t == 2u) {
        int played = n - 12 * ((int)st.v[P_BS_OCTAVE] - 2);
        web_edits();
        if (bass_note >= 0 && bass_sounding(bass_note) == n) {
            bass_stop();
            return 0;
        }
        bass_live(played, 100);
        return 1;
    }
    return -1;
}

/* THE KEYS' CLEAR: every chord latched let go (0), the arpeggio's notes (1), the bass held (2) */
__attribute__((used, visibility("default"))) void web_clear(uint32_t t)
{
    int i;
    web_edits();
    if (t == 0u) {
        for (i = 0; i < 128; i++)
            if (bit_get(lit, i)) {
                chord_off(&lat[i]);
                bit_put(lit, i, 0);
            }
    } else if (t == 1u) {
        arp.n = arp.pn = 0;
        arp.dirty = 1;
        arp_stop();
    } else if (t == 2u) {
        bass_stop();
    }
}

/* THE KEYS, HOLD (the engine playing only: nothing kept): track t's note n (as it sounds) while it is down. The drones
 * the chord HARMONY makes there; GLINT on ARP the arpeggio playing it too (a note it has already: as it was), else the
 * glint itself (the sequence's own note left to it); the bass there, and back to the note it had once let go, or
 * silent (its sequence playing: the sequence goes on) */
static chord_t web_hc[128];
static uint32_t web_ha[4];               /* GLINT's notes held into the arpeggio (without OCTAVE, as it keeps them) */
static int8_t web_hb = -1, web_hbn = -1; /* the bass's note before the one held, the one held (as played) */
static void web_holds_off(void)          /* (a shard taken, or stopped: what the keys held is the engine's no more) */
{
    memset(web_hc, 0, sizeof web_hc);
    memset(web_ha, 0, sizeof web_ha);
    web_hb = web_hbn = -1;
}
static void web_holds_release(void)      /* (everything HOLD holds let go, as its keys would be: back to the shard) */
{
    int n;
    for (n = 0; n < 128; n++) {
        if (web_hc[n].k) {
            chord_off(&web_hc[n]);
            web_hc[n].k = 0;
        }
        if (bit_get(web_ha, n)) {
            bit_put(web_ha, n, 0);
            arp_remove(n);
            if (!arp.n)
                arp_stop();
        }
    }
    if (web_hbn >= 0) {
        if (bass_note == web_hbn && !(st.v[P_BQ_ON] && bq.run)) {
            if (web_hb >= 0)
                bass_live(web_hb, 100);
            else
                bass_stop();
        }
        web_hbn = web_hb = -1;
    }
}
__attribute__((used, visibility("default"))) void web_hold(uint32_t t, uint32_t note, uint32_t down)
{
    int n = note > 127u ? 127 : (int)note, root, i;
    if (t == 0u) {
        if (down && !web_hc[n].k) {
            chord_build(n, st.v, &web_hc[n], &root);
            chord_on(&web_hc[n], 100);
        } else if (!down && web_hc[n].k) {
            chord_off(&web_hc[n]);
            web_hc[n].k = 0;
        }
    } else if (t == 1u) {
        int b = n - 12 * (int)st.v[P_GL_OCTAVE];
        if (b < 1)
            return;
        if (st.v[P_SQ_TYPE] == SQT_ARP) {
            if (down && !bit_get(web_ha, b)) {
                for (i = 0; i < arp.n && arp.held[i] != b; i++)
                    ;
                if (i == arp.n && arp.n < 16u) {
                    arp_add(b, 100);
                    arp.paused = 0;
                    bit_put(web_ha, b, 1);
                }
            } else if (!down && bit_get(web_ha, b)) {
                bit_put(web_ha, b, 0);
                arp_remove(b);
                if (!arp.n)
                    arp_stop();
            }
        } else if (down) {
            frasil_glint_on(b, 100.0f / 127.0f);
        } else if (sq.sounding != b) {
            frasil_glint_off(b);
        }
    } else if (t == 2u) {
        int played = n - 12 * ((int)st.v[P_BS_OCTAVE] - 2);
        if (down) {
            if (web_hbn < 0)
                web_hb = bass_note;
            web_hbn = (int8_t)played;
            bass_live(played, 100);
        } else if (web_hbn == played) {
            if (bass_note == played && !(st.v[P_BQ_ON] && bq.run)) {
                if (web_hb >= 0)
                    bass_live(web_hb, 100);
                else
                    bass_stop();
            }
            web_hbn = web_hb = -1;
        }
    }
}

/* what sounds now, for the River's live view (app_worklet.js {op: "live"}): WEB_LIVE floats, a voice's note -1 when
 * free, its level the envelope (0..1):
 *    0.. 7  the drone's voices' notes      8..15  their levels
 *   16..19  GLINT's voices' notes (OCTAVE on them)   20..23  their levels as heard (0..1: 48 dB from full to gone)
 *   24      the bass's note (OCTAVE on it, folded)   25  its level
 *   26..28  each LFO on (DEPTH over 0 and a TARGET)  29..31  where in its turn (0..1)  32..34  SHAPE's value (-1..1)
 *   35..37  its turns gone by (RANDOM and DRIFT: which turn's value)
 *   38      GLINT's sequence: the step playing, -1 none     39  GLINT's note sounding (its sequence's or the
 *                                                              arpeggio's, as played: before OCTAVE), -1 none
 *   40      the bass's sequence: the step playing, -1 none  41  MIDI clocks into it
 *   42..45  the tracks' meters (drones, bass, glints, the cloud), 0..1     46  the tide on the timbre, -1..1
 *   47      the notes GLINT's arpeggio and sequence have struck (more than at the last frame: those struck since are
 *           the voices struck last, 48..51)
 *   48..51  GLINT's voices: the seconds since each was struck */
#define WEB_LIVE 52
static float web_lv[WEB_LIVE];
__attribute__((used, visibility("default"))) uint32_t web_live_size(void) { return WEB_LIVE; }
__attribute__((used, visibility("default"))) float *web_live(void)
{
    frasil_view_t v[FR_NV];
    glint_view_t g[GLINT_NV];
    bass_view_t b;
    float m[4];
    uint32_t i;
    frasil_view(v);
    frasil_glint_view(g);
    frasil_bass_view(&b);
    frasil_meters(m);
    for (i = 0; i < FR_NV; i++) {
        web_lv[i] = (float)v[i].note;
        web_lv[8 + i] = v[i].note < 0 ? 0.0f : v[i].level;
    }
    for (i = 0; i < GLINT_NV; i++) {
        float l = g[i].level;
        web_lv[16 + i] = (float)g[i].note;
        web_lv[20 + i] = g[i].note < 0 || l < 0.004f ? 0.0f : br_clamp01(1.0f + fm_log2f(l) * (1.0f / 8.0f));
        web_lv[48 + i] = g[i].t;
    }
    web_lv[24] = (float)b.note;
    web_lv[25] = b.note < 0 ? 0.0f : b.level;
    for (i = 0; i < NLFO; i++) {
        web_lv[26 + i] = lfo[i].on ? 1.0f : 0.0f;
        web_lv[29 + i] = lfo[i].ph;
        web_lv[32 + i] = lfo[i].out;
        web_lv[35 + i] = (float)lfo[i].cyc;
    }
    web_lv[38] = st.v[P_SQ_TYPE] == SQT_SEQ && sq.run ? (float)sq.pos : -1.0f;
    web_lv[39] = sq.sounding >= 0 ? (float)sq.sounding : arp.sounding >= 0 ? (float)arp.sounding : -1.0f;
    web_lv[40] = st.v[P_BQ_ON] && bq.run ? (float)bq.pos : -1.0f;
    web_lv[41] = bq.t;
    for (i = 0; i < 4u; i++)
        web_lv[42 + i] = m[i];
    web_lv[46] = frasil_tide();
    web_lv[47] = (float)(arp.steps + sq.steps);
    return web_lv;
}
