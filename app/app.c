/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Copyright (C) 2026 Charles Vestal (from fm1-base's template) */
/* FRASIL: the app. The keys light drones: a tap latches a chord (the harmony makes one of the note: harmony.c), a
 * tap on a lit one lets it go, holding it down presses into it. SELECT turns the keys to GLINT, the icy glints played
 * over the drones (never latched; ARP is their pages, held their arpeggio's latch), or to BASS, one deep note held
 * under them (SEQ is its pages). The three left encoders are the performance's hands (KEYBOARD, the drone's PRESET,
 * FULLNESS), K1 .. K4 the page's parameters; PLAY lets every note go, SEQ held swells every voice, REC records
 * into GLINT's sequencer (held: its metronome), FREEZE (CLOUD's K4) holds the cloud. MIDI in plays the drones too, through the same
 * harmony. Nothing plays by itself (SPARKLE's glints answer what the drones hold).
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in
 * scope. The sound is in dsp/ (compiled apart, fm1.toml o2). */
#include "fastmath.h"
#include "frasil.h"
#include "params.c"

#define SET_MAGIC 0x47535246u            /* "FRSG": the settings record (a new one when its layout changes) */
#define SCN_MAGIC 0x67535246u            /* "FRSg": the scenes record (with their notes, the bass and the sequence) */
#define OCT_DEF (-1)                     /* the keys start at F2: drones live low */
#define TAP_MS 350u                      /* a lit key let go sooner than this, without pressing in: it goes out */
#define HELP_PAGES 5                     /* the help: three of buttons, the encoders, the keys */
#define HOLD_MS 400u                     /* a button held this long does its other thing (on release, a tap its own) */
#define PLAY_HOLD_MS 600u                /* PLAY's, which lets everything go: longer, the screen counting it down */
#define PLAY_SHOW_MS 200u                /* from here the screen counts it down, and letting go does nothing */
#define NLEARN 8u                        /* MIDI CCs learned (GLO held, a CC) */

typedef struct {
    uint32_t magic;
    uint8_t v[NSOUND];                   /* the sound as last saved */
    int8_t octave;
    uint8_t keys, channel, scene, tempo; /* channel: the drones' (P_CANAL's value) */
    uint8_t ch_glint, ch_bass;
    uint8_t seq[SC_SEQ];                 /* GLINT's sequence as last saved */
    uint8_t metro_off;                   /* REC's metronome off (0, as a record without it reads: on) */
    uint8_t morph, keep;                 /* SCENES' MORPH and KEEP */
    uint8_t learn_cc[NLEARN], learn_p[NLEARN];   /* the CCs learned and their settings (cc 0: none) */
} settings_t;

typedef struct {
    uint32_t magic;
    uint16_t used;                       /* bit s: slot s has a saved scene */
    uint16_t rsv;
    scene_t s[NSCENES];
} scenes_t;

static settings_t set;
static scenes_t scn;

/* the records of earlier layouts, read byte by byte and made over into the new ones (the parameters added since at
 * their defaults). A scene: its name (10), the sound (nv), nch, narp, the chords (8 x 5), the arpeggio (16), the bass
 * note (if the layout has one); the scenes' record: magic, used, rsv, 16 scenes. The settings: magic, the sound,
 * octave, keys, channel (one for all: 0 OMNI), scene, tempo, the GLINT and BASS channels (if it has them), to 4 */
typedef struct {
    uint32_t magic;
    uint8_t nv, extra;                   /* sound parameters; extra 1: scenes a bass note, settings two more channels;
                                          * 2: and the sequence (SC_SEQ) */
} layout_t;
static const layout_t OLD_SCN[] = {
    {0x56535246u, 55, 0},                /* "FRSV": before BASS */
    {0x57535246u, 64, 1},                /* "FRSW": BASS's first (SPACE where MODEL is now) */
    {0x58535246u, 65, 1},                /* "FRSX": with RESONANCE */
    {0x59535246u, 77, 1},                /* "FRSY": with PUMP, MASTERING, MIXER */
    {0x5A535246u, 78, 1},                /* "FRSZ": with the shimmer's BRIGHT */
    {0x61535246u, 81, 1},                /* "FRSa": with PUMP II */
    {0x62535246u, 94, 1},                /* "FRSb": with the layers' second pages and SUSTAIN */
    {0x63535246u, 110, 1},               /* "FRSc": with the LFOs */
    {0x64535246u, 113, 2},               /* "FRSd": with the sequence */
    {0x65535246u, 115, 2},               /* "FRSe": with the drone's presets */
    {0x66535246u, 123, 2},               /* "FRSf": with LFO 3 */
};
static const layout_t OLD_SET[] = {
    {0x33535246u, 55, 0}, {0x34535246u, 64, 0}, {0x35535246u, 65, 0},   /* "FRS3", "FRS4", "FRS5" */
    {0x36535246u, 65, 1},                /* "FRS6": a channel for each track */
    {0x37535246u, 77, 1},                /* "FRS7": with PUMP, MASTERING, MIXER */
    {0x38535246u, 78, 1},                /* "FRS8": with the shimmer's BRIGHT */
    {0x39535246u, 81, 1},                /* "FRS9": with PUMP II */
    {0x41535246u, 94, 1},                /* "FRSA": with the layers' second pages and SUSTAIN */
    {0x42535246u, 110, 1},               /* "FRSB": with the LFOs */
    {0x43535246u, 113, 2},               /* "FRSC": with the sequence */
    {0x44535246u, 115, 2},               /* "FRSD": with the drone's presets */
    {0x45535246u, 123, 2},               /* "FRSE": with LFO 3 */
    {0x46535246u, 127, 2},               /* "FRSF": with TYPE and GENERATIVE FX (METRONOME after the sequence) */
};
static uint8_t raw[3600];               /* an old record as it was saved (the largest, FRSf's scenes: 3592) */
_Static_assert(sizeof(scene_t) == 10u + NSOUND + 2u + SC_CHORDS * 5u + SC_ARP + 1u + SC_SEQ, "a scene: its bytes, no padding");
_Static_assert(P_BS_DRIVE == 55 && P_BS_RESO == 64 && P_PUMP_RATE == 65 && P_SH_TONE == 77 && P_PUMP_REL == 78 &&
                   P_CH_SHAPE == 81 && P_L1_SHAPE == 94 && P_SQ_LEN == 110 && P_DR_PRESET == 113 && P_L3_SHAPE == 115 &&
                   P_SQ_TYPE == 123,
               "the parameters added come after the old");
_Static_assert(sizeof(scenes_t) <= 3840u, "the scenes fit a store object (4 KiB less its 256-byte header)");
_Static_assert(sizeof(settings_t) == 192u, "the settings: FRSF's 172 bytes, MORPH, KEEP and the learned CCs after");

_Static_assert(sizeof raw >= 8u + NSCENES * (10u + 123u + 2u + SC_CHORDS * 5u + SC_ARP + 1u + SC_SEQ), "raw holds the largest");
static uint32_t seq_bytes(const layout_t *l) { return l->extra >= 2u ? SC_SEQ : 0u; }
static uint32_t scn_bytes(const layout_t *l)
{
    return 10u + l->nv + 2u + SC_CHORDS * 5u + SC_ARP + (l->extra ? 1u : 0u) + seq_bytes(l);
}
static uint32_t set_bytes(const layout_t *l) { return (4u + l->nv + 5u + 2u * (l->extra ? 1u : 0u) + seq_bytes(l) + 3u) & ~3u; }
static uint32_t rd32(const uint8_t *b) { return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

/* --------------------------------------------------------------- state --- */
static struct {
    uint8_t v[NPARAM];                   /* every parameter now */
    int8_t octave;                       /* -3 .. +2 */
    uint8_t glint;                       /* the keys play GLINT */
    uint8_t bass;                        /* the keys play BASS */
    uint8_t logo;                        /* the logo's screen (HOME again, and at power-on) */
    uint32_t logo_until;                 /* at power-on: it gives way to the river by itself (0: stays)*/
    uint8_t page;                        /* PG_* */
    uint8_t scene;                       /* the slot loaded or saved last, 0 .. 15 */
    uint8_t pick;                        /* SCENES: the slot K1 points at */
    uint8_t dirty;                       /* the sound changed since the scene was loaded or saved */
    uint8_t sustain;                     /* MIDI CC64 held */
    float swell_btn, swell_midi, swell_sent;
    uint32_t hold_at[NB];                /* when each button went down */
    uint32_t hold_done;                  /* bit b: b's hold has acted, or b went with something (SEL and a key, SAVE
                                          * and a key or REC, LFO and an encoder, GLO and a CC): no tap on release */
    uint8_t prev_page;                   /* the page SCENES came from (SAVE again goes back) */
    uint8_t kb_rec;                      /* KEYBOARD before REC turned it to GLINT (REC again gives it back) */
    uint8_t rec_took;                    /* the take under way is REC's (KEYBOARD to give back) */
    uint8_t loading_morph;               /* scene_go loading a scene to MORPH to */
    uint8_t compare;                     /* EDIT held: the scene as saved sounds, not as it is now */
    uint32_t keys_arm_at;                /* GLOBAL's KEYS: the first detent, waiting for the second */
    int8_t keys_arm;
    uint8_t metro;                       /* REC's metronome: a tick on each beat while the sequencer records */
    uint32_t clicks;                     /* its ticks since RECORD went on (the tests) */
    uint8_t help, help_page;             /* HOME held: the help on the screen until HOME again; SELECT its pages */
    uint8_t touched_enc;                 /* the encoder that turned it (EN_*: the screen's badge) */
    uint8_t touched;                     /* the parameter turned last (the screen shows it a moment) */
    uint32_t touched_at;
    uint32_t enc_at[NE];                 /* the last detent of each encoder (acceleration) */
    uint8_t engine_dirty;
} st;

/* the drones by chord: how many chords hold each note (a note shared by two goes out with the last), the latched
 * chords by root, and MIDI's by the note that came in */
static uint8_t held_by[128];
static chord_t lat[128], mid[128];
static uint32_t lit[4], mid_on[4], sus[4];
static int8_t bass_note = -1;            /* the bass's note held (latched, or its key down), -1: none */
static uint32_t mgl[4];                  /* GLINT's notes held from MIDI */
static uint32_t mgs[4];                  /* of them, those that sounded their glint themselves (as a key's gsound) */
static int8_t mgl_base[128];             /* each one's note without GLINT's OCTAVE, as the arpeggio has it */
static int8_t mbass[8];                  /* BASS's notes held from MIDI, the last on top (it sounds) */
static uint32_t mb_n;

static int bit_get(const uint32_t *b, int n) { return n >= 0 && n < 128 && (b[n >> 5] >> (n & 31) & 1u); }
static void bit_put(uint32_t *b, int n, int on)
{
    if (n < 0 || n > 127)
        return;
    if (on)
        b[n >> 5] |= 1u << (n & 31);
    else
        b[n >> 5] &= ~(1u << (n & 31));
}

enum { K_NONE, K_LATCH, K_HOLD, K_GLINT, K_BASS };
static struct {                          /* each key while it is down */
    uint8_t kind;                        /* what it started as (the mode may change while it is held) */
    int8_t root;                         /* K_LATCH: its chord's root; K_GLINT: the glint's note; K_BASS: the bass's */
    int8_t gbase;                        /* K_GLINT: its note without GLINT's OCTAVE (the arpeggio's, the sequence's) */
    uint8_t gsound;                      /* K_GLINT: it sounded its glint itself (TYPE OFF, or RECORD on): its release
                                          * lets it go, whatever TYPE and RECORD are by then */
    chord_t chord;                       /* K_HOLD: the chord it holds */
    uint8_t was_lit;                     /* it was latched already when pressed: a tap lets it go */
    uint8_t pressed_in;                  /* it was pressed into: no tap */
    uint8_t prs;                         /* its pressure last sent */
    uint32_t t;
} key[NKEYS];
static uint32_t keys_prev, btn_prev, btn_now;

static int b_held(uint32_t b) { return btn_now >> b & 1u; }
static void b_used(uint32_t b) { st.hold_done |= 1u << b; }   /* (its release no tap) */
static uint32_t leds_now, midleds_now, glowleds_now;   /* the buttons' lights last set (the tests) */
static volatile uint8_t oct_soft;        /* OCT- and OCT+ in their blink now (app_tick lights them) */
static float pump_low = 1.0f;            /* the drones' lowest pump gain since the tests last asked */

/* the lights: the buttons lit, at the mid level and in the glow, the keys lit and in the glow. The glow is felucca-bsp's
 * DIM HI (~1/30 of lit); the mid level its steady one (lit one frame in eight, the glow between: ~15 %), which the
 * platform's plat_lights does not offer: on the FM-1 it is set here, in fm1_led_mid (the unity build has it in scope).
 * OCT's blink is softer still (app_tick): the glow and every other mid frame, ~9 % */
#ifndef PLAT_HOST
static uint8_t oct_ml[FM1_NCOL];         /* OCT- and OCT+ in fm1_led_mid's columns */
#endif
static void lights_set(uint32_t lit, uint32_t mid, uint32_t glow, uint32_t keys, uint32_t keys_glow)
{
    leds_now = lit;
    midleds_now = mid;
    glowleds_now = glow;
    plat_lights(lit, keys, mid | glow, keys_glow, 2);
#ifndef PLAT_HOST
    {
        uint8_t ml[FM1_NCOL] = {0}, om[FM1_NCOL] = {0};
        uint32_t i, f;
        for (i = 0; i < NB; i++) {
            uint8_t q = led_pos[panel.btn[i]];
            if (q == 0xFF)
                continue;
            if (mid >> i & 1u)
                ml[q >> 3] |= (uint8_t)(1u << (q & 7u));
            if (i == B_OCTDN || i == B_OCTUP)
                om[q >> 3] |= (uint8_t)(1u << (q & 7u));
        }
        f = plat_lock();                 /* (the tick sets OCT's bits) */
        for (i = 0; i < FM1_NCOL; i++) {
            oct_ml[i] = om[i];
            fm1_led_mid[i] = (uint8_t)((ml[i] & ~om[i]) | (fm1_led_mid[i] & om[i]));
        }
        plat_unlock(f);
    }
#endif
}

/* 1 kHz, in the scan's interrupt (it lights the frames): once a mid frame has gone by, OCT's bits for the next, on
 * every other one while they blink */
void app_tick(void)
{
#ifndef PLAT_HOST
    static uint8_t was_mid, odd;
    uint32_t c;
    if (!fm1__mid_ph) {
        was_mid = 1;
        return;
    }
    if (!was_mid)
        return;
    was_mid = 0;
    odd ^= 1u;
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led_mid[c] = (uint8_t)((fm1_led_mid[c] & ~oct_ml[c]) | (oct_soft && odd ? oct_ml[c] : 0u));
#endif
}

/* GLINT's arpeggiator's state (its code below, after the MIDI): here for the scenes */
static struct {
    int8_t held[16];                     /* the notes the keys (or the latch, a scene) ask for, low to high */
    float prs[16];                       /* and how hard each is pressed */
    int8_t play[16];                     /* the notes playing: held's, taken at the next step (on RATE's grid) */
    float pprs[16];
    uint8_t pn, dirty;                   /* how many play; held changed since the last step */
    float tick;                          /* the grid: MIDI clocks (24 a beat) since the start, from the clock or
                                          * from TEMPO; wraps at 9600 (every step size divides it) */
    uint32_t last_ms, clk_new;           /* the last run; MIDI clocks come in since it */
    int32_t step_n;                      /* the step of the grid at the last run */
    int8_t src;                          /* the held note the sounding step comes from */
    uint8_t n, vel;
    int32_t idx, dir;
    int16_t sounding;                    /* the step sounding, -1: none */
    uint32_t off_ms;
    uint32_t clk, clk_at, clk_dt;        /* MIDI clocks since the last step; when the last came; ms between */
    float clk_ms;                        /* ms between clocks, smoothed finely (the tempo, for the delay) */
    uint32_t steps;                      /* steps played (the tests) */
    uint8_t latch;                       /* the notes stay when the keys let go (ARP held) */
    uint8_t paused;                      /* PLAY tapped: it waits, its notes kept (PLAY again: from the top) */
    uint32_t rng;
} arp = {.sounding = -1, .rng = 0x9E3779B9u, .latch = 1, .step_n = -1};   /* latched from the start */

/* GLINT's step sequencer (TYPE on SEQ): SC_SEQ steps, a byte each, its note (SQ_REST, a note, or SQ_TIE: the
 * last one held on) and SQ_ACC; LENGTH of them, played on the arpeggiator's grid (RATE, CHANCE) from the step it
 * started on; the keys transpose it along the scale (RECORD OFF), or write into it (STEP: at STEP, then on; LIVE: on
 * the step nearest the playhead) */
#define SQ_REST 0u
#define SQ_TIE 127u
#define SQ_ACC 0x80u
#define SQT_ARP 1u                       /* (P_SQ_TYPE's values: 0 OFF) */
#define SQT_SEQ 2u
static uint8_t seq[SC_SEQ];
static struct {
    uint8_t run;                         /* playing */
    uint8_t restart;                     /* MIDI Start: from its first step, at the grid's top */
    int32_t start;                       /* the grid's step its first step fell on */
    int32_t pos;                         /* the step played last (-1: none yet) */
    int32_t skip;                        /* a step LIVE has just written ahead of the playhead: it sounded already */
    int16_t sounding;                    /* its note sounding, -1: none */
    uint32_t off_ms;
    uint32_t steps;                      /* notes played (the tests) */
} sq = {.pos = -1, .skip = -1, .sounding = -1};

/* GLINT's pages: GLINT ARP only while TYPE is on ARP, the SEQ pages only while it is on SEQ */
static int page_hidden(uint32_t p)
{
    if (p == PG_ARP)
        return st.v[P_SQ_TYPE] != SQT_ARP;
    return (p == PG_SEQ || p == PG_SEQ2 || p == PG_SEQ3) && st.v[P_SQ_TYPE] != SQT_SEQ;
}

/* -------------------------------------------------------------- engine --- */
static float delay_s_now = 0.25f;        /* GLINT's delay time from the tempo (delay_sync) */

static uint8_t saved_v[NSOUND], saved_seq[SC_SEQ];   /* (the sound as last loaded or saved: below) */
static void drone_resolve(uint8_t *v);

static void engine_push(void)
{
    frasil_params_t b;
    seq_shift = 12 * (int32_t)st.v[P_GL_OCTAVE];   /* (NOTE shows the steps as they sound) */
    if (st.compare) {                    /* EDIT held: the scene as it was saved, the rest as now */
        static uint8_t v[NPARAM];
        uint32_t i;
        for (i = 0; i < NPARAM; i++)
            v[i] = i < NSOUND ? saved_v[i] : st.v[i];
        drone_resolve(v);
        params_to_engine(v, st.v[P_FREEZE], &b);
    } else {
        params_to_engine(st.v, st.v[P_FREEZE], &b);
    }
    b.glint.delay_s = delay_s_now;
    frasil_set(&b);
    st.engine_dirty = 0;
}

static void chord_on(const chord_t *c, uint32_t vel)
{
    uint32_t i;
    for (i = 0; i < c->k; i++)
        if (held_by[(uint8_t)c->n[i]]++ == 0)
            frasil_note_on(c->n[i], (float)vel * (1.0f / 127.0f));
}

static void chord_off(const chord_t *c)
{
    uint32_t i;
    for (i = 0; i < c->k; i++)
        if (held_by[(uint8_t)c->n[i]] && --held_by[(uint8_t)c->n[i]] == 0)
            frasil_note_off(c->n[i]);
}

static void chord_press(const chord_t *c, float p)
{
    uint32_t i;
    for (i = 0; i < c->k; i++)
        frasil_pressure(c->n[i], p);
}

static int bass_fold(int note)            /* C1 at the lowest: under it, nothing a speaker plays */
{
    while (note < 24)
        note += 12;
    return note > 127 ? 127 : note;
}

static void bass_play(int note, uint32_t vel)
{
    note = bass_fold(note);
    if (note != bass_note && bass_note >= 0)
        frasil_bass_pressure(0.0f);      /* another note: its own key's pressure, from nothing */
    bass_note = (int8_t)note;
    frasil_bass_on(bass_note, (float)vel * (1.0f / 127.0f));
}

static void bass_stop(void)
{
    if (bass_note >= 0)
        frasil_bass_off();
    bass_note = -1;
}

static void midi_glints_off(void);       /* (MIDI, below) */

static void release_drones(void)
{
    uint32_t k;
    frasil_all_off();
    for (k = 0; k < 128u; k++)
        held_by[k] = 0;
    for (k = 0; k < 4u; k++)
        lit[k] = mid_on[k] = sus[k] = 0;
    for (k = 0; k < NKEYS; k++)          /* keys still down hold nothing now */
        if (key[k].kind == K_LATCH || key[k].kind == K_HOLD)
            key[k].kind = K_NONE;
}

static void release_all(void)
{
    uint32_t k;
    release_drones();
    bass_stop();
    mb_n = 0;
    midi_glints_off();
    frasil_glint_off(-1);                /* (and every glint still ringing: keys', SPARKLE's) */
    for (k = 0; k < NKEYS; k++)
        if (key[k].kind == K_BASS)
            key[k].kind = K_NONE;
}

/* the sound as it was last loaded or saved: SAVE lights while the sound differs from it (back to it, it goes out) */
/* the drone's CUSTOM: its settings as they were last turned, kept while a preset plays. A sound as it is kept (the
 * scenes, the settings) has CUSTOM's settings for the drone's and the preset's number: a preset untouched is its table's */
static uint8_t custom[NDRONE];
static int8_t drone_of[NPARAM];          /* a setting's place in DRONE_P, -1: not the drone's (drone_init) */

static void drone_init(void)
{
    uint32_t i;
    for (i = 0; i < NPARAM; i++)
        drone_of[i] = -1;
    for (i = 0; i < NDRONE; i++)
        drone_of[DRONE_P[i]] = (int8_t)i;
}

static void drone_apply(uint32_t k)      /* preset k (0: CUSTOM) into the settings */
{
    uint32_t i;
    for (i = 0; i < NDRONE; i++)
        st.v[DRONE_P[i]] = k ? DRONE_PRESET[(k - 1u) % NDPRESET][i] : custom[i];
    st.engine_dirty = 1;
}

static void drone_take(void)             /* a sound just loaded into st.v: its drone's settings CUSTOM's, then its
                                          * preset's on top */
{
    uint32_t i;
    for (i = 0; i < NDRONE; i++)
        custom[i] = st.v[DRONE_P[i]];
    if (st.v[P_DR_PRESET])
        drone_apply(st.v[P_DR_PRESET]);
}

static uint8_t kept_v(uint32_t p) { return drone_of[p] >= 0 ? custom[drone_of[p]] : st.v[p]; }   /* as it is kept */

static void drone_resolve(uint8_t *v)    /* a sound as it is kept, its drone's preset (if any) onto its settings */
{
    uint32_t i;
    if (v[P_DR_PRESET])
        for (i = 0; i < NDRONE; i++)
            v[DRONE_P[i]] = DRONE_PRESET[(v[P_DR_PRESET] - 1u) % NDPRESET][i];
}

static void sound_mark(void)
{
    uint32_t i;
    for (i = 0; i < NSOUND; i++)
        saved_v[i] = kept_v(i);
    for (i = 0; i < SC_SEQ; i++)
        saved_seq[i] = seq[i];
    st.dirty = 0;
}

static int sound_changed(void)
{
    uint32_t i;
    for (i = 0; i < NSOUND; i++)
        if (kept_v(i) != saved_v[i])
            return 1;
    for (i = 0; i < SC_SEQ; i++)
        if (seq[i] != saved_seq[i])
            return 1;
    return 0;
}

/* the sequencer's settings that are its steps' (NOTE and ACCENT show the step on STEP), CLEAR, MODE */
static void seq_view(void)
{
    uint8_t b = seq[st.v[P_SQ_STEP] % SC_SEQ];
    st.v[P_SQ_NOTE] = b & 127u;
    st.v[P_SQ_ACC] = b >> 7;
}

static void seq_note_off(void)
{
    if (sq.sounding >= 0)
        frasil_glint_off(sq.sounding);
    sq.sounding = -1;
}

static void seq_begin(void)              /* from its first step, at the grid's next */
{
    seq_note_off();
    sq.run = 1;
    sq.start = arp.step_n + 1;
    sq.pos = sq.skip = -1;
}

static uint8_t seq_prev[SC_SEQ], seq_had;   /* the sequence before the last take or CLEAR (UNDO) */
static void seq_keep(void)
{
    memcpy(seq_prev, seq, sizeof seq_prev);
    seq_had = 1;
}

static void seq_set(int p, int32_t x)
{
    uint32_t c = st.v[P_SQ_STEP] % SC_SEQ;
    if (p == P_SQ_STEP)
        seq_view();
    else if (p == P_SQ_NOTE)
        seq[c] = (uint8_t)((seq[c] & SQ_ACC) | (uint32_t)x);
    else if (p == P_SQ_ACC)
        seq[c] = (uint8_t)((seq[c] & 127u) | (x ? SQ_ACC : 0u));
    else if (p == P_SQ_CLEAR && x >= 2) {   /* turned twice: every step a rest */
        seq_keep();
        memset(seq, 0, sizeof seq);
        st.v[P_SQ_CLEAR] = 0;
        seq_view();
        app_say("SEQUENCE", "CLEARED");
    } else if (p == P_SQ_NEW && x >= 2) {   /* turned twice: a new take, recording LIVE from rests */
        seq_keep();
        memset(seq, 0, sizeof seq);
        st.v[P_SQ_NEW] = 0;
        st.v[P_SQ_REC] = 2;
        if (!sq.run)
            seq_begin();
        seq_view();
        app_say("NEW TAKE", "RECORDING");
    } else if (p == P_SQ_UNDO && x >= 2) {   /* turned twice: the sequence before the last take or CLEAR (again:
                                              * back) */
        st.v[P_SQ_UNDO] = 0;
        if (seq_had) {
            uint8_t t[SC_SEQ];
            memcpy(t, seq, sizeof t);
            memcpy(seq, seq_prev, sizeof seq);
            memcpy(seq_prev, t, sizeof t);
            seq_view();
            app_say("SEQUENCE", "UNDONE");
        } else {
            app_say("UNDO", "NOTHING YET");
        }
    } else if (p == P_SQ_TYPE) {
        if (x == (int32_t)SQT_SEQ)
            seq_begin();
        else {
            seq_note_off();
            sq.run = 0;
            st.v[P_SQ_REC] = 0;          /* (no sequencer, nothing to record into) */
            st.rec_took = 0;
        }
        arp.paused = 0;
        if (page_hidden(st.page))        /* (its pages gone) */
            st.page = PG_GLINT;
    }
    st.dirty = (uint8_t)sound_changed();
}

/* a scene's MORPH under way: what each setting still lacks of the scene's at the start (morph_off goes from it to 0) */
static struct {
    uint8_t on;
    uint32_t t0, dur;
    float d[NSOUND];
    char name[11];
} mo;
static void scene_undo(void);

static void param_set(int p, int32_t x)
{
    x = clamp(x, 0, PD[p].max);
    if (st.v[p] == (uint8_t)x)
        return;
    st.v[p] = (uint8_t)x;
    if (p == P_GL_PRESET)                /* a preset: its sound into the GLINT parameters */
        glint_preset(st.v, (uint32_t)x);
    if (p == P_KEYS)                     /* to held notes, or back: the latched ones go */
        release_all();
    if (p == P_KB) {                     /* what the keys play (what sounds stays) */
        st.glint = x == 1;
        st.bass = x == 2;
    }
    if (p == P_FREEZE)
        st.engine_dirty = 1;
    if (p == P_SQ_TYPE || (p >= P_SQ_STEP && p <= P_SQ_CLEAR) || p == P_SQ_NEW || p == P_SQ_UNDO)
        seq_set(p, x);
    if (p == P_SC_UNDO && x >= 2) {
        st.v[P_SC_UNDO] = 0;
        scene_undo();
    }
    if (p < NSOUND)                      /* a hand on it: out of the scene's MORPH (a preset: all it sets) */
        morph_off[p] = mo.d[p] = 0.0f;
    if (p == P_DR_PRESET) {
        uint32_t i;
        for (i = 0; i < NDRONE; i++)
            morph_off[DRONE_P[i]] = mo.d[DRONE_P[i]] = 0.0f;
    } else if (p == P_GL_PRESET) {
        uint32_t i;
        for (i = P_GL_RATIO; i < P_GL_RATIO + 7u; i++)
            morph_off[i] = mo.d[i] = 0.0f;
        morph_off[P_GL_ATTACK] = mo.d[P_GL_ATTACK] = morph_off[P_GL_PING] = mo.d[P_GL_PING] = 0.0f;
        morph_off[P_GL_SUSTAIN] = mo.d[P_GL_SUSTAIN] = 0.0f;
    }
    if (p == P_SQ_REC && !x)
        st.rec_took = 0;
    if (p == P_DR_PRESET) {              /* a preset, or CUSTOM back */
        drone_apply((uint32_t)x);
    } else if (drone_of[p] >= 0) {       /* a drone's setting turned: CUSTOM from here, the rest as they were */
        if (st.v[P_DR_PRESET]) {
            uint32_t i;
            st.v[P_DR_PRESET] = 0;
            for (i = 0; i < NDRONE; i++)
                custom[i] = st.v[DRONE_P[i]];
        } else {
            custom[drone_of[p]] = (uint8_t)x;
        }
    }
    if (p < NSOUND) {
        st.dirty = (uint8_t)sound_changed();
        st.engine_dirty = 1;
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    frasil_render(lr, n, (float)master_q12 * (1.0f / 4096.0f));
}

void app_panic(void) { frasil_panic(); }

/* the render ran over 90 % of its time (audio interrupt): one step lighter at once; the main loop gives the steps
 * back as the load allows (load_watch) */
void app_overload(void) { frasil_shed(frasil_shed_level() + 1); }

static void load_watch(void)
{
    static uint32_t calm_since;
    uint32_t now = plat_ms();
    if (!frasil_shed_level() || plat_cpu_pct() > 60u) {
        calm_since = now;
        return;
    }
    if (now - calm_since > 4000u) {
        frasil_shed(frasil_shed_level() - 1);
        calm_since = now;
    }
}

/* ------------------------------------------------------- saving, scenes --- */
static void arp_stop(void);              /* (the arpeggiator, below) */

/* what plays, into a scene: the latched chords (by root, lowest first) and the arpeggio's notes */
static void scene_notes_take(scene_t *sc)
{
    uint32_t n, i;
    sc->nch = 0;
    for (n = 0; n < 128u && sc->nch < SC_CHORDS; n++)
        if (bit_get(lit, (int)n)) {
            int8_t *c = sc->ch[sc->nch++];
            c[0] = (int8_t)n;
            for (i = 0; i < 4u; i++)
                c[1 + i] = i < lat[n].k ? lat[n].n[i] : -1;
        }
    sc->narp = arp.n > SC_ARP ? (uint8_t)SC_ARP : arp.n;
    for (i = 0; i < sc->narp; i++)
        sc->arp[i] = arp.held[i];
    sc->bass = st.v[P_KEYS] ? -1 : bass_note;   /* held notes are not kept, as the drones' are not */
    for (i = 0; i < SC_SEQ; i++)
        sc->seq[i] = seq[i];
}

/* a scene's notes, played (everything let go before): the drones latched again, the arpeggio (latched) on its
 * notes */
static void scene_notes_play(const scene_t *sc)
{
    uint32_t i, j;
    for (i = 0; i < sc->nch && i < SC_CHORDS; i++) {
        int root = sc->ch[i][0];
        chord_t c;
        if (root < 0)
            continue;
        c.k = 0;
        for (j = 0; j < 4u; j++)
            if (sc->ch[i][1 + j] >= 0)
                c.n[c.k++] = sc->ch[i][1 + j];
        lat[root] = c;
        bit_put(lit, root, 1);
        chord_on(&c, 100);
    }
    arp.n = 0;
    for (i = 0; i < sc->narp && i < 16u; i++) {
        arp.held[i] = sc->arp[i];
        arp.prs[i] = 0.0f;
    }
    arp.n = (uint8_t)i;
    arp.dirty = 1;                       /* from the next step */
    if (arp.n) {
        arp.latch = 1;
        arp.vel = 100;
        arp.paused = 0;
    }
    if (sc->bass >= 0)
        bass_play(sc->bass, 100);
}

static void scene_get(uint32_t s, scene_t *out)
{
    if (scn.used >> s & 1u) {
        *out = scn.s[s];
        return;
    }
    *out = FACTORY[s < NFACTORY ? s : 0];
    glint_preset(out->v, out->v[P_GL_PRESET]);   /* a factory scene names its GLINT preset */
    arp_defaults(out->v);
    later_defaults(out->v, P_BS_DRIVE);
    if (s >= NFACTORY) {                         /* an empty slot: the first scene, named by its number */
        out->name[0] = 0;
        text_cat(out->name, "SCENE ");
        int_text((int32_t)s + 1, out->name + 6);
    }
}

static void scene_load(uint32_t s)
{
    scene_t sc;
    uint32_t i;
    scene_get(s, &sc);
    for (i = 0; i < NSOUND; i++)
        st.v[i] = sc.v[i] > PD[i].max ? PD[i].max : sc.v[i];
    drone_take();
    for (i = 0; i < SC_SEQ; i++)
        seq[i] = sc.seq[i];
    seq_view();
    if (st.v[P_SQ_TYPE] == SQT_SEQ)      /* its sequence from the top */
        seq_begin();
    else {
        seq_note_off();
        sq.run = 0;
        st.v[P_SQ_REC] = 0;
        st.rec_took = 0;
    }
    arp.paused = 0;
    if (page_hidden(st.page))
        st.page = PG_GLINT;
    st.scene = (uint8_t)s;
    sound_mark();
    if (st.loading_morph)                /* (scene_go sets the glide first: no step to the new sound) */
        st.engine_dirty = 1;
    else
        engine_push();
    release_all();                       /* a scene starts clean: every note let go, the arpeggio stopped */
    arp.n = arp.pn = 0;
    arp.dirty = 0;
    arp_stop();
    if (sc.nch || sc.narp || sc.bass >= 0)   /* then what it plays, if anything */
        scene_notes_play(&sc);
}

static int store_all(void)
{
    uint32_t i;
    set.magic = SET_MAGIC;
    for (i = 0; i < NSOUND; i++)
        set.v[i] = kept_v(i);
    set.octave = st.octave;
    set.keys = st.v[P_KEYS];
    set.channel = st.v[P_CANAL];
    set.ch_glint = st.v[P_CH_GLINT];
    set.ch_bass = st.v[P_CH_BASS];
    set.scene = st.scene;
    set.tempo = st.v[P_TEMPO];
    for (i = 0; i < SC_SEQ; i++)
        set.seq[i] = seq[i];
    set.metro_off = !st.metro;
    set.morph = st.v[P_MORPH];
    set.keep = st.v[P_KEEP];
    return plat_store_save(OBJ_SETTINGS, &set, sizeof set);
}

static struct {                         /* the slot STORE wrote last, as it was (SCENES' UNDO) */
    uint8_t valid, slot, used;
    scene_t sc;
} undo;

static scene_t rst;                      /* what PLAY held let go, for RESTORE (its notes alone) */
static uint8_t rst_valid, rst_seq;

static void scene_undo(void)
{
    if (!undo.valid) {
        app_say("UNDO", "NOTHING YET");
        return;
    }
    scn.s[undo.slot] = undo.sc;
    scn.used = (uint16_t)((scn.used & ~(1u << undo.slot)) | (uint32_t)undo.used << undo.slot);
    undo.valid = 0;
    if (plat_store_save(OBJ_SCENES, &scn, sizeof scn) == 0) {
        scene_t sc;
        uint32_t i;
        scene_get(undo.slot, &sc);
        if (undo.slot == st.scene) {     /* (the one playing: CHANGED, COMPARE against it again) */
            for (i = 0; i < NSOUND; i++)
                saved_v[i] = sc.v[i];
            for (i = 0; i < SC_SEQ; i++)
                saved_seq[i] = sc.seq[i];
            st.dirty = (uint8_t)sound_changed();
        }
        app_say("UNDONE", sc.name);
    } else {
        app_say("UNDO", "FAILED");
    }
}

/* MORPH, KEEP, the metronome, the CCs learned: kept at once (the sound and the rest of the record as SAVE left them;
 * never saved yet: all of it) */
static void prefs_save(void)
{
    if (set.magic != SET_MAGIC) {
        store_all();
        return;
    }
    set.metro_off = !st.metro;
    set.morph = st.v[P_MORPH];
    set.keep = st.v[P_KEEP];
    plat_store_save(OBJ_SETTINGS, &set, sizeof set);
}

static void scene_save(uint32_t s)
{
    scene_t sc;
    uint32_t i;
    char say2[16];
    scene_get(s, &sc);                   /* keeps its name */
    undo.valid = 1;                      /* (the slot as it was, for UNDO) */
    undo.slot = (uint8_t)s;
    undo.used = (uint8_t)(scn.used >> s & 1u);
    undo.sc = scn.s[s];
    if (!(scn.used >> s & 1u) && s >= NFACTORY && st.v[P_DR_PRESET]) {   /* an empty slot: the drone's preset's name */
        sc.name[0] = 0;
        text_cat(sc.name, DRONE_NAME[st.v[P_DR_PRESET] % (NDPRESET + 1u)]);
    }
    for (i = 0; i < NSOUND; i++)
        sc.v[i] = kept_v(i);
    scene_notes_take(&sc);
    scn.magic = SCN_MAGIC;
    scn.s[s] = sc;
    scn.used |= (uint16_t)(1u << s);
    st.scene = (uint8_t)s;
    if (plat_store_save(OBJ_SCENES, &scn, sizeof scn) == 0 && store_all() == 0) {
        sound_mark();
        say2[0] = (char)('0' + (s + 1u) / 10u);   /* "03 ABYSS" */
        say2[1] = (char)('0' + (s + 1u) % 10u);
        say2[2] = ' ';
        say2[3] = 0;
        text_cat(say2, sc.name);
        app_say("STORED", say2);
    } else {
        app_say("SAVE", "FAILED");
    }
}

/* a scene, as SCENES' MORPH and KEEP have it: KEEP SCENE lets everything go and plays the scene's notes (as
 * scene_load); KEEP PLAYING leaves what plays (the drones, the bass, GLINT's arpeggio and sequence, with its TYPE and
 * the settings that play it) and changes the sound alone. MORPH, CUT or a time: the sound glides there, the settings
 * of a few choices (presets, MODEL, ROOT, TARGET ...) at once, each a hand turns out of it */
static const uint8_t KEEP_P[8] = {P_SQ_TYPE, P_ARP_MODE, P_ARP_RATE, P_ARP_RANGE, P_ARP_CHANCE, P_SQ_LEN, P_SQ_GATE,
                                  P_SQ_TRANS};
static int morphs(uint32_t p)            /* a setting that glides (not a choice among a few, not a name) */
{
    uint32_t f = PD[p].fmt;
    return PD[p].max > 16u && f != F_LTGT && f != F_DPRESET && f != F_GPRESET && f != F_LRATE && f != F_SQLEN &&
           f != F_DTIME && f != F_SQTRANS && f != F_CENTS;
}

static void scene_go(uint32_t s)
{
    static const uint32_t MORPH_MS[9] = {0, 1000, 2000, 4000, 8000, 15000, 30000, 60000, 120000};
    static float from[NSOUND];
    uint32_t i, dur = MORPH_MS[st.v[P_MORPH] % 9u];
    scene_t sc;
    scene_get(s, &sc);
    for (i = 0; i < NSOUND; i++)         /* (where it is now, a MORPH under way too) */
        from[i] = (float)st.v[i] + morph_off[i];
    if (st.v[P_KEEP]) {
        uint8_t kept[sizeof KEEP_P];
        for (i = 0; i < sizeof KEEP_P; i++)
            kept[i] = st.v[KEEP_P[i]];
        for (i = 0; i < NSOUND; i++)
            st.v[i] = sc.v[i] > PD[i].max ? PD[i].max : sc.v[i];
        drone_take();
        st.scene = (uint8_t)s;
        sound_mark();                    /* (the scene as saved: its own sequence) */
        for (i = 0; i < SC_SEQ; i++)
            saved_seq[i] = sc.seq[i];
        for (i = 0; i < sizeof KEEP_P; i++)
            st.v[KEEP_P[i]] = kept[i];
        st.dirty = (uint8_t)sound_changed();
        st.engine_dirty = 1;
    } else {
        st.loading_morph = dur != 0u;
        scene_load(s);
        st.loading_morph = 0;
    }
    mo.on = dur != 0u;
    for (i = 0; i < NSOUND; i++) {
        mo.d[i] = dur && morphs(i) ? from[i] - (float)st.v[i] : 0.0f;
        morph_off[i] = mo.d[i];
    }
    st.engine_dirty = 1;
    if (dur) {
        mo.t0 = plat_ms();
        mo.dur = dur;
        for (i = 0; i < 10u && sc.name[i]; i++)
            mo.name[i] = sc.name[i];
        mo.name[i] = 0;
    } else {
        app_say("LOADED", sc.name);
    }
}

static void morph_run(void)              /* a hundred times a second while a MORPH is under way */
{
    static uint32_t at;
    uint32_t now = plat_ms(), i;
    float k, g;
    if (!mo.on || now - at < 10u)
        return;
    at = now;
    k = (float)(now - mo.t0) / (float)mo.dur;
    if (k >= 1.0f) {
        mo.on = 0;
        for (i = 0; i < NSOUND; i++)
            morph_off[i] = mo.d[i] = 0.0f;
    } else {
        g = 1.0f - k * k * (3.0f - 2.0f * k);
        for (i = 0; i < NSOUND; i++)
            morph_off[i] = mo.d[i] * g;
    }
    st.engine_dirty = 1;
}

/* an earlier layout's sound made over: its first nv parameters, the rest at their defaults (and MODEL, where FRSW
 * kept SPACE; before TYPE, MODE's OFF, five and SEQ in TYPE and MODE, the delay's MODE in its TIME) */
static void sound_over(uint8_t *d, const uint8_t *v, uint32_t nv)
{
    uint32_t i;
    for (i = 0; i < nv; i++)
        d[i] = v[i];
    later_defaults(d, nv);
    if (nv == 64u)
        d[P_BS_MODEL] = PD[P_BS_MODEL].def;
    if (nv == 94u) {                     /* the second pages' first layout: what has changed since at its default (TYPE
                                          * where BLOOM was; EVOLVE, BEAT, PEAK and SHAPE, RESONANCE going further) */
        static const uint8_t REDONE[7] = {P_CH_SHAPE, P_CH_RESO, P_CH_TYPE, P_PA_EVOLVE, P_PA_BEAT, P_PA_FORMANT,
                                          P_PA_PEAK};
        uint32_t k;
        for (k = 0; k < sizeof REDONE; k++)
            d[REDONE[k]] = PD[REDONE[k]].def;
    }
    if (nv <= P_SQ_TYPE) {
        uint32_t m = v[P_ARP_MODE];
        d[P_SQ_TYPE] = (uint8_t)(m == 6u ? SQT_SEQ : m ? SQT_ARP : 0u);
        d[P_ARP_MODE] = (uint8_t)(m >= 1u && m <= 5u ? m - 1u : 0u);
        d[P_GD_TIME] = (uint8_t)(v[P_GD_TIME] % 12u + (v[P_GD_MODE] ? 12u : 0u));
    }
}

/* before TYPE, the arpeggio's and the sequence's notes were kept with GLINT's OCTAVE on them: without it now */
static int8_t note_down(int n, const uint8_t *v)
{
    n -= 12 * (int)v[P_GL_OCTAVE];
    return (int8_t)(n < 1 ? 1 : n);
}

static void seq_down(uint8_t *s, const uint8_t *v)
{
    uint32_t j;
    for (j = 0; j < SC_SEQ; j++)
        if ((s[j] & 127u) != SQ_REST && (s[j] & 127u) != SQ_TIE)
            s[j] = (uint8_t)((s[j] & SQ_ACC) | (uint8_t)note_down(s[j] & 127, v));
}

static void scenes_over(void)             /* the scenes in an earlier layout, if that is what the store holds */
{
    int n = plat_store_load(OBJ_SCENES, raw, sizeof raw);
    uint32_t k, s, j;
    for (k = 0; k < sizeof OLD_SCN / sizeof OLD_SCN[0]; k++) {
        const layout_t *l = &OLD_SCN[k];
        uint32_t rs = scn_bytes(l), nv = l->nv;
        if (n != (int)(8u + NSCENES * rs) || rd32(raw) != l->magic)
            continue;
        for (s = 0; s < NSCENES; s++) {
            const uint8_t *o = raw + 8u + s * rs;
            scene_t *d = &scn.s[s];
            for (j = 0; j < sizeof d->name; j++)
                d->name[j] = (char)o[j];
            sound_over(d->v, o + 10, nv);
            d->nch = o[10u + nv];
            d->narp = o[11u + nv];
            for (j = 0; j < SC_CHORDS * 5u; j++)
                d->ch[j / 5u][j % 5u] = (int8_t)o[12u + nv + j];
            for (j = 0; j < SC_ARP; j++)
                d->arp[j] = j < d->narp ? note_down((int8_t)o[12u + nv + SC_CHORDS * 5u + j], d->v)
                                        : (int8_t)o[12u + nv + SC_CHORDS * 5u + j];
            d->bass = l->extra ? (int8_t)o[12u + nv + SC_CHORDS * 5u + SC_ARP] : -1;
            for (j = 0; j < SC_SEQ; j++)  /* (none, before it had one) */
                d->seq[j] = seq_bytes(l) ? o[13u + nv + SC_CHORDS * 5u + SC_ARP + j] : SQ_REST;
            seq_down(d->seq, d->v);
        }
        scn.used = (uint16_t)(raw[4] | raw[5] << 8);
        scn.magic = SCN_MAGIC;
        return;
    }
}

static void settings_over(void)           /* the settings in an earlier layout, the same */
{
    int n = plat_store_load(OBJ_SETTINGS, raw, sizeof raw);
    uint32_t k, j;
    for (k = 0; k < sizeof OLD_SET / sizeof OLD_SET[0]; k++) {
        const layout_t *l = &OLD_SET[k];
        const uint8_t *o = raw + 4u + l->nv;
        if (n != (int)set_bytes(l) || rd32(raw) != l->magic)
            continue;
        sound_over(set.v, raw + 4, l->nv);
        set.octave = (int8_t)o[0];
        set.keys = o[1];
        set.channel = l->extra || o[2] ? o[2] : 1u;   /* one channel for all, OMNI by default: the drones' 1 now */
        set.scene = o[3];
        set.tempo = o[4];
        set.ch_glint = l->extra ? o[5] : PD[P_CH_GLINT].def;
        set.ch_bass = l->extra ? o[6] : PD[P_CH_BASS].def;
        for (j = 0; j < SC_SEQ; j++)
            set.seq[j] = seq_bytes(l) ? o[7u + j] : SQ_REST;
        if (l->nv <= P_SQ_TYPE)
            seq_down(set.seq, set.v);
        set.metro_off = l->magic == 0x46535246u ? o[7u + SC_SEQ] : 0u;   /* (FRSF had it) */
        if (!l->extra && set.ch_glint == set.channel)   /* one channel for all was N: GLINT and BASS not on it too */
            set.ch_glint = 0;
        if (!l->extra && set.ch_bass == set.channel)
            set.ch_bass = 0;
        set.magic = SET_MAGIC;
        return;
    }
}

void app_init(void)
{
    uint32_t i;
    frasil_init();
    drone_init();
    st.octave = OCT_DEF;
    st.metro = 1;
    rst_valid = seq_had = 0;             /* (nothing to restore or undo yet; the host's reboot keeps statics) */
    undo.valid = 0;
    mo.on = 0;
    memset(morph_off, 0, sizeof morph_off);
    arp.latch = 1;                       /* the arpeggio latched from the start, empty */
    arp.n = 0;
    bass_note = -1;
    for (i = 0; i < NPARAM; i++)
        st.v[i] = PD[i].def;
    if (plat_store_load(OBJ_SCENES, &scn, sizeof scn) != (int)sizeof scn || scn.magic != SCN_MAGIC) {
        scn.used = 0;
        scenes_over();
        scn.magic = SCN_MAGIC;
    }
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC) {
        memset(&set, 0, sizeof set);     /* (nothing learned, the defaults, whatever was read) */
        settings_over();
    }
    if (set.magic == SET_MAGIC) {
        for (i = 0; i < NSOUND; i++)
            st.v[i] = set.v[i] > PD[i].max ? PD[i].max : set.v[i];
        drone_take();
        st.octave = (int8_t)clamp(set.octave, -3, 2);
        st.metro = !set.metro_off;
        st.v[P_MORPH] = set.morph > PD[P_MORPH].max ? 0 : set.morph;
        st.v[P_KEEP] = set.keep ? 1 : 0;
        st.v[P_KEYS] = set.keys ? 1 : 0;
        st.v[P_CANAL] = set.channel > 17u ? PD[P_CANAL].def : set.channel;
        st.v[P_CH_GLINT] = set.ch_glint > 17u ? PD[P_CH_GLINT].def : set.ch_glint;
        st.v[P_CH_BASS] = set.ch_bass > 17u ? PD[P_CH_BASS].def : set.ch_bass;
        st.scene = set.scene % NSCENES;
        st.v[P_TEMPO] = set.tempo > PD[P_TEMPO].max ? PD[P_TEMPO].def : set.tempo;
        for (i = 0; i < SC_SEQ; i++)
            seq[i] = set.seq[i];
        seq_view();
        if (st.v[P_SQ_TYPE] == SQT_SEQ)
            seq_begin();
    } else {
        scene_load(0);
    }
    st.pick = st.scene;
    st.page = PG_SON;
    sound_mark();                        /* the sound saved last: nothing changed yet */
    engine_push();
}

/* -------------------------------------------------------- the arpeggiator --- */
/* GLINT's keys held, played one at a time at RATE (MIDI clock when one comes in, else TEMPO), over RANGE octaves,
 * each step played or not as CHANCE says. In the main loop: its timing is the loop's (a millisecond or two). */


static uint32_t arp_rand(void)
{
    arp.rng ^= arp.rng << 13;
    arp.rng ^= arp.rng >> 17;
    arp.rng ^= arp.rng << 5;
    return arp.rng;
}

static void arp_add(int note, uint32_t vel)
{
    uint32_t i, j;
    if (arp.n >= 16u)
        return;
    for (i = 0; i < arp.n; i++)
        if (arp.held[i] == note)
            return;
    for (i = 0; i < arp.n && arp.held[i] < note; i++)
        ;
    for (j = arp.n; j > i; j--) {
        arp.held[j] = arp.held[j - 1];
        arp.prs[j] = arp.prs[j - 1];
    }
    arp.held[i] = (int8_t)note;
    arp.prs[i] = 0.0f;
    arp.n++;
    arp.dirty = 1;                       /* it plays from the next step */
    arp.vel = (uint8_t)vel;
}

static void arp_remove(int note)
{
    uint32_t i, j;
    for (i = 0; i < arp.n; i++)
        if (arp.held[i] == note) {
            for (j = i; j + 1u < arp.n; j++) {
                arp.held[j] = arp.held[j + 1u];
                arp.prs[j] = arp.prs[j + 1u];
            }
            arp.n--;
            arp.dirty = 1;
            return;
        }
}

static void arp_stop(void)
{
    if (arp.sounding >= 0)
        frasil_glint_off(arp.sounding);
    arp.sounding = -1;
}

/* GLINT's keys down now, but key `except` */
static int glint_keys_down(uint32_t except)
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        if (k != except && key[k].kind == K_GLINT)
            return 1;
    return 0;
}

/* the latch let go: the arpeggio keeps only the notes whose keys are still down */
static void arp_keep_held(void)
{
    uint32_t i = 0, k;
    while (i < arp.n) {
        int down = 0;
        for (k = 0; k < NKEYS; k++)
            if (key[k].kind == K_GLINT && key[k].gbase == arp.held[i])
                down = 1;
        if (down)
            i++;
        else
            arp_remove(arp.held[i]);
    }
}

/* GLINT's delay on the beat: the MIDI clock's tempo when one comes in, else TEMPO; halved while too long */
static void delay_sync(void)
{
    float bpm = plat_ms() - arp.clk_at < 500u && arp.clk_ms > 1.0f ? 2500.0f / arp.clk_ms : (float)(st.v[P_TEMPO] + 30u);
    float s = (float)GD_TICKS[st.v[P_GD_TIME] % 12u] * 2.5f / bpm;
    while (s > GLINT_DELAY_MAX)
        s *= 0.5f;
    if (fm_fabsf(s - delay_s_now) > delay_s_now * 0.003f) {
        delay_s_now = s;
        st.engine_dirty = 1;
    }
}

/* PUMP on the arpeggiator's grid (MIDI clock, else TEMPO): where the beat is in RATE's period, and its pace */
static float grid_now(void);

static void pump_sync(void)
{
    uint32_t r = st.v[P_PUMP_RATE] % 7u;
    float per, ph, ms_tick;
    if (!r) {
        frasil_pump(0.0f, 0.0f);
        return;
    }
    ms_tick = plat_ms() - arp.clk_at < 500u && arp.clk_ms > 1.0f ? arp.clk_ms : 2500.0f / (float)(st.v[P_TEMPO] + 30u);
    per = (float)PUMP_TICKS[r];
    ph = grid_now() / per;
    frasil_pump(ph - fm_floorf(ph), (1000.0f / 44100.0f) / (ms_tick * per));
}

/* the grid now: MIDI clocks since the start (arp.tick, from the clock or from TEMPO); clocked, the grid moves in whole
 * clocks, and between them this is how far into the next one */
static float grid_now(void)
{
    float g = arp.tick;
    if (plat_ms() - arp.clk_at < 500u && arp.clk_ms > 1.0f) {
        float f = (float)(plat_ms() - arp.clk_at) / arp.clk_ms;
        g += f > 1.0f ? 1.0f : f;
    }
    return g;
}

/* ------------------------------------------------------------------ the LFOs --- */
/* LFO 1, 2 and 3, free running while on (DEPTH over 0 and a TARGET set): at RATE's own pace, or on the grid (MIDI clock
 * or TEMPO; MIDI Start puts them back to the top of their turn). SHAPE's value, as POLARITY has it, times DEPTH times a
 * setting's whole range, goes onto each TARGET (mod: params_to_engine adds it), a hundred times a second; the engine
 * glides between. RANDOM and DRIFT take a new value each turn (the same for a turn number, from a hash) */
static struct {
    float ph;                            /* where in its turn, 0..1 */
    uint32_t cyc;                        /* the turns gone by */
    float out;                           /* SHAPE's value now, -1..1 (the screen) */
    float peak;                          /* the largest offset since the tests asked (0..1) */
    uint8_t on;
} lfo[NLFO];
static uint32_t lfo_at;
static float lfo_grid;                   /* the grid last time */
static uint8_t lfo_any, lfo_restart;

static float lfo_rnd(uint32_t i, uint32_t c)   /* RANDOM's value for turn c of LFO i, -1..1 */
{
    uint32_t h = (c + 1u) * 2654435761u ^ (i + 1u) * 0x9E3779B9u;
    h ^= h >> 15;
    h *= 0x85EBCA77u;
    h ^= h >> 13;
    return (float)(h >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

static float lfo_shape(uint32_t s, float x, uint32_t i, uint32_t c)   /* SHAPE s at x (0..1) of turn c, -1..1 */
{
    switch (s) {
    case 0:
        return fm_sinf(6.2831853f * x);
    case 1:                              /* TRIANGLE: up from 0, down, back */
        return x < 0.25f ? 4.0f * x : x < 0.75f ? 2.0f - 4.0f * x : 4.0f * x - 4.0f;
    case 2:                              /* SAW: up */
        return 2.0f * x - 1.0f;
    case 3:                              /* RAMP: down */
        return 1.0f - 2.0f * x;
    case 4:
        return x < 0.5f ? 1.0f : -1.0f;
    case 5:                              /* RANDOM: a value a turn */
        return lfo_rnd(i, c);
    default: {                           /* DRIFT: from this turn's value to the next, eased */
        float a = lfo_rnd(i, c), b = lfo_rnd(i, c + 1u);
        return a + (b - a) * x * x * (3.0f - 2.0f * x);
    }
    }
}

static void lfo_run(void)
{
    uint32_t now = plat_ms(), i, k;
    float dt, g, dg;
    int any = 0;
    if (now - lfo_at < 10u)
        return;
    dt = (float)(now - lfo_at) * 0.001f;
    lfo_at = now;
    if (dt > 0.1f)
        dt = 0.1f;
    g = grid_now();
    dg = g - lfo_grid;
    if (dg < -4800.0f)                   /* (the grid wraps at 9600) */
        dg += 9600.0f;
    else if (dg < 0.0f)
        dg = 0.0f;
    lfo_grid = g;
    memset(mod, 0, sizeof mod);
    for (i = 0; i < NLFO; i++) {
        const uint8_t *L = &st.v[LFO_BASE[i]];
        float depth = (float)L[2] * (1.0f / 127.0f), w, off;
        uint32_t r = L[1], used = 0, n;
        for (k = 0; k < 4u; k++)
            used |= L[4u + k];
        lfo[i].on = depth > 0.0f && used;
        if (!lfo[i].on)
            continue;
        any = 1;
        if (lfo_restart && r >= LFO_NFREE) {
            lfo[i].ph = 0.0f;
            lfo[i].cyc = 0;
        }
        lfo[i].ph += r < LFO_NFREE ? dt * lfo_hz(r) : dg / (float)LFO_TICKS[(r - LFO_NFREE) % 12u];
        n = (uint32_t)lfo[i].ph;
        lfo[i].ph -= (float)n;
        lfo[i].cyc += n;
        w = lfo_shape(L[0] % 7u, lfo[i].ph, i, lfo[i].cyc);
        lfo[i].out = w;
        off = depth * (L[3] == 1u ? 0.5f * (w + 1.0f) : L[3] == 2u ? -0.5f * (w + 1.0f) : w);   /* BIPOLAR, UP, DOWN */
        if (fm_fabsf(off) > lfo[i].peak)
            lfo[i].peak = fm_fabsf(off);
        for (k = 0; k < 4u; k++)
            if (L[4u + k]) {
                uint32_t p = LFO_TGT[L[4u + k] % NLFO_TGT];
                mod[p] += off * (float)PD[p].max;
            }
    }
    lfo_restart = 0;
    if (any || lfo_any)                  /* (once more after the last goes off: back to the settings as they are) */
        st.engine_dirty = 1;
    lfo_any = (uint8_t)any;
}

/* CHANCE as it is now: with what the LFOs add to it (it is the arpeggiator's, not the engine's) */
static uint32_t arp_chance(void)
{
    float c = (float)st.v[P_ARP_CHANCE] + mod[P_ARP_CHANCE];
    return c <= 0.0f ? 0u : c >= 127.0f ? 127u : (uint32_t)(c + 0.5f);
}

/* LENGTH as it plays now, 1 .. SC_SEQ: with what the LFOs add to it */
static uint32_t seq_len(void)
{
    float c = (float)st.v[P_SQ_LEN] + mod[P_SQ_LEN];
    return c <= 0.0f ? 1u : c >= (float)(SC_SEQ - 1u) ? SC_SEQ : (uint32_t)(c + 0.5f) + 1u;
}

/* a step's note as it plays: TRANSPOSE's steps along the scale, GLINT's OCTAVE up */
static int seq_note(uint32_t b)
{
    int n = scale_shift((int)(b & 127u), (int)st.v[P_SQ_TRANS] - 14, st.v) + 12 * (int)st.v[P_GL_OCTAVE];
    return n > 127 ? 127 : n;
}

/* the sequencer on the grid: its note off at GATE's end (a TIE on the next step holds it on), the next step's on */
static void seq_run(uint32_t now, int32_t stepn, uint32_t period)
{
    uint32_t len = seq_len(), b, nb;
    int32_t pos;
    uint32_t gate = (uint32_t)((float)period * (0.05f + 0.95f * (float)st.v[P_SQ_GATE] * (1.0f / 127.0f)));
    if (sq.sounding >= 0 && (int32_t)(now - sq.off_ms) >= 0)
        seq_note_off();
    if (sq.restart) {                    /* MIDI Start: its first step on the grid's top */
        sq.restart = 0;
        sq.run = 1;
        sq.start = stepn;
        sq.pos = sq.skip = -1;
        arp.step_n = stepn - 1;
    }
    if (!sq.run || stepn == arp.step_n)
        return;
    pos = (stepn - sq.start) % (int32_t)len;
    if (pos < 0)
        pos += (int32_t)len;
    sq.pos = pos;
    b = seq[pos];
    nb = seq[(uint32_t)(pos + 1) % len] & 127u;
    if ((b & 127u) == SQ_TIE) {          /* held on through this step */
        if (sq.sounding >= 0)
            sq.off_ms = now + (nb == SQ_TIE ? 2u * period : gate);
        return;
    }
    seq_note_off();
    if (pos == sq.skip) {                /* LIVE wrote it a moment ago, and it sounded then */
        sq.skip = -1;
        return;
    }
    if ((b & 127u) == SQ_REST || arp_rand() % 127u >= arp_chance())
        return;
    {
        int n = seq_note(b);
        float v = (b & SQ_ACC ? 1.0f : 0.68f) * (0.92f + 0.08f * (float)(arp_rand() & 255u) * (1.0f / 255.0f));
        frasil_glint_on(n, v);
        sq.sounding = (int16_t)n;
        sq.off_ms = now + (nb == SQ_TIE ? 2u * period : gate);
        sq.steps++;
    }
}

/* a GLINT key or note while the sequencer has them: RECORD OFF, the sequence transposed from its first note to this
 * one along the scale; STEP, written at STEP, then on to the next (it sounds while down); LIVE, written on the step
 * nearest the playhead (it sounds as it is played; ahead of the playhead, that step skips this time round) */
static void seq_key(int n, int sound, uint32_t vel)   /* n: without GLINT's OCTAVE (kept so); sound: with it */
{
    uint32_t len = st.v[P_SQ_LEN] + 1u, i;
    uint8_t b = (uint8_t)((n < 1 ? 1 : n > 126 ? 126 : n) | (vel >= 100u ? SQ_ACC : 0u));
    if (st.v[P_SQ_REC] == 1u) {
        uint32_t c = st.v[P_SQ_STEP] % len;
        seq[c] = b;
        st.v[P_SQ_STEP] = (uint8_t)((c + 1u) % len);
        seq_view();
        frasil_glint_on(sound, (float)vel * (1.0f / 127.0f));
    } else if (st.v[P_SQ_REC] == 2u) {
        float g = grid_now() / (float)ARP_TICKS[st.v[P_ARP_RATE] % 10u] - (float)sq.start, f;
        int32_t q;
        len = seq_len();                 /* (as it plays now) */
        f = g - fm_floorf(g);
        q = (int32_t)fm_floorf(g) + (f >= 0.5f);
        q %= (int32_t)len;
        if (q < 0)
            q += (int32_t)len;
        seq[q] = b;
        if (f >= 0.5f)
            sq.skip = q;
        seq_view();
        frasil_glint_on(sound, (float)vel * (1.0f / 127.0f));
    } else {
        for (i = 0; i < len; i++)        /* from the first note (not a rest, not a tie) */
            if ((seq[i] & 127u) != SQ_REST && (seq[i] & 127u) != SQ_TIE)
                break;
        if (i < len)
            st.v[P_SQ_TRANS] = (uint8_t)clamp(14 + scale_steps(seq[i] & 127, n, st.v), 0, 28);
    }
    st.dirty = (uint8_t)sound_changed();
}

static void arp_run(void)
{
    uint32_t now = plat_ms(), ticks = ARP_TICKS[st.v[P_ARP_RATE] % 10u], dt = now - arp.last_ms, i;
    float ms_tick = now - arp.clk_at < 500u && arp.clk_ms > 1.0f ? arp.clk_ms
                                                                 : 2500.0f / (float)(st.v[P_TEMPO] + 30u);
    uint32_t period = (uint32_t)((float)ticks * ms_tick);   /* a step's length, for the gate below */
    int32_t count, stepn;
    arp.last_ms = now;
    /* the grid moves on: by the clocks that came in, else by the time at TEMPO */
    if (now - arp.clk_at < 500u)
        arp.tick += (float)arp.clk_new;
    else
        arp.tick += (float)(dt > 1000u ? 1000u : dt) / ms_tick;
    arp.clk_new = 0;
    if (arp.tick >= 9600.0f)
        arp.tick -= 9600.0f;
    stepn = (int32_t)(arp.tick / (float)ticks);
    if (arp.sounding >= 0 && (int32_t)(now - arp.off_ms) >= 0)
        arp_stop();
    if (st.v[P_SQ_TYPE] == SQT_SEQ) {    /* the sequencer instead: the arpeggio's notes go */
        arp_stop();
        arp.n = arp.pn = 0;
        arp.dirty = 0;
        seq_run(now, stepn, period);
        arp.step_n = stepn;
        return;
    }
    if (st.v[P_SQ_TYPE] == SQT_ARP && arp.paused) {   /* waiting: silent, its notes kept */
        arp_stop();
        arp.step_n = stepn;
        return;
    }
    if (st.v[P_SQ_TYPE] != SQT_ARP) {
        arp_stop();
        arp.paused = 0;
        arp.n = arp.pn = 0;              /* the arpeggiator off: its notes (latched too) go */
        arp.dirty = 0;
        arp.step_n = stepn;
        return;
    }
    if (stepn == arp.step_n)             /* between two steps of the grid: nothing changes */
        return;
    arp.step_n = stepn;
    if (arp.dirty) {                     /* on the step: what the keys ask for plays from here, its pattern from the top */
        arp.dirty = 0;
        for (i = 0; i < arp.n; i++) {
            arp.play[i] = arp.held[i];
            arp.pprs[i] = arp.prs[i];
        }
        arp.pn = arp.n;
        arp.idx = -1;
        arp.dir = 1;
    }
    if (!arp.pn) {
        arp_stop();
        return;
    }
    count = (int32_t)arp.pn * (int32_t)(st.v[P_ARP_RANGE] + 1u);
    switch (st.v[P_ARP_MODE]) {
    case 0:                              /* UP */
        arp.idx = (arp.idx + 1) % count;
        break;
    case 1:                              /* DOWN */
        arp.idx = arp.idx <= 0 ? count - 1 : arp.idx - 1;
        break;
    case 2:                              /* UPDOWN */
        arp.idx += arp.dir;
        if (arp.idx >= count) {
            arp.idx = count > 1 ? count - 2 : 0;
            arp.dir = -1;
        } else if (arp.idx < 0) {
            arp.idx = count > 1 ? 1 : 0;
            arp.dir = 1;
        }
        break;
    case 3:                              /* RANDOM */
        arp.idx = (int32_t)(arp_rand() % (uint32_t)count);
        break;
    default:                             /* DRIFT: a step either way, or stay */
        arp.idx = arp.idx < 0 ? 0 : arp.idx + (int32_t)(arp_rand() % 3u) - 1;
        if (arp.idx < 0)
            arp.idx = count > 1 ? 1 : 0;
        if (arp.idx >= count)
            arp.idx = count > 1 ? count - 2 : 0;
        break;
    }
    arp_stop();
    if (arp_rand() % 127u < arp_chance()) {
        int note = arp.play[arp.idx % (int32_t)arp.pn] + 12 * (arp.idx / (int32_t)arp.pn + (int32_t)st.v[P_GL_OCTAVE]);
        float v = (float)arp.vel * (1.0f / 127.0f) * (0.8f + 0.2f * (float)(arp_rand() & 255u) * (1.0f / 255.0f));
        if (note > 127)
            note = 127;
        frasil_glint_on(note, v);
        arp.src = arp.play[arp.idx % (int32_t)arp.pn];
        if (arp.pprs[arp.idx % (int32_t)arp.pn] > 0.0f)   /* its key pressed: the step louder */
            frasil_glint_pressure(note, arp.pprs[arp.idx % (int32_t)arp.pn]);
        arp.sounding = (int16_t)note;
        arp.off_ms = now + period / 2u;
        arp.steps++;
    }
}

/* REC's metronome: a tick on each beat of the grid (MIDI clock, else TEMPO) while the sequencer records, the bar's
 * first higher; from the beat after it starts */
static void metro_run(void)
{
    static int32_t last = -1;
    int32_t b = (int32_t)(arp.tick * (1.0f / 24.0f));
    if (!st.v[P_SQ_REC])
        st.clicks = 0;
    else if (b != last && last >= 0 && st.metro) {
        frasil_click(b % 4 == 0);
        st.clicks++;
    }
    last = b;
}

/* GENERATIVE FX's SPARKLE: glints by themselves now and then, on the notes the drones hold (GLINT's OCTAVE up, one
 * in three an octave more), as often as it is turned (with what the LFOs add: a few a minute .. six a second), soft,
 * short; nothing held, nothing */
static struct {
    uint8_t armed;                       /* the next one's time set */
    uint32_t next;
    int8_t note[2];                      /* the ones sounding, -1: none (two at most: GLINT's other voices are the
                                          * player's) */
    uint32_t off[2];
    uint32_t n;                          /* played (the tests) */
} spk = {.note = {-1, -1}};

/* a glint the player sounds on note n now (a key, MIDI, the arpeggio's or the sequence's step): SPARKLE leaves it be */
static int glint_taken(int n)
{
    uint32_t k;
    if (arp.sounding == n || sq.sounding == n || bit_get(mgl, n))
        return 1;
    for (k = 0; k < NKEYS; k++)
        if (key[k].kind == K_GLINT && key[k].root == n)
            return 1;
    return 0;
}

static void sparkle_run(void)
{
    uint32_t now = plat_ms(), i, n, cnt = 0, slot = 0;
    float d = ((float)st.v[P_GX_SPARK] + mod[P_GX_SPARK]) * (1.0f / 127.0f), mean;
    for (i = 0; i < 2u; i++)
        if (spk.note[i] >= 0 && (int32_t)(now - spk.off[i]) >= 0) {
            if (!glint_taken(spk.note[i]))   /* (taken since: the player's now) */
                frasil_glint_off(spk.note[i]);
            spk.note[i] = -1;
        }
    if (d <= 0.004f) {
        spk.armed = 0;
        return;
    }
    mean = 1000.0f / (6.0f * (d > 1.0f ? 1.0f : d * d) + 0.02f);   /* ms between two, on average */
    if (!spk.armed || (int32_t)(spk.next - now) > (int32_t)(mean * 1.7f)) {   /* just on, or turned up: the next one
                                                                            * from now (not at once) */
        spk.armed = 1;
        spk.next = now + (uint32_t)(mean * (0.3f + 1.4f * (float)(arp_rand() & 1023u) * (1.0f / 1023.0f)));
        return;
    }
    if ((int32_t)(now - spk.next) < 0)
        return;
    spk.next = now + (uint32_t)(mean * (0.3f + 1.4f * (float)(arp_rand() & 1023u) * (1.0f / 1023.0f)));
    for (n = 0; n < 128u; n++)
        cnt += held_by[n] != 0;
    if (!cnt)
        return;
    cnt = arp_rand() % cnt;
    for (n = 0; n < 128u; n++)
        if (held_by[n] && cnt-- == 0)
            break;
    {
        int note = (int)n + 12 * (int)st.v[P_GL_OCTAVE] + (arp_rand() % 3u == 0 ? 12 : 0);
        while (note > 120)
            note -= 12;
        if (glint_taken(note))
            return;
        for (i = 0; i < 2u; i++)         /* a free place, else the first goes */
            if (spk.note[i] < 0) {
                slot = i;
                break;
            }
        if (spk.note[slot] >= 0 && !glint_taken(spk.note[slot]))
            frasil_glint_off(spk.note[slot]);
        frasil_glint_on(note, 0.22f + 0.4f * (float)(arp_rand() & 255u) * (1.0f / 255.0f));
        spk.note[slot] = (int8_t)note;
        spk.off[slot] = now + 80u + arp_rand() % 140u;
        spk.n++;
    }
}

/* ----------------------------------------------------------------- MIDI --- */
static const struct {
    uint8_t cc, p;
} CC_MAP[] = {
    {1, P_MOUVEMENT}, {74, P_TIMBRE}, {91, P_ESPACE}, {70, P_CHOEUR}, {71, P_PARTIELS}, {72, P_SOUFFLE},
    {73, P_ATTAQUE}, {75, P_RELACHE}, {76, P_TAILLE}, {77, P_DUREE}, {78, P_SHIMMER}, {79, P_CHALEUR},
    {12, P_DR_FULL}, {13, P_DR_PRESET}, {14, P_GX_SPARK}, {15, P_GX_BLUR}, {16, P_GX_SINK}, {17, P_MX_DRONE},
    {18, P_MX_GLINT}, {19, P_MX_BASS},   /* (the performance's: the hands, GENERATIVE FX, the faders) */
};

/* GLO held and a CC on the drones' channel: that CC turns the setting turned last from now on (NLEARN of them, the
 * oldest replaced; kept with the settings) */
static void cc_learn(uint32_t cc)
{
    uint32_t i, at = NLEARN;
    char t[12];
    if (!st.touched_at || st.touched >= NPARAM) {
        app_say("MIDI LEARN", "TURN A SETTING FIRST");
        return;
    }
    for (i = 0; i < NLEARN; i++)
        if (set.learn_cc[i] == cc || (at == NLEARN && !set.learn_cc[i]))
            at = i;
    if (at == NLEARN) {                  /* full: the oldest goes */
        for (i = 1; i < NLEARN; i++) {
            set.learn_cc[i - 1] = set.learn_cc[i];
            set.learn_p[i - 1] = set.learn_p[i];
        }
        at = NLEARN - 1u;
    }
    set.learn_cc[at] = (uint8_t)cc;
    set.learn_p[at] = st.touched;
    prefs_save();
    t[0] = 0;
    text_cat(t, "CC ");
    int_text((int32_t)cc, t + 3);
    app_say(t, PD[st.touched].name);
}

/* GLINT's note n (as it sounds) pressed (0..1): arpeggiated, the steps from it (kept without OCTAVE); else the glint
 * itself */
static void glint_note_press(int n, float f)
{
    uint32_t i;
    if (st.v[P_SQ_TYPE] == SQT_SEQ) {    /* the sequencer's: the note sounding, louder and longer */
        if (sq.sounding >= 0)
            frasil_glint_pressure(sq.sounding, f);
        else if (st.v[P_SQ_REC])
            frasil_glint_pressure(n, f);
        return;
    }
    if (st.v[P_SQ_TYPE] == SQT_ARP) {
        n = bit_get(mgl, n) ? mgl_base[n] : n - 12 * (int)st.v[P_GL_OCTAVE];
        for (i = 0; i < arp.n; i++)
            if (arp.held[i] == n)
                arp.prs[i] = f;
        for (i = 0; i < arp.pn; i++)
            if (arp.play[i] == n)
                arp.pprs[i] = f;
        if (arp.sounding >= 0 && arp.src == n)
            frasil_glint_pressure(arp.sounding, f);
    } else {
        frasil_glint_pressure(n, f);
    }
}

/* each track on its own MIDI channel (P_CANAL, P_CH_GLINT, P_CH_BASS: OFF, 1 .. 16, OMNI), whatever the keys play.
 * GLINT's and BASS's notes play as they come (no scale, no octave added: the sequencer's own; into the arpeggio and
 * the sequence, kept without GLINT's OCTAVE, as a key's are, it on them as they play); the drones' go
 * through the harmony, as ever */
static int on_ch(int p, uint32_t c) { return st.v[p] > 16u || (st.v[p] && st.v[p] == c); }

static void midi_glint_on(int n, uint32_t vel)
{
    uint32_t i, held = 0;
    for (i = 0; i < 4u; i++)
        held |= mgl[i];
    bit_put(mgl, n, 1);
    mgl_base[n] = (int8_t)(n - 12 * (int)st.v[P_GL_OCTAVE]);
    bit_put(mgs, n, st.v[P_SQ_TYPE] == SQT_SEQ ? st.v[P_SQ_REC] != 0 : st.v[P_SQ_TYPE] != SQT_ARP);
    if (st.v[P_SQ_TYPE] == SQT_SEQ) {    /* into the sequencer, as a key would */
        seq_key(mgl_base[n], n, vel);
        return;
    }
    if (st.v[P_SQ_TYPE] == SQT_ARP) {    /* into the arpeggio, as a key would */
        if (arp.latch && !held && !glint_keys_down(NKEYS)) {
            arp.n = 0;                   /* latched, a new hand: a new set */
            arp.dirty = 1;
        }
        arp_add(mgl_base[n], vel);
    } else {
        frasil_glint_on(n, (float)vel * (1.0f / 127.0f));
    }
}

static void midi_glint_off(int n)
{
    if (!bit_get(mgl, n))
        return;
    bit_put(mgl, n, 0);
    if (bit_get(mgs, n) && sq.sounding != n)
        frasil_glint_off(n);
    bit_put(mgs, n, 0);
    if (st.v[P_SQ_TYPE] != SQT_SEQ && !(arp.latch && st.v[P_SQ_TYPE] == SQT_ARP))
        arp_remove(mgl_base[n]);
}

static void midi_glints_off(void)        /* every GLINT note held from MIDI let go */
{
    uint32_t i;
    for (i = 0; i < 128u; i++)
        midi_glint_off((int)i);
}

static void midi_bass_on(int n, uint32_t vel)   /* mono, the last note on top */
{
    uint32_t i, j;
    for (i = j = 0; i < mb_n; i++)
        if (mbass[i] != n)
            mbass[j++] = mbass[i];
    mb_n = j;
    if (mb_n == sizeof mbass) {          /* full: the oldest goes */
        for (i = 1; i < mb_n; i++)
            mbass[i - 1] = mbass[i];
        mb_n--;
    }
    mbass[mb_n++] = (int8_t)n;
    bass_play(n, vel);
}

static void midi_bass_off(int n)          /* the one on top let go: back to the one under it, else silence */
{
    uint32_t i, j;
    int top = mb_n ? mbass[mb_n - 1] : -1;
    for (i = j = 0; i < mb_n; i++)
        if (mbass[i] != n)
            mbass[j++] = mbass[i];
    mb_n = j;
    if (n != top || bass_note != bass_fold(n))
        return;                          /* not the one sounding (or the keys have the bass now) */
    if (mb_n)
        bass_play(mbass[mb_n - 1], 100);
    else
        bass_stop();
}

static void midi_note_off(int n)
{
    if (!bit_get(mid_on, n))
        return;
    chord_off(&mid[n]);
    bit_put(mid_on, n, 0);
}

static void midi_in(void)
{
    uint32_t pkt, i;
    int port, root, dr, gl, bs;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st8 = (pkt >> 8) & 0xFFu, ch = st8 & 15u, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        uint32_t kind = st8 & 0xF0u;
        if (st8 == 0xF8u) {              /* clock: the arpeggiator follows it */
            uint32_t now = plat_ms(), dt = now - arp.clk_at;
            if (dt < 200u) {
                arp.clk_dt = arp.clk_dt ? (arp.clk_dt * 3u + dt) / 4u : dt;
                arp.clk_ms = arp.clk_ms > 0.0f ? arp.clk_ms + ((float)dt - arp.clk_ms) * 0.05f : (float)dt;
            }
            arp.clk++;
            arp.clk_new++;
            arp.clk_at = now;
            continue;
        }
        if (st8 == 0xFAu) {              /* start: the grid from its top (and the LFOs on it) */
            arp.clk = arp.clk_new = 0;
            arp.tick = 0.0f;
            lfo_restart = 1;
            if (st.v[P_SQ_TYPE] == SQT_SEQ)
                sq.restart = 1;
            arp.step_n = -1;
            arp.idx = -1;
            continue;
        }
        if (st8 < 0x80u || st8 >= 0xF0u)
            continue;
        dr = on_ch(P_CANAL, ch + 1u);
        gl = on_ch(P_CH_GLINT, ch + 1u);
        bs = on_ch(P_CH_BASS, ch + 1u);
        if (!dr && !gl && !bs)
            continue;
        if (kind == 0x90u && d2) {                   /* a note */
            if (dr) {                                /* the drones: its chord, held until its note-off */
                midi_note_off((int)d1);
                bit_put(sus, (int)d1, 0);
                chord_build((int)d1, st.v, &mid[d1], &root);
                chord_on(&mid[d1], d2);
                bit_put(mid_on, (int)d1, 1);
            }
            if (gl)
                midi_glint_on((int)d1, d2);
            if (bs)
                midi_bass_on((int)d1, d2);
        } else if (kind == 0x80u || kind == 0x90u) {
            if (dr) {
                if (st.sustain)
                    bit_put(sus, (int)d1, 1);
                else
                    midi_note_off((int)d1);
            }
            if (gl)
                midi_glint_off((int)d1);
            if (bs)
                midi_bass_off((int)d1);
        } else if (kind == 0xA0u) {                  /* a note's pressure */
            float f = (float)d2 * (1.0f / 127.0f);
            if (dr && bit_get(mid_on, (int)d1))
                chord_press(&mid[d1], f);
            if (gl && bit_get(mgl, (int)d1))
                glint_note_press((int)d1, f);
            if (bs && mb_n && mbass[mb_n - 1] == (int8_t)d1 && bass_note == bass_fold((int)d1))
                frasil_bass_pressure(f);
        } else if (kind == 0xD0u) {                  /* the channel's: every note of the track */
            float f = (float)d1 * (1.0f / 127.0f);
            if (dr)
                st.swell_midi = f;
            if (gl)
                for (i = 0; i < 128u; i++)
                    if (bit_get(mgl, (int)i))
                        glint_note_press((int)i, f);
            if (bs && mb_n && bass_note == bass_fold(mbass[mb_n - 1]))
                frasil_bass_pressure(f);
        } else if (kind == 0xB0u) {
            if (d1 == 64u && !dr) {
                /* sustain: the drones' alone */
            } else if (d1 == 64u) {                  /* sustain: the notes let go meanwhile go out with it */
                st.sustain = d2 >= 64u;
                if (!st.sustain)
                    for (i = 0; i < 128u; i++)
                        if (bit_get(sus, (int)i)) {
                            bit_put(sus, (int)i, 0);
                            midi_note_off((int)i);
                        }
            } else if (d1 == 120u || d1 == 123u) {   /* all notes off: the track's */
                if (dr)
                    release_drones();
                if (gl)
                    midi_glints_off();
                if (bs && mb_n) {
                    int own = bass_note == bass_fold(mbass[mb_n - 1]);   /* (else the keys have the bass) */
                    mb_n = 0;
                    if (own)
                        bass_stop();
                }
            } else if (!dr) {
                /* the sound's CCs: on the drones' channel only */
            } else if (b_held(B_GLO) && d1) {   /* GLO held: MIDI learn */
                b_used(B_GLO);
                cc_learn(d1);
            } else {
                int learned = 0;
                for (i = 0; i < NLEARN; i++)   /* a learned one first */
                    if (d1 && set.learn_cc[i] == d1 && set.learn_p[i] < NPARAM) {
                        param_set(set.learn_p[i], ((int32_t)d2 * PD[set.learn_p[i]].max + 63) / 127);
                        learned = 1;
                    }
                if (learned) {
                } else if (d1 == 11u) {
                    st.swell_midi = (float)d2 * (1.0f / 127.0f);
                } else if (d1 == 80u) {
                    param_set(P_FREEZE, d2 >= 64u);
                } else {
                    for (i = 0; i < sizeof CC_MAP / sizeof CC_MAP[0]; i++)
                        if (CC_MAP[i].cc == d1)
                            param_set(CC_MAP[i].p, ((int32_t)d2 * PD[CC_MAP[i].p].max + 63) / 127);
                }
            }
        } else if (kind == 0xC0u && dr && d1 < NSCENES) {   /* Program Change: a scene, as SCENES' MORPH and KEEP */
            scene_go(d1);
        }
    }
}

/* ---------------------------------------------------------------- input --- */
static void page_button(uint32_t b)       /* (a page: the logo goes; PLAY and OCT leave it up) */
{
    uint32_t p, first = NPAGES;
    if (b != B_HOME)
        st.logo = 0;
    st.help = 0;
    for (p = 0; p < NPAGES; p++)
        if (PAGES[p].button == b && !page_hidden(p)) {
            if (first == NPAGES)
                first = p;
            if (p == st.page) {          /* again: its next page shown */
                uint32_t q = p + 1u;
                while (q < NPAGES && PAGES[q].button == b && page_hidden(q))
                    q++;
                if (q < NPAGES && PAGES[q].button == b) {
                    st.page = (uint8_t)q;
                    return;
                }
            }
        }
    if (first < NPAGES) {
        st.page = (uint8_t)first;
        if (first == PG_SCENES)
            st.pick = st.scene;
    }
}

/* LFO held and an encoder turned: the setting under it moves to the next LFO (OFF, LFO 1, 2, 3, OFF), into its first
 * free TARGET; DEPTH at 0 comes up to a quarter */
static void lfo_route(uint32_t p, int32_t d)
{
    uint32_t t, i, k, cur = 0, nx, at = 4;
    char m[12];
    for (t = 1; p < NPARAM && t < NLFO_TGT && LFO_TGT[t] != p; t++)
        ;
    if (p >= NPARAM || t >= NLFO_TGT) {
        app_say(p < NPARAM ? PD[p].name : "LFO", "NOT AN LFO TARGET");
        return;
    }
    for (i = 0; i < NLFO; i++)
        for (k = 0; k < 4u; k++)
            if (st.v[LFO_BASE[i] + 4u + k] == t)
                cur = i + 1u;
    nx = (cur + (d > 0 ? 1u : NLFO)) % (NLFO + 1u);
    if (nx) {
        for (k = 0; k < 4u && at == 4u; k++)
            if (st.v[LFO_BASE[nx - 1u] + 4u + k] == 0)
                at = k;
        if (at == 4u) {
            m[0] = 0;
            text_cat(m, "LFO ");
            int_text((int32_t)nx, m + 4);
            text_cat(m, " FULL");
            app_say(LFO_TGT_NAME[t], m);
            return;
        }
    }
    for (i = 0; i < NLFO; i++)           /* off the others */
        for (k = 0; k < 4u; k++)
            if (st.v[LFO_BASE[i] + 4u + k] == t)
                param_set((int)(LFO_BASE[i] + 4u + k), 0);
    if (!nx) {
        app_say(LFO_TGT_NAME[t], "> NO LFO");
        return;
    }
    param_set((int)(LFO_BASE[nx - 1u] + 4u + at), (int32_t)t);
    if (!st.v[LFO_BASE[nx - 1u] + 2u])
        param_set((int)(LFO_BASE[nx - 1u] + 2u), 32);   /* (DEPTH: a quarter) */
    m[0] = 0;
    text_cat(m, "> LFO ");
    int_text((int32_t)nx, m + 6);
    app_say(LFO_TGT_NAME[t], m);
}

static int32_t accel(uint32_t e, int32_t d)
{
    uint32_t now = plat_ms(), dt = now - st.enc_at[e];
    st.enc_at[e] = now;
    return dt < 25u ? d * 4 : dt < 60u ? d * 2 : d;
}

static void encoders(void)
{
    uint32_t e;
    for (e = 0; e < NE; e++) {
        int32_t d = plat_enc((int)e), p;
        if (!d)
            continue;
        if (st.help && e == EN_SELECT) { /* the help up: SELECT turns its pages */
            st.help_page = (uint8_t)clamp((int32_t)st.help_page + (d > 0 ? 1 : -1), 0, HELP_PAGES - 1);
            continue;
        }
        st.help = 0;
        st.logo = 0;
        if (b_held(B_LFO) && e >= EN_K1) {   /* LFO held: the setting under it onto an LFO, the next, or off */
            b_used(B_LFO);
            lfo_route(st.page == PG_SCENES ? NPARAM : PAGES[st.page].p[e - EN_K1], d);
            continue;
        }
        if (MACRO_OF[e] < NPARAM) {
            p = MACRO_OF[e];
        } else if (st.page == PG_SCENES && e == EN_K1) {
            st.pick = (uint8_t)clamp((int32_t)st.pick + d, 0, NSCENES - 1);
            continue;
        } else {
            p = PAGES[st.page].p[e - EN_K1];
            if (p >= NPARAM)                 /* an empty slot */
                continue;
        }
        if (p == P_KEYS) {               /* KEYS lets the notes go: a second detent the same way, within 2 s */
            int8_t s = d > 0 ? 1 : -1;
            if (st.keys_arm != s || plat_ms() - st.keys_arm_at > 2000u) {
                st.keys_arm = s;
                st.keys_arm_at = plat_ms();
                app_say("KEYS", "TURN AGAIN");
                continue;
            }
            st.keys_arm = 0;
        }
        param_set(p, (int32_t)st.v[p] + (PD[p].max > 16u ? accel(e, d) : d));
        st.touched = (uint8_t)p;
        st.touched_enc = (uint8_t)e;
        st.touched_at = plat_ms();
    }
}

/* REC tapped: into GLINT's sequencer, recording LIVE (TYPE on SEQ, the keys GLINT's, its first page, the sequence
 * playing); recording, RECORD OFF, the keys back to what they played, the page staying */
static void rec_tap(void)
{
    if (st.v[P_SQ_REC]) {
        static const char *const KB[3] = {"DRONE", "GLINT", "BASS"};
        int give = st.rec_took;
        param_set(P_SQ_REC, 0);
        if (give)
            param_set(P_KB, st.kb_rec);
        app_say("RECORD OFF", KB[st.v[P_KB] % 3u]);
        return;
    }
    st.kb_rec = st.v[P_KB];
    seq_keep();                          /* (UNDO: the sequence before this take) */
    param_set(P_SQ_TYPE, (int32_t)SQT_SEQ);
    param_set(P_KB, 1);
    param_set(P_SQ_REC, 2);
    st.rec_took = 1;
    if (!sq.run)
        seq_begin();
    st.logo = 0;
    st.help = 0;
    st.page = PG_SEQ;
    app_say("RECORD", "LIVE");
}

/* PLAY tapped: GLINT's transport (the sequence stops, or starts from its first step; the arpeggio waits, or goes on
 * from the top); on SCENES, the scene K1 points at (MORPH, KEEP) */
static void play_tap(void)
{
    if (st.page == PG_SCENES) {
        scene_go(st.pick);
    } else if (st.v[P_SQ_TYPE] == SQT_SEQ) {
        if (sq.run) {
            seq_note_off();
            sq.run = 0;
            app_say("SEQUENCE", "STOPPED");
        } else {
            seq_begin();
            app_say("SEQUENCE", "FROM THE TOP");
        }
    } else if (st.v[P_SQ_TYPE] == SQT_ARP) {
        arp.paused ^= 1u;
        arp.dirty = 1;                   /* (back: its pattern from the top) */
        if (arp.paused)
            arp_stop();
        app_say("ARPEGGIO", arp.paused ? "WAITING" : "PLAYING");
    } else {
        app_say("HOLD PLAY", "RELEASE ALL");
    }
}

/* PLAY held: everything let go (the drones, the bass, GLINT's notes, the arpeggio, the sequence), what was held kept;
 * held again with nothing held, that comes back (RESTORE) */
static int holding_any(void) { return lit[0] | lit[1] | lit[2] | lit[3] || bass_note >= 0 || arp.n || sq.run; }

static void play_hold(void)
{
    if (!holding_any() && rst_valid) {
        rst_valid = 0;
        scene_notes_play(&rst);
        if (rst_seq && st.v[P_SQ_TYPE] == SQT_SEQ)
            seq_begin();
        app_say("RESTORED", "");
        return;
    }
    if (holding_any()) {
        scene_notes_take(&rst);
        rst_seq = sq.run;
        rst_valid = 1;
    }
    release_all();
    arp.n = arp.pn = 0;                  /* and the arpeggio, latched or not; the sequence stops */
    arp.dirty = 0;
    arp.paused = 0;
    seq_note_off();
    sq.run = 0;
    app_say("RELEASE ALL", rst_valid ? "HOLD AGAIN: RESTORE" : "");
}

/* PLAY being held, for the screen: 0 not yet (or not held), else how far to its hold, 0..1 (from 200 ms) */
static float play_holding(void)
{
    uint32_t t = plat_ms() - st.hold_at[B_PLAY];
    if (!b_held(B_PLAY) || (st.hold_done >> B_PLAY & 1u) || t < PLAY_SHOW_MS)
        return 0.0f;
    return (float)(t - PLAY_SHOW_MS) / (float)(PLAY_HOLD_MS - PLAY_SHOW_MS);
}

static uint32_t help_for(uint32_t b)     /* the help's page with button b on it */
{
    return b == B_FX || b == B_SEL || b == B_ENV || b == B_LFO ? 0u
         : b == B_EDIT || b == B_GLO || b == B_HOME || b == B_SAVE ? 1u : 2u;
}

static void tap(uint32_t k)              /* a button let go before its hold, alone */
{
    switch (k) {
    case B_HOME:                         /* its page; the help closes; SOUND, MIXER, the logo, the river */
        if (st.help)
            st.help = 0;
        else if (st.page == PG_SON && st.logo) {
            st.logo = 0;
            st.logo_until = 0;
        } else if (st.page == PG_SON) {
            st.page = PG_MIXER;
        } else if (st.page == PG_MIXER) {
            st.page = PG_SON;
            st.logo = 1;
            st.logo_until = 0;
        } else {
            st.logo = 0;
            page_button(B_HOME);
        }
        break;
    case B_SAVE:                         /* SCENES, and back where it came from */
        if (st.page == PG_SCENES) {
            st.page = st.prev_page < NPAGES && st.prev_page != PG_SCENES ? st.prev_page : PG_SON;
            if (page_hidden(st.page))
                st.page = PG_GLINT;
        } else {
            st.prev_page = st.page;
            page_button(B_SAVE);
        }
        st.logo = 0;
        st.help = 0;
        break;
    case B_PLAY:
        play_tap();
        break;
    case B_REC:
        rec_tap();
        break;
    case B_OCTDN:
    case B_OCTUP:
        break;
    default:                             /* its pages */
        st.logo = 0;
        page_button(k);
        break;
    }
}

/* the buttons just down: when, and nothing done with them yet (first in input: what goes with them in the same pass
 * counts) */
static void buttons_down(uint32_t btn)
{
    uint32_t down = btn & ~btn_prev, k, now = plat_ms();
    for (k = 0; k < NB; k++)
        if (down >> k & 1u) {
            st.hold_at[k] = now;
            st.hold_done &= ~(1u << k);
        }
}

static void buttons(uint32_t btn)
{
    uint32_t down = btn & ~btn_prev, up = btn_prev & ~btn, k, now = plat_ms();
    for (k = 0; k < NB; k++)
        if ((up >> k & 1u) && !(st.hold_done >> k & 1u) &&
            !(k == B_PLAY && now - st.hold_at[B_PLAY] >= PLAY_SHOW_MS))   /* (PLAY let go while counting: nothing) */
            tap(k);
    for (k = 0; k < NB; k++) {
        if (!(down >> k & 1u))
            continue;
        if (k == B_OCTDN || k == B_OCTUP) {   /* at once; both: back to the start */
            if ((btn >> B_OCTDN & 1u) && (btn >> B_OCTUP & 1u))
                st.octave = OCT_DEF;
            else
                st.octave = (int8_t)clamp(st.octave + (k == B_OCTUP ? 1 : -1), -3, 2);
        } else if (k == B_REC && (btn >> B_SAVE & 1u)) {   /* SAVE held, REC: STORE (SCENES' slot, else the scene's) */
            b_used(B_SAVE);
            b_used(B_REC);
            scene_save(st.page == PG_SCENES ? st.pick : st.scene);
        }
    }
    btn_prev = btn;
}

/* the holds, each once while its button stays down (HOLD_MS; PLAY's longer) */
static void holds(void)
{
    uint32_t now = plat_ms(), b;
    for (b = 0; b < NB; b++) {
        if (!b_held(b) || (st.hold_done >> b & 1u) || now - st.hold_at[b] < (b == B_PLAY ? PLAY_HOLD_MS : HOLD_MS))
            continue;
        switch (b) {
        case B_HOME:                     /* the help, on its page with this page's button, until HOME again */
            b_used(b);
            st.help = 1;
            st.help_page = (uint8_t)help_for(PAGES[st.page].button);
            break;
        case B_ARP:                      /* the arpeggio's latch */
            b_used(b);
            arp.latch ^= 1u;
            if (!arp.latch)              /* off: only the keys still down go on */
                arp_keep_held();
            app_say("ARP LATCH", arp.latch ? "ON" : "OFF");
            break;
        case B_FX:                       /* FREEZE, the page staying */
            b_used(b);
            param_set(P_FREEZE, !st.v[P_FREEZE]);
            app_say("FREEZE", st.v[P_FREEZE] ? "ON" : "OFF");
            break;
        case B_REC:                      /* the metronome */
            b_used(b);
            st.metro ^= 1u;
            prefs_save();
            app_say("METRONOME", st.metro ? "ON" : "OFF");
            break;
        case B_SEQ:                      /* SWELL, while it is held (swell_update) */
            b_used(b);
            break;
        case B_EDIT:                     /* COMPARE: the scene as saved, while it is held */
            b_used(b);
            st.compare = 1;
            st.engine_dirty = 1;
            break;
        case B_PLAY:
            b_used(b);
            play_hold();
            break;
        default:                         /* (SEL, SAVE, LFO, GLO: with something else; the rest, none) */
            break;
        }
    }
    if (st.compare && !b_held(B_EDIT)) {
        st.compare = 0;
        st.engine_dirty = 1;
    }
}

static void swell_update(uint32_t btn)
{
    static uint32_t last;
    uint32_t now = plat_ms(), dt = now - last;
    float s, step = (float)(dt > 100u ? 100u : dt) * (1.0f / 1500.0f);   /* 1.5 s up, 1.5 s down */
    last = now;
    if ((btn >> B_SEQ & 1u) && now - st.hold_at[B_SEQ] >= HOLD_MS)
        st.swell_btn = st.swell_btn + step > 1.0f ? 1.0f : st.swell_btn + step;
    else
        st.swell_btn = st.swell_btn - step < 0.0f ? 0.0f : st.swell_btn - step;
    s = st.swell_btn > st.swell_midi ? st.swell_btn : st.swell_midi;
    if ((float)st.v[P_SWELL] * (1.0f / 127.0f) > s)   /* GENERATIVE FX's, where it is turned */
        s = (float)st.v[P_SWELL] * (1.0f / 127.0f);
    if (fm_fabsf(s - st.swell_sent) > 0.004f || (s == 0.0f && st.swell_sent != 0.0f)) {
        st.swell_sent = s;
        frasil_swell(s * s * (3.0f - 2.0f * s));
    }
}

static int32_t white_of(uint32_t k)     /* key k's place among the white ones (F2's key the first), -1: black */
{
    uint32_t i, n = 0;
    for (i = 0; i <= k; i++) {
        uint32_t c = (5u + i) % 12u;     /* (its note's place in the octave: key 0 is an F) */
        int w = (0xAB5u >> c) & 1u;      /* C D E F G A B: bits 0 2 4 5 7 9 11 */
        if (i == k)
            return w ? (int32_t)n : -1;
        n += (uint32_t)w;
    }
    return -1;
}

static void key_down(uint32_t k, uint32_t vel)
{
    int played = 53 + (int)k + 12 * st.octave, root;
    chord_t c;
    if (st.logo_until)                   /* playing ends the power-on logo */
        st.logo = 0;
    key[k].t = plat_ms();
    key[k].prs = 0;
    key[k].pressed_in = 0;
    key[k].was_lit = 0;
    if (b_held(B_SAVE)) {                /* SAVE held: the white keys are the scenes, nothing sounds */
        int32_t w = white_of(k);
        b_used(B_SAVE);
        key[k].kind = K_NONE;
        if (w >= 0 && w < (int32_t)NSCENES)
            scene_go((uint32_t)w);
        return;
    }
    if (b_held(B_SEL))                   /* (SEL with a key: no HARMONY on its release, whatever the keys play) */
        b_used(B_SEL);
    if (st.glint) {                      /* a glint: in the scale, GLINT's octaves up (the arpeggio and the sequence
                                          * keep it without them, OCTAVE on them as they play) */
        int b = scale_snap(played, st.v), n = b + 12 * st.v[P_GL_OCTAVE];
        key[k].kind = K_GLINT;
        key[k].root = (int8_t)(n > 127 ? 127 : n);
        key[k].gbase = (int8_t)b;
        key[k].gsound = st.v[P_SQ_TYPE] == SQT_SEQ ? st.v[P_SQ_REC] != 0 : st.v[P_SQ_TYPE] != SQT_ARP;
        if (st.v[P_SQ_TYPE] == SQT_SEQ) {
            seq_key(b, key[k].root, vel);
            return;
        }
        if (st.v[P_SQ_TYPE] == SQT_ARP) {
            if (arp.latch && !glint_keys_down(k)) {   /* latched, a new hand: a new set (at the next step) */
                arp.n = 0;
                arp.dirty = 1;
            }
            arp_add(b, vel);
        }
        else
            frasil_glint_on(key[k].root, (float)vel * (1.0f / 127.0f));
        return;
    }
    if (st.bass) {                       /* the bass: one note, in the scale, BASS's octaves down; held as a drone is */
        int n = scale_snap(played, st.v) + 12 * ((int)st.v[P_BS_OCTAVE] - 2);
        n = bass_fold(n);
        key[k].kind = K_BASS;
        key[k].root = (int8_t)n;
        if (!st.v[P_KEYS] && bass_note == key[k].root)
            key[k].was_lit = 1;          /* latched already: a tap lets it go */
        else
            bass_play(key[k].root, vel);
        return;
    }
    if (b_held(B_SEL)) {                 /* SEL held: a single note, whatever CHORD says */
        uint8_t v1[NSOUND];
        uint32_t i;
        for (i = 0; i < NSOUND; i++)
            v1[i] = st.v[i];
        v1[P_CHORD] = 0;
        v1[P_VOICING] = 0;
        chord_build(played, v1, &c, &root);
    } else {
        chord_build(played, st.v, &c, &root);
    }
    if (st.v[P_KEYS]) {                  /* held: the chord while the key is down */
        key[k].kind = K_HOLD;
        key[k].chord = c;
        chord_on(&c, vel);
        return;
    }
    key[k].kind = K_LATCH;
    key[k].root = (int8_t)root;
    if (bit_get(lit, root)) {
        key[k].was_lit = 1;              /* a tap lets it go; pressing in swells it */
        return;
    }
    lat[root] = c;
    bit_put(lit, root, 1);
    chord_on(&c, vel);
}

static void key_up(uint32_t k)
{
    switch (key[k].kind) {
    case K_GLINT:
        if (key[k].gsound && sq.sounding != key[k].root)   /* what it sounded itself (not the sequence's step) */
            frasil_glint_off(key[k].root);
        if (st.v[P_SQ_TYPE] != SQT_SEQ && !(arp.latch && st.v[P_SQ_TYPE] == SQT_ARP))
            arp_remove(key[k].gbase);    /* (if it was arpeggiated; latched, it stays) */
        break;
    case K_HOLD:
        chord_off(&key[k].chord);
        break;
    case K_BASS:
        if (bass_note != key[k].root)
            break;                       /* another note has the bass now */
        if (key[k].prs)
            frasil_bass_pressure(0.0f);
        if (st.v[P_KEYS]) {              /* held: back to the last other bass key still down, else let go */
            int32_t j, last = -1;
            for (j = 0; j < (int32_t)NKEYS; j++)
                if ((uint32_t)j != k && key[j].kind == K_BASS && (last < 0 || key[j].t - key[last].t < 0x80000000u))
                    last = j;
            key[k].kind = K_NONE;
            if (last >= 0)
                bass_play(key[last].root, 100);
            else
                bass_stop();
        } else if (key[k].was_lit && !key[k].pressed_in && plat_ms() - key[k].t < TAP_MS) {
            bass_stop();                 /* latched: a tap on it */
        }
        break;
    case K_LATCH:
        if (key[k].prs)
            chord_press(&lat[(uint8_t)key[k].root], 0.0f);
        if (key[k].was_lit && !key[k].pressed_in && plat_ms() - key[k].t < TAP_MS &&
            bit_get(lit, key[k].root)) {
            chord_off(&lat[(uint8_t)key[k].root]);
            bit_put(lit, key[k].root, 0);
        }
        break;
    }
    key[k].kind = K_NONE;
}

static void key_pressure(uint32_t k, uint8_t p)
{
    float f = (float)p * (1.0f / 127.0f);
    if (p > 12u)
        key[k].pressed_in = 1;
    if (!(p > key[k].prs + 2u || p + 2u < key[k].prs || (p == 0 && key[k].prs)))
        return;
    key[k].prs = p;
    if (key[k].kind == K_GLINT)
        glint_note_press(key[k].root, f);
    else if (key[k].kind == K_HOLD)
        chord_press(&key[k].chord, f);
    else if (key[k].kind == K_BASS && bass_note == key[k].root)
        frasil_bass_pressure(f);
    else if (key[k].kind == K_LATCH && bit_get(lit, key[k].root))
        chord_press(&lat[(uint8_t)key[k].root], f);
}

static void keys_scan(void)
{
    uint32_t keys, k;
    touch_poll();
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(keys_prev & bit))
            key_down(k, touch_vel(k));
        else if (!(keys & bit) && (keys_prev & bit))
            key_up(k);
        else if (keys & bit)
            key_pressure(k, touch_pressure(k));
    }
    keys_prev = keys;
}

/* the keys' lights: bright, the roots of the latched chords; dim, the scale's notes (none when CHROMATIC) */
/* the key that plays note g, `shift` semitones from its own in the scale (GLINT's octaves up, BASS's down), at this
 * octave (the one whose own note it is, else one that falls to it); -1 none */
static int32_t note_key(int g, int shift)
{
    int32_t k, near = -1;
    for (k = 0; k < (int32_t)NKEYS; k++) {
        int played = 53 + (int)k + 12 * st.octave, snapped = scale_snap(played, st.v);
        if (snapped + shift != g)
            continue;
        if (snapped == played)
            return k;
        if (near < 0)
            near = k;
    }
    return near;
}

static void keys_lights(uint32_t *bright, uint32_t *dim)
{
    uint32_t k;
    uint16_t bits = SCALE_BITS[st.v[P_SCALE] % NSCALES];
    *bright = *dim = 0;
    if (b_held(B_SAVE)) {                /* SAVE held: the scenes on the white keys, the one loaded bright */
        for (k = 0; k < NKEYS; k++) {
            int32_t w = white_of(k);
            if (w < 0 || w >= (int32_t)NSCENES)
                continue;
            if (w == (int32_t)st.scene)
                *bright |= 1u << k;
            else if ((scn.used >> w & 1u) || w < (int32_t)NFACTORY)
                *dim |= 1u << k;
        }
        return;
    }
    if (st.glint && st.v[P_SQ_TYPE] == SQT_SEQ) {   /* the sequencer: the note sounding bright, the scale's dim */
        int32_t kk;
        for (k = 0; k < NKEYS; k++)
            if (bits != 0xFFFu && in_scale(53 + (int)k + 12 * st.octave, st.v[P_ROOT] % 12, bits))
                *dim |= 1u << k;
        if (sq.sounding >= 0 && (kk = note_key(sq.sounding, 12 * st.v[P_GL_OCTAVE])) >= 0)
            *bright |= 1u << kk;
        *bright |= keys_prev;
        return;
    }
    if (st.glint && st.v[P_SQ_TYPE] == SQT_ARP) {   /* GLINT's arpeggio: its notes dim, the one playing bright */
        int32_t kk, sh = 0;              /* (its notes are kept without OCTAVE) */
        for (k = 0; k < arp.pn; k++)
            if ((kk = note_key(arp.play[k], sh)) >= 0)
                *dim |= 1u << kk;
        if (arp.sounding >= 0 && (kk = note_key(arp.src, sh)) >= 0)
            *bright |= 1u << kk;
        *bright |= keys_prev;
        return;
    }
    if (st.bass) {                       /* BASS: its note bright, the scale's dim */
        int32_t kk;
        for (k = 0; k < NKEYS; k++)
            if (bits != 0xFFFu && in_scale(53 + (int)k + 12 * st.octave, st.v[P_ROOT] % 12, bits))
                *dim |= 1u << k;
        if (bass_note >= 0 && (kk = note_key(bass_note, 12 * ((int)st.v[P_BS_OCTAVE] - 2))) >= 0)
            *bright |= 1u << kk;
        *bright |= keys_prev;
        return;
    }
    for (k = 0; k < NKEYS; k++) {
        int n = 53 + (int)k + 12 * st.octave;
        if (bit_get(lit, n))
            *bright |= 1u << k;
        if (bits != 0xFFFu && in_scale(n, st.v[P_ROOT] % 12, bits))
            *dim |= 1u << k;
    }
    *bright |= keys_prev;
}

static void input(void)
{
    uint32_t btn = plat_buttons(), lb, mb, gb, kb, kd, i;
    frasil_view_t v[FR_NV];
    btn_now = btn;
    buttons_down(btn);
    keys_scan();
    arp_run();
    delay_sync();
    midi_in();
    pump_sync();                         /* (after the MIDI: the clocks just come in) */
    lfo_run();
    sparkle_run();
    metro_run();
    morph_run();
    {                                    /* SCENES left: its MORPH and KEEP kept, if they changed */
        static uint8_t was;
        if (was == PG_SCENES && st.page != PG_SCENES &&
            (set.morph != st.v[P_MORPH] || set.keep != st.v[P_KEEP] || set.magic != SET_MAGIC))
            prefs_save();
        was = st.page;
    }
    if (frasil_pump_gain() < pump_low)
        pump_low = frasil_pump_gain();
    encoders();
    buttons(btn);
    holds();
    swell_update(btn);
    if (st.engine_dirty)
        engine_push();
    /* the buttons' lights, four levels (lights_set): the function buttons at the mid level, their names readable in the
     * dark; lit, what is chosen or on: the page's button, FX while the cloud is held, SEQ swelling. PLAY in the glow,
     * lit while anything sounds. REC at the mid level; while the sequencer records, flashing on the beat, twice as quick
     * as OCT. OCT- and OCT+ in the glow, a little brighter on the beat (MIDI clock, else TEMPO; app_tick). SAVE lit
     * while the sound differs from the scene's. LFO in the glow while no LFO runs. ARP, off its pages, a flash on each
     * of GLINT's steps */
    lb = 1u << PAGES[st.page].button;
    mb = ((1u << B_REC) - 1u) & ~(1u << B_PLAY);
    gb = 1u << B_PLAY | 1u << B_OCTDN | 1u << B_OCTUP;
    if (st.dirty)
        lb |= 1u << B_SAVE;
    if (!lfo_any) {
        mb &= ~(1u << B_LFO);
        gb |= 1u << B_LFO;
    }
    {
        static uint32_t steps_seen, flash_until;
        uint32_t n = arp.steps + sq.steps;
        if (n != steps_seen) {
            steps_seen = n;
            flash_until = plat_ms() + 60u;
        }
        if (PAGES[st.page].button != B_ARP && (int32_t)(flash_until - plat_ms()) > 0)
            lb |= 1u << B_ARP;
    }
    if (!st.v[P_SQ_REC])
        mb |= 1u << B_REC;
    else if ((uint32_t)arp.tick % 12u < 3u)   /* (the first quarter of each half beat) */
        lb |= 1u << B_REC;
    if (st.v[P_FREEZE])
        lb |= 1u << B_FX;
    oct_soft = (uint32_t)arp.tick % 24u < 6u;   /* the first quarter of each beat */
    if (st.swell_btn > 0.0f)
        lb |= 1u << B_SEQ;
    frasil_view(v);
    for (i = 0; i < FR_NV; i++)
        if (v[i].note >= 0)
            lb |= 1u << B_PLAY;
    {                                    /* and the bass */
        bass_view_t bv;
        frasil_bass_view(&bv);
        if (bv.note >= 0)
            lb |= 1u << B_PLAY;
    }
    keys_lights(&kb, &kd);
    lights_set(lb, mb & ~lb, gb & ~(lb | mb), kb, kd);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------- the screen --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
} say;

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        say.msg[0][i] = a[i];
    say.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        say.msg[1][i] = b[i];
    say.msg[1][i] = 0;
    say.msg_until = plat_ms() + 1300u;
    if (ann_on()) {                          /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, say.msg[0]);
        if (say.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, say.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

#include "ui.c"

void app_ui_init(void)
{
    touch_on(1);                             /* velocity and pressure: the keys' readings on */
    st.logo = 1;                             /* the logo a moment, alive, then the river */
    st.logo_until = plat_ms() + 4000u;
    ui_init();
}

void app_frame(void)
{
    input();
    load_watch();
    ui_frame();
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    frasil_view_t v[FR_NV];
    glint_view_t s[GLINT_NV];
    uint32_t i;
    int n = 0, g = 0, sp = 0;
    frasil_view(v);
    frasil_glint_view(s);
    for (i = 0; i < FR_NV; i++) {
        n += v[i].note >= 0;
        g += v[i].note >= 0 && v[i].gate;
    }
    for (i = 0; i < GLINT_NV; i++)
        sp += s[i].note >= 0;
    if (!strcmp(what, "voices"))             /* sounding (held or fading) */
        *got = n;
    else if (!strcmp(what, "held"))
        *got = g;
    else if (!strcmp(what, "glints"))        /* glints ringing */
        *got = sp;
    else if (!strcmp(what, "glintlvl")) {   /* the loudest glint's level x 100, and its note (glintnote) */
        int best = 0;
        for (i = 0; i < GLINT_NV; i++)
            if (s[i].note >= 0 && (int)(s[i].level * 100.0f) > best)
                best = (int)(s[i].level * 100.0f);
        *got = best;
    } else if (!strcmp(what, "glintnote")) {
        int nn = -1;
        for (i = 0; i < GLINT_NV; i++)
            if (s[i].note >= 0)
                nn = s[i].note;
        *got = nn;
    } else if (!strncmp(what, "kprs", 4))     /* kprsN: key N's pressure last sent */
        *got = key[atoi(what + 4) % NKEYS].prs;
    else if (!strcmp(what, "arpsteps"))     /* the arpeggiator's steps played */
        *got = (int)arp.steps;
    else if (!strcmp(what, "glint"))         /* the keys play GLINT */
        *got = st.glint;
    else if (!strcmp(what, "bass"))          /* the keys play BASS */
        *got = st.bass;
    else if (!strcmp(what, "bassnote"))      /* the bass's note held, -1 none */
        *got = bass_note;
    else if (!strcmp(what, "bassvoice")) {   /* the bass sounding: its note (held or fading), -1 silent */
        bass_view_t bv;
        frasil_bass_view(&bv);
        *got = bv.note;
    } else if (!strcmp(what, "basslvl")) {   /* its envelope x 100 */
        bass_view_t bv;
        frasil_bass_view(&bv);
        *got = (int)(bv.level * 100.0f);
    } else if (!strcmp(what, "bprs")) {      /* its pressure x 100 */
        bass_view_t bv;
        frasil_bass_view(&bv);
        *got = (int)(bv.pressure * 100.0f + 0.5f);
    } else if (!strcmp(what, "recled") || !strcmp(what, "fxled") || !strcmp(what, "octled") ||
               !strcmp(what, "saveled") || !strcmp(what, "playled")) {
        uint32_t b = what[0] == 'r' ? B_REC : what[0] == 'f' ? B_FX : what[0] == 's' ? B_SAVE : what[0] == 'p' ? B_PLAY
                                                                                                         : B_OCTUP;
        /* 0 off, 1 glow, 2 mid (OCT: its softer blink), 3 lit */
        *got = (leds_now >> b & 1u) ? 3 : (midleds_now >> b & 1u) ? 2 : b == B_OCTUP && oct_soft ? 2
             : (glowleds_now >> b & 1u) ? 1 : 0;
    } else if (!strcmp(what, "clicks")) {    /* the metronome's ticks; metro: it on */
        *got = (int)st.clicks;
    } else if (!strcmp(what, "metro")) {
        *got = st.metro;
    } else if (!strcmp(what, "sparkles")) {  /* SPARKLE's glints played */
        *got = (int)spk.n;
    } else if (!strcmp(what, "compgr")) {    /* the compressor's reduction, dB x 10 */
        *got = (int)(frasil_comp_gr() * 10.0f);
    } else if (!strcmp(what, "pumpg")) {     /* the drones' pump gain now x 100 */
        *got = (int)(frasil_pump_gain() * 100.0f + 0.5f);
    } else if (!strcmp(what, "pumplow")) {   /* its lowest since last asked x 100 */
        *got = (int)(pump_low * 100.0f + 0.5f);
        pump_low = 1.0f;
    } else if (!strncmp(what, "meter", 5)) { /* meterN: track N's meter x 100 (0 drones, 1 bass, 2 glints, 3 cloud) */
        float m[4];
        frasil_meters(m);
        *got = (int)(m[atoi(what + 5) & 3] * 100.0f);
    } else if (!strcmp(what, "glintcents")) {   /* the largest VIBRATO bend among the glints, cents x 10 */
        int best = 0;
        for (i = 0; i < GLINT_NV; i++) {
            int c = (int)(fm_fabsf(s[i].cents) * 10.0f);
            if (s[i].note >= 0 && c > best)
                best = c;
        }
        *got = best;
    }
    else if (!strcmp(what, "arplatch"))
        *got = arp.latch;
    else if (!strcmp(what, "arpnotes"))    /* notes the arpeggio is asked for */
        *got = arp.n;
    else if (!strcmp(what, "kbright") || !strcmp(what, "kdim")) {   /* the keys' lights: bit k = key k */
        uint32_t kb, kd;
        keys_lights(&kb, &kd);
        *got = (int)(what[1] == 'b' ? kb : kd);
    } else if (!strcmp(what, "arpplay"))     /* notes it plays (they change on its steps) */
        *got = arp.pn;
    else if (!strcmp(what, "help"))
        *got = st.help;
    else if (!strcmp(what, "paused"))    /* the arpeggio waiting (PLAY tapped) */
        *got = arp.paused;
    else if (!strcmp(what, "playhold"))  /* PLAY held, counting down on the screen */
        *got = play_holding() > 0.0f;
    else if (!strcmp(what, "morph"))     /* a scene's MORPH under way */
        *got = mo.on;
    else if (!strcmp(what, "compare"))   /* EDIT held: the scene as saved sounding */
        *got = st.compare;
    else if (!strncmp(what, "used", 4))  /* usedN: slot N has a saved scene */
        *got = scn.used >> (atoi(what + 4) % NSCENES) & 1u;
    else if (!strcmp(what, "logo"))      /* the logo's screen up */
        *got = st.logo;
    else if (!strcmp(what, "seqrun"))    /* the sequencer playing */
        *got = sq.run;
    else if (!strcmp(what, "seqpos"))    /* its step now, -1: none yet */
        *got = sq.pos;
    else if (!strcmp(what, "seqsteps"))  /* the notes it has played */
        *got = (int)sq.steps;
    else if (!strcmp(what, "seqnotes")) {   /* its steps that hold a note */
        uint32_t i;
        *got = 0;
        for (i = 0; i < SC_SEQ; i++)
            *got += (seq[i] & 127u) != SQ_REST && (seq[i] & 127u) != SQ_TIE;
    } else if (!strncmp(what, "seqstep", 7))   /* seqstepN: step N's byte (its note, + 128 with ACCENT) */
        *got = seq[atoi(what + 7) % SC_SEQ];
    else if (!strncmp(what, "lfoon", 5)) /* lfoonN: LFO N (0, 1) running */
        *got = lfo[(uint32_t)(what[5] - '0') % NLFO].on;
    else if (!strncmp(what, "lfopeak", 7)) {   /* lfopeakN: its largest offset since last asked, x 100 */
        uint32_t i = (uint32_t)(what[7] - '0') % NLFO;
        *got = (int)(lfo[i].peak * 100.0f + 0.5f);
        lfo[i].peak = 0.0f;
    }
    else if (!strcmp(what, "helppage"))
        *got = st.help_page;
    else if (!strcmp(what, "page"))
        *got = st.page;
    else if (!strcmp(what, "octave"))
        *got = st.octave;
    else if (!strcmp(what, "latch"))
        *got = !st.v[P_KEYS];
    else if (!strcmp(what, "freeze"))
        *got = st.v[P_FREEZE];
    else if (!strcmp(what, "dirty"))
        *got = st.dirty;
    else if (!strcmp(what, "scene"))
        *got = st.scene;
    else if (!strcmp(what, "shed"))
        *got = frasil_shed_level();
    else if (!strncmp(what, "lit", 3))       /* litN: a chord on root N latched */
        *got = bit_get(lit, atoi(what + 3));
    else if (!strncmp(what, "on", 2))        /* onN: note N sounding (held by a chord) */
        *got = held_by[atoi(what + 2) & 127];
    else if (!strncmp(what, "p", 1) && what[1] >= '0' && what[1] <= '9')   /* pN: parameter N's value */
        *got = st.v[atoi(what + 1) % NPARAM];
    else
        return 0;
    return 1;
}

/* the tests' own command: "oldstore L" saves the records in an earlier layout, L: 3 FRSV + FRS3 (before BASS), 4 FRSW
 * + FRS4 (SPACE, 10, where MODEL is now), 5 FRSX + FRS5, 6 FRSX + FRS6 (GLINT on channel 5, BASS on 6), 7 FRSY + FRS7,
 * 8 FRSZ + FRS8, 9 FRSa + FRS9, 10 FRSb + FRSA, 11 FRSc + FRSB, 12 FRSd + FRSC, 13 FRSe + FRSD, 14 FRSf + FRSE (the
 * same channels):
 * slot 6 a scene
 * named OLD with a chord on D3 (and the bass on A1), the settings with TEMPO at 100 BPM and slot 6 last; for a reboot
 * to read and make over */
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    static const uint8_t SCN_OF[12] = {0, 1, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10};   /* L's scenes layout */
    int k = atoi(a) - 3;
    const layout_t *ls, *lt;
    uint32_t i, rs, nv;
    uint8_t *o;
    (void)b;
    (void)c;
    (void)d;
    if (strcmp(cmd, "oldstore") || k < 0 || k > 11)
        return 0;
    ls = &OLD_SCN[SCN_OF[k]];
    lt = &OLD_SET[k];
    rs = scn_bytes(ls);
    nv = ls->nv;
    memset(raw, 0, sizeof raw);
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(ls->magic >> (8u * i));
    raw[4] = 1u << 5;
    o = raw + 8u + 5u * rs;
    memcpy(o, "OLD", 3);
    for (i = 0; i < nv; i++)
        o[10u + i] = st.v[i];
    if (nv == 64u)
        o[10u + P_BS_MODEL] = 10;
    o[10u + nv] = 1;                     /* nch */
    o[12u + nv] = o[13u + nv] = 50;      /* the chord: root D3, D3 */
    o[14u + nv] = o[15u + nv] = o[16u + nv] = 0xFF;
    if (ls->extra)
        o[12u + nv + SC_CHORDS * 5u + SC_ARP] = 33;
    plat_store_save(OBJ_SCENES, raw, 8u + NSCENES * rs);
    nv = lt->nv;
    memset(raw, 0, sizeof raw);
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(lt->magic >> (8u * i));
    for (i = 0; i < nv; i++)
        raw[4u + i] = st.v[i];
    if (nv == 64u)
        raw[4u + P_BS_MODEL] = 10;
    o = raw + 4u + nv;
    o[0] = 0xFF;                         /* octave -1 */
    o[2] = lt->extra ? 1 : 0;
    o[3] = 5;
    o[4] = 70;
    if (lt->extra) {
        o[5] = 5;
        o[6] = 6;
    }
    plat_store_save(OBJ_SETTINGS, raw, set_bytes(lt));
    return 1;
}
#endif
