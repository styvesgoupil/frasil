/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Copyright (C) 2026 Charles Vestal (from fm1-base's template) */
/* FRASIL: the app. The keys light drones: a tap latches a chord (the harmony makes one of the note: harmony.c), a
 * tap on a lit one lets it go, holding it down presses into it. SELECT turns the keys to GLINT, the icy glints played
 * over the drones (never latched; ARP is their pages, held their arpeggio's latch), or to BASS, one deep note held
 * under them (SEQ is its pages). The three left encoders are the performance's hands (KEYS, the drone's PRESET,
 * BODY), K1 .. K4 the page's parameters; PLAY lets every note go, ENV held swells the drone's voices, REC records
 * into GLINT's sequencer (held: its metronome), FREEZE (CLOUD's K4) holds the cloud. MIDI in plays the drones too, through the same
 * harmony. Nothing plays by itself (SPARKLE's glints answer what the drones hold).
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in
 * scope. The sound is in dsp/ (compiled apart, fm1.toml o2). */
#include <stddef.h>
#include "fastmath.h"
#include "frasil.h"
#include "params.c"

#define SET_MAGIC 0x4C535246u            /* "FRSL": the settings record (a new one when its layout changes) */
#define SET_FRSK 0x4B535246u             /* "FRSK": 2.5's betas' (b6 .. b9): this one, its sound FRSO_NV long, without
                                          * ANIMATIONS (FRSK_BYTES), the settings learned numbered so */
#define FRSO_NV 146u                     /* the sound before ROUTING and the pans (FRSo, FRSK, FRSp) */
#define FRSK_BYTES 244u
#define SET_FRSJ 0x4A535246u             /* "FRSJ": the one before MASTER EQ and MASTER FILTER: this one, its sound
                                          * FRSL_NV long (FRSJ_BYTES), the settings learned numbered so */
#define FRSL_NV 130u                     /* the sound before MASTER EQ and MASTER FILTER (FRSl, FRSJ, FRSn) */
#define FRSJ_BYTES 228u
#define SET_FRSI 0x49535246u             /* "FRSI": the one before GLINT's RELEASE: this one, SCATTER where RELEASE is */
#define SET_FRSH 0x48535246u             /* "FRSH": the one before BASS's DECAY, SUSTAIN and PULSE: this one, its sound
                                          * FRSH_NV long (FRSH_BYTES), the settings learned numbered so */
#define SET_FRSG 0x47535246u             /* "FRSG": the one before the bass's sequence, FRSH's first FRSH_BSEQ bytes
                                          * (FRSG_BYTES with its padding) */
#define FRSH_NV 127u
#define FRSH_BYTES 224u
#define FRSH_BSEQ (FRSH_BYTES - 2u * SC_BSEQ - 3u)
#define FRSG_BYTES 192u
#define SCN_MAGIC 0x71535246u            /* "FRSq": the scenes record (with their notes and the bass; names of
                                          * sixteen), each scene up to GLINT's sequence (SC_REC): the sequences in
                                          * OBJ_BSEQ (the sound outgrew the record) */
#define SCN_FRSO 0x6F535246u             /* "FRSo": 2.5's betas' (OLD_SCN's last): this one, its sound FRSO_NV long */
#define SCN_FRSL 0x6C535246u             /* "FRSl": the one before MASTER EQ and MASTER FILTER (OLD_SCN's last, the
                                          * sequence in each scene's record), GLINT's RELEASE in it */
#define SCN_FRSJ 0x6A535246u             /* "FRSj": the one before GLINT's RELEASE (OLD_SCN's last): this one, SCATTER
                                          * where RELEASE is */
#define SCN_FRSH 0x68535246u             /* "FRSh": the one before BASS's DECAY, SUSTAIN and PULSE (OLD_SCN's last) */
#define BSQ_MAGIC 0x55425246u            /* "FRBU": the scenes' sequences (OBJ_BSEQ), each GLINT's, then the bass's
                                          * notes and lengths, SEQUENCE, SEQ STEPS, TRANSPOSE, and the scene's TEMPO */
#define BSQ_FRBT 0x54425246u             /* "FRBT": the one before (with FRSl): from the bass's notes on */
#define BSQ_FRBS 0x53425246u             /* "FRBS": the one before the scenes' TEMPO (FRBT's less one a scene) */
#define SHARD_MAGIC 0x72535246u          /* "FRSr": a scene whole (scene_t: its sequences and its TEMPO after the
                                          * rest), as the River keeps it, a shard; "FRSp": 2.5's betas' (FRSO_NV
                                          * sounds); "FRSn": the one before MASTER EQ and
                                          * MASTER FILTER (FRSL_NV sounds); "FRSm": that without its TEMPO;
                                          * "FRSk" shards: without it, SCATTER where GLINT's
                                          * RELEASE is; "FRSi": before BASS's DECAY, SUSTAIN and PULSE too (FRSH_NV
                                          * sounds); "FRSh": those without the bass's too */
#define SHARD_FRSP 0x70535246u
#define SHARD_FRSN 0x6E535246u
#define SHARD_FRSM 0x6D535246u
#define SHARD_FRSK 0x6B535246u
#define SHARD_FRSI 0x69535246u
#define OCT_DEF (-1)                     /* the keys start at F2: drones live low */
#define TAP_MS 350u                      /* a lit key let go sooner than this, without pressing in: it goes out */
#define HELP_PAGES 5                     /* the help: three of buttons, the encoders, the keys */
#define HOLD_MS 400u                     /* a button held this long does its other thing (on release, a tap its own) */
#define PLAY_HOLD_MS 600u                /* PLAY's, which lets everything go: longer, the screen counting it down */
#define PLAY_SHOW_MS 200u                /* from here the screen counts it down, and letting go does nothing */
#define PLAY_TWICE_MS 350u               /* PLAY tapped again within it, while anything played: RELEASE ALL */
#define NLEARN 8u                        /* MIDI CCs learned (GLO held, a CC) */

typedef struct {
    uint32_t magic;
    uint8_t v[NSOUND];                   /* the sound as last saved */
    int8_t octave;
    uint8_t keys, channel, scene, tempo; /* channel: the drones' (P_CANAL's value); scene: the slot, 0 .. 15, or
                                          * SC_NEW, 16: NEW SHARD (read % 16, as an earlier FRASIL and the River
                                          * read it: slot 01) */
    uint8_t ch_glint, ch_bass;
    uint8_t seq[SC_SEQ];                 /* GLINT's sequence as last saved */
    uint8_t metro_off;                   /* REC's metronome off (0, as a record without it reads: on) */
    uint8_t speaker_off, load;           /* SPEAKER off (SCENES' MORPH once, written 0 since it starts on CUT) and
                                          * SCENES' LOAD */
    uint8_t learn_cc[NLEARN], learn_p[NLEARN];   /* the CCs learned and their settings (cc 0: none) */
    uint8_t bnote[SC_BSEQ], blen[SC_BSEQ];   /* the bass's sequence as last saved */
    uint8_t bq_on, bq_steps, bq_trans;   /* and its SEQUENCE, SEQ STEPS, TRANSPOSE */
    uint8_t anim;                        /* ANIMATIONS LOW (0, as FRSK's padding byte reads: NORMAL) */
    uint8_t clock;                       /* CLOCK INTERNAL (0, as FRSL's padding byte reads till 2.5-b30: AUTO) */
} settings_t;

typedef struct {
    uint32_t magic;
    uint16_t used;                       /* bit s: slot s has a saved scene */
    uint16_t rsv;
    scene_t s[NSCENES];
} scenes_t;                              /* (in the store as two records: scenes_save) */

static settings_t set;
static scenes_t scn;

/* the records of earlier layouts, read byte by byte and made over into the new ones (the parameters added since at
 * their defaults). A scene: its name (OLD_NAME, or the layout's own), the sound (nv), nch, narp, the chords (8 x 5), the
 * arpeggio (16), the bass note (if the layout has one); the scenes' record: magic, used, rsv, 16 scenes. The settings:
 * magic, the sound, octave, keys, channel (one for all: 0 OMNI), scene, tempo, the GLINT and BASS channels (if it has
 * them), to 4 (FRSH's and FRSG's: settings_over) */
typedef struct {
    uint32_t magic;
    uint8_t nv, extra;                   /* sound parameters; extra 1: scenes a bass note, settings two more channels;
                                          * 2: and the sequence (SC_SEQ); 3 (scenes): and names of SC_NAME */
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
    {0x67535246u, 127, 2},               /* "FRSg": with GLINT's SEQ, GENERATIVE FX, its names of ten */
    {SCN_FRSH, FRSH_NV, 3},              /* "FRSh": with names of sixteen */
    {SCN_FRSJ, FRSL_NV, 3},              /* "FRSj": with BASS's DECAY, SUSTAIN and PULSE (SCATTER where GLINT's
                                          * RELEASE is) */
    {SCN_FRSL, FRSL_NV, 4},              /* "FRSl": with GLINT's RELEASE (extra 4: none to make over) */
    {SCN_FRSO, FRSO_NV, 5},              /* "FRSo": with MASTER EQ, MASTER FILTER and LOFI TAPE (extra 5: GLINT's
                                          * sequence in OBJ_BSEQ already, FRBU's) */
};
#define OLD_NAME 10u                     /* their names: ten characters (NUL-padded, none when all ten were used) */
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
    {0x46535246u, 127, 2},               /* "FRSF": with GLINT's SEQ and GENERATIVE FX (METRONOME after the sequence) */
};
/* the scenes in the store: OBJ_SCENES, the scenes' record (magic, used, rsv, each scene's first SC_REC bytes: up to
 * GLINT's sequence), and OBJ_BSEQ, the rest of each (magic, each scene's sequence, bnote, blen ... TEMPO). A scene_t is
 * the two records' bytes one after the other */
#define SC_TAIL (2u + SC_CHORDS * 5u + SC_ARP + 1u + SC_SEQ)   /* after the sound, to the bass's sequence: nch .. seq */
#define SC_REC (SC_NAME + NSOUND + SC_TAIL - SC_SEQ)
#define FRSL_REC (SC_NAME + FRSL_NV + SC_TAIL)   /* FRSl's and FRSj's (the sequence in them) */
#define FRSH_REC (SC_NAME + FRSH_NV + SC_TAIL)   /* FRSh's (its sound FRSH_NV long) */
#define SCN_BYTES (8u + NSCENES * SC_REC)
#define FRBT_SCENE (2u * SC_BSEQ + 4u)   /* FRBT's: a scene's bass, its notes, its lengths, SEQUENCE, SEQ STEPS,
                                          * TRANSPOSE, TEMPO */
#define BSQ_SCENE (SC_SEQ + FRBT_SCENE)  /* a scene's in OBJ_BSEQ: GLINT's sequence, then FRBT's */
#define FRBS_SCENE (FRBT_SCENE - 1u)     /* FRBS's: without TEMPO */
#define BSQ_BYTES (4u + NSCENES * BSQ_SCENE)
static uint8_t raw[8u + NSCENES * FRSL_REC];   /* an old record as it was saved (the largest, FRSl's scenes: 3800),
                                                * and the scenes' records as they are written and read (FRSo's:
                                                * 3480) */
static uint8_t store_old;                /* bit 0 the scenes, bit 1 the settings: made over from an earlier layout at
                                          * load, the store's still in it (a backup saves them first) */
_Static_assert(sizeof(scene_t) == SC_REC + BSQ_SCENE && offsetof(scene_t, seq) == SC_REC &&
                   offsetof(scene_t, bnote) == SC_REC + SC_SEQ && offsetof(scene_t, nch) == SC_NAME + NSOUND &&
                   offsetof(scene_t, bq_on) == SC_REC + SC_SEQ + 2u * SC_BSEQ &&
                   offsetof(scene_t, tempo) == SC_REC + SC_SEQ + FRBS_SCENE,
               "a scene: its bytes, no padding, the sequences last");
_Static_assert(P_BS_DRIVE == 55 && P_BS_RESO == 64 && P_PUMP_RATE == 65 && P_SH_TONE == 77 && P_PUMP_REL == 78 &&
                   P_CH_SHAPE == 81 && P_L1_SHAPE == 94 && P_SQ_LEN == 110 && P_DR_PRESET == 113 && P_L3_SHAPE == 115 &&
                   P_SQ_TYPE == 123 && P_BS_DECAY == FRSH_NV && P_EQ_PRESET == FRSL_NV && P_FL_PRE == FRSO_NV,
               "the parameters added come after the old");
_Static_assert(FACTORY_NV == FRSL_NV, "the factory's whole sounds: FRSl's");
_Static_assert(SCN_BYTES <= 3840u && BSQ_BYTES <= 3840u, "the scenes' records fit a store object each (4 KiB less "
                                                         "its 256-byte header)");
_Static_assert(offsetof(settings_t, clock) == offsetof(settings_t, anim) + 1u &&
                   offsetof(settings_t, anim) == offsetof(settings_t, bq_trans) + 1u &&
                   sizeof(settings_t) == ((offsetof(settings_t, anim) + 4u) & ~3u) &&
                   FRSK_BYTES == ((4u + FRSO_NV + offsetof(settings_t, anim) - offsetof(settings_t, octave) + 3u) & ~3u) &&
                   FRSJ_BYTES == ((4u + FRSL_NV + offsetof(settings_t, anim) - offsetof(settings_t, octave) + 3u) & ~3u) &&
                   offsetof(settings_t, bnote) == 192u + (NSOUND - FRSL_NV) &&
                   offsetof(settings_t, octave) ==
                   4u + NSOUND && offsetof(settings_t, bq_trans) + 1u - offsetof(settings_t, octave) ==
                   FRSH_BYTES - 4u - FRSH_NV && FRSH_BSEQ == 189u,
               "the settings: FRSH's (FRSG's: FRSF's 172 bytes, MORPH, LOAD and the learned CCs; 192 with its padding; "
               "then the bass's sequence), the sound three settings longer");

_Static_assert(sizeof raw >= SCN_BYTES && sizeof raw >= BSQ_BYTES && sizeof raw >= sizeof(settings_t) &&
                   sizeof raw >= 8u + NSCENES * FRSH_REC, "raw holds the largest");
static uint32_t name_bytes(const layout_t *l) { return l->extra >= 3u ? SC_NAME : OLD_NAME; }
static uint32_t seq_bytes(const layout_t *l) { return l->extra >= 2u && l->extra != 5u ? SC_SEQ : 0u; }
static uint32_t scn_bytes(const layout_t *l)
{
    return name_bytes(l) + l->nv + 2u + SC_CHORDS * 5u + SC_ARP + (l->extra ? 1u : 0u) + seq_bytes(l);
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
                                          * and a key or REC, LFO and an encoder, GLO and a CC, a page button and
                                          * SELECT): no tap on release */
    uint32_t hold_acted;                 /* bit b: b's hold has acted, this press (SELECT turned with it undoes it) */
    uint8_t prev_page;                   /* the page SCENES came from (SAVE again goes back) */
    uint8_t kb_rec;                      /* KEYS before RECORD turned them to its track (RECORD OFF gives them back) */
    uint8_t rec_took;                    /* a recording under way has the keys (KEYS to give back: rec_keys) */
    uint8_t loading_morph;               /* scene_go loading a scene to MORPH to */
    uint8_t compare;                     /* EDIT held: the scene as saved sounds, not as it is now */
    uint32_t keys_arm_at;                /* GLOBAL's KEYS: the first detent, waiting for the second */
    int8_t keys_arm;
    uint8_t metro;                       /* REC's metronome: a tick on each beat while a sequencer records LIVE */
    uint32_t clicks, hiclicks;           /* its ticks since RECORD went on, and the higher ones (the tests) */
    uint8_t help, help_page;             /* HOME held: the help on the screen until HOME again; SELECT its pages */
    uint8_t touched_enc;                 /* the encoder that turned it (EN_*: the screen's badge) */
    uint8_t touched;                     /* the parameter turned last (the screen shows it a moment) */
    uint32_t touched_at;
    uint32_t enc_at[NE];                 /* the last detent of each encoder (acceleration) */
    uint8_t engine_dirty;
    uint8_t store_hold;                  /* a backup streaming to USB: nothing saved until it ends (app_backup_hold) */
    uint8_t prefs_due;                   /* prefs_save asked: 1 once nothing sounds (prefs_quiet), 2 now (SPK/JACK) */
    uint8_t booting;                     /* app_init done, the UI not started yet (app_ui_init: the logo) */
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
static uint8_t spk_page;                 /* a PERFORM page plays the FM-1's USB audio (COMPUTER AUDIO): its own output
                                          * quiet meanwhile, SPK/JACK as it is (the page can only quiet it) */

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

enum { K_NONE, K_LATCH, K_HOLD, K_GLINT, K_BASS, K_BSEQ };   /* (K_BSEQ: BASS's, into its sequence) */
static struct {                          /* each key while it is down */
    uint8_t kind;                        /* what it started as (the mode may change while it is held) */
    int8_t root;                         /* K_LATCH: its chord's root; K_GLINT: the glint's note; K_BASS, K_BSEQ: the
                                          * bass's */
    int8_t gbase;                        /* K_GLINT: its note without GLINT's OCTAVE (the arpeggio's, the sequence's);
                                          * K_BSEQ: without BASS's (the bass's sequence's) */
    uint8_t gsound;                      /* K_GLINT: it sounded its glint itself (SEQ OFF, or RECORD on): its release
                                          * lets it go, whatever SEQ and RECORD are by then */
    chord_t chord;                       /* K_HOLD: the chord it holds */
    uint8_t was_lit;                     /* it was latched already when pressed: a tap lets it go */
    uint8_t pressed_in;                  /* it was pressed into: no tap */
    uint8_t prs;                         /* its pressure last sent */
    uint32_t t;
} key[NKEYS];
static uint32_t keys_prev, btn_prev, btn_now;

static int b_held(uint32_t b) { return btn_now >> b & 1u; }
static void b_used(uint32_t b) { st.hold_done |= 1u << b; }   /* (its release no tap) */
static uint32_t leds_now, midleds_now, glowleds_now, keyleds_now;   /* the lights last set (the tests) */
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
    keyleds_now = keys | keys_glow;
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

/* ------------------------------------------------------------ a pop-up --- */
/* a warning (something did not happen, or wants a hand), or what a restore from USB did: over the whole screen
 * (ui.c popup_screen) until SEL (OK) closes it, the page staying. Meanwhile the keys and MIDI play; the other buttons
 * and the encoders wait (their presses, holds and turns dropped). The gestures' words stay in the header (app_say) */
enum { POP_WARN, POP_DONE };
static struct {
    uint8_t on, kind, drawn;
    uint32_t btn;                        /* the buttons down last frame: SEL pressed closes it */
    char kicker[24], title[16], line[3][32];
} pop;

static void pop_copy(char *d, const char *s, uint32_t n)
{
    uint32_t i;
    for (i = 0; s && s[i] && i + 1u < n; i++)
        d[i] = s[i];
    d[i] = 0;
}

static void popup(int kind, const char *kicker, const char *title, const char *l1, const char *l2, const char *l3)
{
    char t[128];
    uint32_t i;
    pop_copy(pop.kicker, kicker, sizeof pop.kicker);
    pop_copy(pop.title, title, sizeof pop.title);
    pop_copy(pop.line[0], l1, sizeof pop.line[0]);
    pop_copy(pop.line[1], l2, sizeof pop.line[1]);
    pop_copy(pop.line[2], l3, sizeof pop.line[2]);
    pop.kind = (uint8_t)kind;
    pop.on = 1;
    pop.drawn = 0;
    pop.btn = plat_buttons();            /* (SEL already down closes it only pressed again) */
    st.hold_done = ~0u;                  /* the buttons down now: no tap, no hold */
    if (ann_on()) {                      /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, pop.title);
        for (i = 0; i < 3u; i++)
            if (pop.line[i][0]) {
                text_cat(t, " ");
                text_cat(t, pop.line[i]);
            }
        text_cat(t, " SEL: OK.");
        ann_say(t, 0, ANN_NOW);
    }
}

/* a pop-up up, a frame's input: SEL pressed closes it (its release and its hold nothing); the encoders' turns
 * dropped. 1: still up (the buttons, for the rest of the frame, none) */
static int popup_input(uint32_t btn)
{
    uint32_t down = btn & ~pop.btn, e;
    pop.btn = btn;
    for (e = 0; e < NE; e++)
        plat_enc((int)e);
    if (!(down >> B_SEL & 1u))
        return 1;
    pop.on = 0;
    btn_prev = btn;                      /* (what is down now: used, no tap on its release) */
    st.hold_done = ~0u;
    return 0;
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

/* GLINT's step sequencer (GLINT I's SEQ on SEQ): SC_SEQ steps, a byte each, its note (SQ_REST, a note, or SQ_TIE: the
 * last one held on) and SQ_ACC; SEQ LENGTH of them, played on the arpeggiator's grid (RATE, CHANCE) from the step it
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
    uint8_t off;                         /* its steps this many clocks after the grid's (a key's restart on a beat) */
    uint8_t at_trans;                    /* TRANSPOSE from that restart */
    float at;                            /* a key's restart, due on the grid's beat at this clock (-1: none) */
    int32_t start;                       /* its step its first step fell on (its steps: seq_stepn) */
    int32_t n;                           /* its step at the last run */
    int32_t pos;                         /* the step played last (-1: none yet) */
    int32_t skip;                        /* a step LIVE has just written ahead of the playhead: it sounded already */
    int16_t sounding;                    /* its note sounding, -1: none */
    uint32_t off_ms;
    uint32_t steps;                      /* notes played (the tests) */
} sq = {.at = -1.0f, .pos = -1, .skip = -1, .sounding = -1};

/* the grid as GLINT's sequence has it, its steps sq.off clocks after the grid's (wrapping at 9600 as it does), and
 * its step at grid g */
static float seq_grid(float g)
{
    g -= (float)sq.off;
    return g < 0.0f ? g + 9600.0f : g;
}
static int32_t seq_stepn(float g, uint32_t ticks) { return (int32_t)(seq_grid(g) / (float)ticks); }

/* the bass's sequence (SEQUENCE on): SC_BSEQ steps, each its note (bnote: SQ_REST a CUT, the bass stops; SQ_TIE a
 * HOLD, it goes on; else a NOTE, as BASS's keys have it, without its OCTAVE) and its STEP LENGTH (blen: BQ_TICKS'
 * index); SEQ STEPS of them in turn on the arpeggiator's grid (MIDI clock, else TEMPO), from the top with GLINT's
 * sequence. A NOTE sounds until the next NOTE or CUT. The keys transpose it along the scale (RECORD OFF), or write
 * into it (STEP: at STEP, then on, each the STEP LENGTH set last; LIVE: a take, each note as it was played, to the
 * sixteenth, over what was there: bseq_take) */
static uint8_t bnote[SC_BSEQ], blen[SC_BSEQ];

static int bseq_has_notes(void)          /* the bass's sequence has a NOTE in its SEQ STEPS */
{
    uint32_t i, n = st.v[P_BQ_STEPS] + 1u;
    for (i = 0; i < n && i < SC_BSEQ; i++)
        if ((bnote[i] & 127u) != SQ_REST && (bnote[i] & 127u) != SQ_TIE)
            return 1;
    return 0;
}

/* BASS's keys (and notes) into its sequence: SEQUENCE on, RECORDing, or RECORD OFF with notes to transpose (an empty
 * one: the keys play the bass, as with SEQUENCE off) */
static int bseq_keys(void) { return st.v[P_BQ_ON] && (st.v[P_BQ_REC] || bseq_has_notes()); }

static struct {
    uint8_t run;                         /* playing */
    uint8_t len;                         /* STEP: the STEP LENGTH a step written takes (K3's meanwhile), carried
                                          * on: the step's on STEP as STEP began, then K3's as it turns */
    uint8_t wait;                        /* begun: its first step at the grid's next step (1: GLINT's, else a
                                          * sixteenth; 2: at once, MIDI Start's top) */
    int8_t pos;                          /* the step playing, -1: none yet */
    float t;                             /* MIDI clocks into it */
    float g;                             /* the grid at the last run */
    int8_t tn;                           /* LIVE: the take's note down (as the sequence keeps it), -1: none; its
                                          * steps silent under it */
    float tp, te;                        /* where in the loop it went down (clocks from its first step; before it
                                          * began: 0), and the clocks since */
    uint8_t open;                        /* LIVE, a take on an empty loop: from its first note's bar, a bar longer
                                          * each time it ends (bseq_open; 0: the loop as it is, the take within it,
                                          * round) */
    uint8_t wrap;                        /* (open: its first note just before a bar line, its bar the next one:
                                          * the loop comes round once more first) */
    int8_t skip;                         /* a step the take has just written ahead of the playhead: it sounded
                                          * already, let go (-1: none) */
    uint8_t moved;                       /* the playhead put elsewhere in time (a take's loop opened, closed): the
                                          * metronome on from there, no tick for it */
    uint32_t steps;                      /* NOTEs played (the tests) */
} bq = {.pos = -1, .tn = -1, .skip = -1};
#define BQ_BAR 6u                        /* (BQ_TICKS' 1 BAR: a take's loop grows and closes by it) */
#define BQ_CELLS 4096u                   /* the longest loop, in sixteenths (16 steps of 16 BARS) */
static uint8_t bq_tl[BQ_CELLS];          /* LIVE: the loop laid out, a sixteenth a byte (bseq_lay) */

/* GLINT's pages: GLINT ARP only while its SEQ (GLINT I) is on ARP, the SEQ pages only while it is on SEQ */
static int page_hidden(uint32_t p)
{
    if (p == PG_ARP)
        return st.v[P_SQ_TYPE] != SQT_ARP;
    return (p == PG_SEQ || p == PG_SEQ2 || p == PG_SEQ3) && st.v[P_SQ_TYPE] != SQT_SEQ;
}

/* GLINT's keys into its sequencer (seq_key): the sequencer on, recording (from any page), or not on GLINT SEQ's pages
 * (they transpose it). On those, RECORD OFF, each key plays its glint, as with SEQ OFF: a line tried before it is
 * recorded, the sequence going on as it was */
static uint32_t seq_len(void);           /* (below) */

static int seq_has_notes(void)           /* GLINT's sequence has a note in the steps it plays */
{
    uint32_t i, n = seq_len();
    for (i = 0; i < n && i < SC_SEQ; i++)
        if ((seq[i] & 127u) != SQ_REST && (seq[i] & 127u) != SQ_TIE)
            return 1;
    return 0;
}

/* GLINT's keys (and notes) into its sequencer: SEQ on SEQ, RECORDing; RECORD OFF, away from its pages, a sequence
 * with notes to transpose (an empty one: the keys play their glints, as with SEQ OFF) */
static int seq_keys(void)
{
    return st.v[P_SQ_TYPE] == SQT_SEQ &&
           (st.v[P_SQ_REC] || ((st.page < PG_SEQ || st.page > PG_SEQ3) && seq_has_notes()));
}

/* -------------------------------------------------------------- engine --- */
static float delay_s_now = 0.25f;        /* GLINT's delay time from the tempo (delay_sync) */

static uint8_t saved_v[NSOUND], saved_seq[SC_SEQ], saved_bnote[SC_BSEQ], saved_blen[SC_BSEQ], saved_bq[4];   /* (the sound as last
                                                                                               * loaded or saved) */
static void drone_resolve(uint8_t *v);

static void engine_push(void)
{
    frasil_params_t b;
    seq_shift = 12 * (int32_t)st.v[P_GL_OCTAVE];   /* (NOTE shows the steps as they sound) */
    bseq_shift = 12 * ((int32_t)st.v[P_BS_OCTAVE] - 2);
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

/* a bass note as it sounds: OCTAVE on it (the engine puts it there: bass.c), C1 at the lowest (under it, nothing a
 * speaker plays) */
static int bass_sounding(int note)
{
    note += 12 * ((int)st.v[P_BS_OCTAVE] - 2);
    while (note < 24)
        note += 12;
    return note > 127 ? 127 : note;
}

/* BASS II's PULSE (pulse_run): the grid at its last run; where on it the bass's note was played last, and whether that
 * was a moment ago (three clocks at most: fresh); the notes played again, and the division the last fell on (the
 * tests) */
static struct {
    float g, ng, at;
    uint8_t fresh;
    uint32_t n;
#ifdef PLAT_HOST
    uint32_t seen;                       /* (the tests: the engine's strikes seen, and how late the last fell: ms after
                                          * its division) */
    int32_t late;
#endif
} bp;

static void bass_play(int note, uint32_t vel)   /* note: as played (the engine puts OCTAVE on it) */
{
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (note != bass_note && bass_note >= 0)
        frasil_bass_pressure(0.0f);      /* another note: its own key's pressure, from nothing */
    bass_note = (int8_t)note;
    frasil_bass_on(bass_note, (float)vel * (1.0f / 127.0f));
    bp.ng = arp.tick;
    bp.fresh = 1;
}

/* a note played by hand (a key, a MIDI note): with PULSE on, a note sounding (or in its gap) goes to it, not struck:
 * PULSE strikes it on its next division, the beat kept (the same note: nothing); else bass_play's */
static void bass_live(int note, uint32_t vel)
{
    if (!(st.v[P_BS_PULSE] % NPULSE) || bass_note < 0) {
        bass_play(note, vel);
        return;
    }
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (note == bass_note)
        return;
    frasil_bass_pressure(0.0f);          /* (another note: its own key's pressure, from nothing) */
    bass_note = (int8_t)note;
    frasil_bass_retune(bass_note, (float)vel * (1.0f / 127.0f));
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
        if (key[k].kind == K_BASS || key[k].kind == K_BSEQ)
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
    memcpy(saved_bnote, bnote, sizeof bnote);
    memcpy(saved_blen, blen, sizeof blen);
    saved_bq[0] = st.v[P_BQ_ON];
    saved_bq[1] = st.v[P_BQ_STEPS];
    saved_bq[2] = st.v[P_BQ_TRANS];
    saved_bq[3] = st.v[P_TEMPO];         /* (the scene's TEMPO too: scene_t's, after bq_trans) */
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
    return memcmp(bnote, saved_bnote, sizeof bnote) || memcmp(blen, saved_blen, sizeof blen) ||
           saved_bq[0] != st.v[P_BQ_ON] || saved_bq[1] != st.v[P_BQ_STEPS] || saved_bq[2] != st.v[P_BQ_TRANS] ||
           saved_bq[3] != st.v[P_TEMPO];
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

static void seq_begin(void)              /* from its first step, at the grid's next (its steps the grid's again) */
{
    seq_note_off();
    sq.run = 1;
    sq.off = 0;
    sq.at = -1.0f;
    sq.n = arp.step_n;
    sq.start = arp.step_n + 1;
    sq.pos = sq.skip = -1;
}

static void rec_keys(int p);
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
        rec_keys(P_SQ_REC);
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
            app_say("UNDO TAKE", "NOTHING YET");
        }
    } else if (p == P_SQ_TYPE) {
        if (x == (int32_t)SQT_SEQ)
            seq_begin();
        else {
            seq_note_off();
            sq.run = 0;
            if (st.v[P_SQ_REC]) {        /* (no sequencer, nothing to record into) */
                st.v[P_SQ_REC] = 0;
                rec_keys(P_SQ_REC);
            }
        }
        arp.paused = 0;
        if (page_hidden(st.page))        /* (its pages gone) */
            st.page = PG_GLINT;
    }
    st.dirty = (uint8_t)sound_changed();
}

/* --------------------------------------------------- the bass's sequence --- */
/* NOTE and STEP LENGTH show the step on STEP. Recording on STEP, STEP LENGTH shows the length carried on instead
 * (bq.len), so that it always says what the next key will write: turned, it sets that, and the step on STEP's own at
 * once (bseq_set); NOTE turned changes the note alone. The step's own length the strip shows, each step as wide as it
 * is long */
static void bseq_view(void)
{
    uint32_t c = st.v[P_BQ_STEP] % SC_BSEQ;
    st.v[P_BQ_NOTE] = bnote[c] & 127u;
    st.v[P_BQ_LEN] = (uint8_t)((st.v[P_BQ_REC] == 1u ? bq.len : blen[c]) % NBQLEN);
}

static uint32_t bseq_ticks(int32_t i) { return BQ_TICKS[blen[i] % NBQLEN]; }   /* step i's length, in MIDI clocks */

/* a step's note as it plays: TRANSPOSE's steps along the scale (OCTAVE on it in the engine, as on every note) */
static int bseq_note(uint32_t b)
{
    return scale_shift((int)(b & 127u), (int)st.v[P_BQ_TRANS] - 14, st.v);
}

/* the newest BASS key down into the sequence (else MIDI's last note), as the sequence keeps it; -1: none */
static int bseq_held(void)
{
    int32_t k, last = -1;
    for (k = 0; k < (int32_t)NKEYS; k++)
        if (key[k].kind == K_BSEQ && (last < 0 || key[k].t - key[last].t < 0x80000000u))
            last = k;
    if (last >= 0)
        return key[last].gbase;
    return mb_n ? mbass[mb_n - 1] : -1;
}

/* the playhead in the loop, in MIDI clocks from its first step; put back at clock c once the steps are written
 * again (the same time, whatever step it falls in now) */
static float bseq_at(void)
{
    float t = bq.t;
    int32_t i;
    for (i = 0; i < bq.pos; i++)
        t += (float)bseq_ticks(i);
    return t;
}

static void bseq_seek(float c)
{
    int32_t i;
    for (i = 0; i < (int32_t)st.v[P_BQ_STEPS] && c >= (float)bseq_ticks(i); i++)
        c -= (float)bseq_ticks(i);
    bq.pos = (int8_t)i;
    bq.t = c;
}

static int bseq_playing(void) { return bq.run && !bq.wait && bq.pos >= 0; }   /* (begun: a playhead) */

/* ------------------------------------------------------- a LIVE take --- */
/* A take keeps each note as it was played, its start and its length to the sixteenth (at least one), laid over the
 * loop once it ends (let go, or another key taking over): the loop laid out a sixteenth a cell (bseq_lay), the note
 * over what was there (bseq_lay_note), then written back into steps (bseq_fold), each stretch in as few as STEP
 * LENGTH's lengths make it, the longest first: a 5/16, a NOTE of 1/4 and a HOLD of 1/16 */

/* the loop (its SEQ STEPS steps) laid out in bq_tl: a NOTE where it starts, SQ_TIE on through it and its HOLDs, SQ_REST
 * where nothing sounds (a CUT, a HOLD after it; at its start, after its last steps). Its length in cells */
static uint32_t bseq_lay(void)
{
    uint32_t i, k, c, b, n = 0;
    int quiet = 1;
    for (i = 0; i <= st.v[P_BQ_STEPS]; i++)   /* (what its end leaves sounding, round to its first steps) */
        if ((b = bnote[i] & 127u) != SQ_TIE)
            quiet = b == SQ_REST;
    for (i = 0; i <= st.v[P_BQ_STEPS]; i++) {
        if ((b = bnote[i] & 127u) != SQ_TIE)
            quiet = b == SQ_REST;
        for (c = bseq_ticks((int32_t)i) / 6u, k = 0; k < c; k++)
            bq_tl[n++] = (uint8_t)(quiet ? SQ_REST : k || b == SQ_TIE ? SQ_TIE : b);
    }
    return n;
}

/* bq_tl's n cells back into steps (write: else only counted): each stretch, a note on through its TIEs or a silence,
 * in as few steps as STEP LENGTH's lengths make it, the longest first (the fewest so, whatever its length): its NOTE
 * then HOLDs, or CUTs; the steps past them as CLEAR leaves them, SEQ STEPS theirs. How many, 0: more than the SC_BSEQ
 * there are (nothing written) */
static uint32_t bseq_fold(uint32_t n, int write)
{
    uint32_t i, e, c, k, m = 0;
    for (i = 0; i < n; i = e) {
        uint8_t h = bq_tl[i], f = h == SQ_REST ? SQ_REST : SQ_TIE;
        for (e = i + 1u; e < n && bq_tl[e] == f; e++)
            ;
        for (c = e - i; c; c -= BQ_TICKS[k] / 6u, m++) {
            for (k = NBQLEN - 1u; BQ_TICKS[k] / 6u > c; k--)
                ;
            if (m == SC_BSEQ)
                return 0;
            if (write) {
                bnote[m] = c == e - i ? h : f;
                blen[m] = (uint8_t)k;
            }
        }
    }
    if (write) {
        memset(bnote + m, SQ_REST, SC_BSEQ - m);
        memset(blen + m, 0, SC_BSEQ - m);
        st.v[P_BQ_STEPS] = (uint8_t)(m - 1u);
        bseq_view();
        st.dirty = (uint8_t)sound_changed();
    }
    return m;
}

/* a take's note over the loop's n cells: its NOTE at cell s, on through d of them (round the loop's end). What it
 * cuts short does not go on after it from its middle: CUT till the next NOTE or CUT there was */
static void bseq_lay_note(uint32_t n, uint32_t s, uint32_t d, uint8_t note)
{
    uint32_t i, j;
    for (i = 0; i < d; i++)
        bq_tl[(s + i) % n] = i ? SQ_TIE : note;
    for (j = (s + d) % n; d < n && bq_tl[j] == SQ_TIE; j = (j + 1u) % n, d++)
        bq_tl[j] = SQ_REST;
}

/* legato, to fit: the silence from cell j on held by the note before it, as long as it lasts (round the loop's end,
 * if round); and where the silence just before cell s begins */
static void bseq_join(uint32_t n, uint32_t j, int round)
{
    uint32_t k;
    if ((!j && !round) || bq_tl[j ? j - 1u : n - 1u] == SQ_REST)
        return;                          /* (nothing before it to hold on) */
    for (k = 0; k < n && bq_tl[j] == SQ_REST; k++) {
        bq_tl[j] = SQ_TIE;
        if (++j == n && !round)
            break;
        j %= n;
    }
}

static uint32_t bseq_gap(uint32_t n, uint32_t s, int round)
{
    uint32_t k;
    for (k = 0; k < n && (s || round) && bq_tl[s ? s - 1u : n - 1u] == SQ_REST; k++)
        s = s ? s - 1u : n - 1u;
    return s;
}

/* a take's note into the loop: from cell s, d long (round the loop's end; an open take's loop grown to the bar it
 * ends in). More steps than there are: legato, to fit, the silence after it held on by it, else the one before it by
 * the note before, else both (open, the one before only: what comes after it is not played yet): BASS SEQ FULL,
 * JOINED LEGATO; still too many, it is not kept, the earliest stay (BASS SEQ FULL, NOT RECORDED). The playhead on
 * where it was in time; ahead (let go before its start came): its step sounded already (bq.skip) */
static void bseq_take(uint32_t s, uint32_t d, uint8_t note, int ahead)
{
    float at = bseq_playing() ? bseq_at() : -1.0f;
    uint32_t n = bseq_lay(), e = n, m = 0, k, c;
    int32_t i;
    if (bq.open) {
        d = s + d <= BQ_CELLS ? d : s < BQ_CELLS ? BQ_CELLS - s : 0u;
        e = (s + d + 15u) / 16u * 16u;
        e = e > n ? e : n;
    } else {
        s %= n;
        d = d < n ? d : n;
    }
    for (k = 0; d && !m && k < (bq.open ? 2u : 4u); k++) {
        if (k)
            bseq_lay();
        memset(bq_tl + n, SQ_REST, e - n);
        bseq_lay_note(e, s, d, note);
        if (!bq.open && (k & 1u))
            bseq_join(e, (s + d) % e, 1);
        if (bq.open ? k : k >> 1)
            bseq_join(e, bseq_gap(e, s, !bq.open), !bq.open);
        m = bseq_fold(e, 0);
    }
    if (!m) {
        app_say("BASS SEQ FULL", "NOT RECORDED");
        return;
    }
    if (k > 1u)
        app_say("BASS SEQ FULL", "JOINED LEGATO");
    bseq_fold(e, 1);
    if (at >= 0.0f)
        bseq_seek(at);
    bq.skip = -1;
    for (i = 0, c = 0; ahead && c < s && i < (int32_t)SC_BSEQ; i++)
        c += bseq_ticks(i) / 6u;
    if (ahead && c == s)
        bq.skip = (int8_t)i;
}

/* the step the playhead began a moment ago (at most `late` clocks, 3 at most), silent under a key: its NOTE now */
static void bseq_catch(float late)
{
    uint32_t b;
    if (!bseq_playing() || bq.t > late || bq.t >= 3.0f)
        return;
    b = bnote[bq.pos] & 127u;
    if (b != SQ_REST && b != SQ_TIE) {
        bass_play(bseq_note(b), 100);
        bq.steps++;
    }
}

/* the take's note ends (let go; legato: another taking over, this one to where that one starts, to the sixteenth),
 * into the loop. Let go after its end (by less than half a sixteenth), the step begun there since sounds now */
static void bseq_end(int legato)
{
    float e = bq.tp + bq.te, late;
    int32_t s = (int32_t)fm_floorf(bq.tp / 6.0f + 0.5f);
    int32_t d = (int32_t)fm_floorf((legato ? e : bq.te) / 6.0f + 0.5f) - (legato ? s : 0);
    uint8_t note = (uint8_t)bq.tn;
    bq.tn = -1;
    if (legato && d < 1)                 /* (the next one starts where it did: it replaces it) */
        return;
    d = d < 1 ? 1 : d;
    bseq_take((uint32_t)(s < 0 ? 0 : s), (uint32_t)d, note, e <= 6.0f * (float)s);
    late = e - 6.0f * (float)(s + d);
    if (!legato && late >= 0.0f)
        bseq_catch(late);
}

/* the take ends: RECORD off LIVE, the sequence stopped or begun again. Its note down ends now; an open take closes:
 * its loop to the bar line nearest the playhead, past its last note's start (a note still down on to that line), the
 * bar begun past it gone, the playhead on in it as far into its turn */
static void bseq_close(void)
{
    if (bseq_playing() && bq.open) {
        float at = bseq_at(), l;
        uint32_t n = bseq_lay(), e = (uint32_t)(at / 96.0f + 0.5f) * 16u, i;
        uint32_t s = (uint32_t)fm_floorf(bq.tp / 6.0f + 0.5f);   /* (a note down: its start) */
        for (i = n; i && (bq_tl[i - 1u] == SQ_REST || bq_tl[i - 1u] == SQ_TIE); i--)
            ;
        if (bq.tn >= 0 && i < s + 1u)
            i = s + 1u;
        if (e < i)
            e = (i + 15u) / 16u * 16u;
        e = e < 16u ? 16u : e;
        if (bq.tn >= 0 && s < e) {
            uint8_t note = (uint8_t)bq.tn;
            bq.tn = -1;
            bseq_take(s, e - s, note, 0);
            n = bseq_lay();
        }
        if (e < n && !bseq_fold(e, 0))   /* (the bar begun past it: it stays if the loop does not fit without it) */
            e = n;
        else if (e < n)
            bseq_fold(e, 1);
        l = 6.0f * (float)e;
        bq.open = 0;
        bseq_seek(at - l * fm_floorf(at / l));
        if (at >= l) {                   /* (round to its turn's start: its NOTE begun a moment ago, now) */
            bq.moved = 1;
            bseq_catch(3.0f);
        }
    } else if (bseq_playing() && bq.tn >= 0) {
        bseq_end(0);
    }
    bq.tn = -1;
    bq.open = bq.wrap = 0;
}

/* LIVE, a note on an empty loop (no NOTE in what it plays): a take of its own (bq.open), from the bar the note falls
 * in, to the sixteenth (from the loop's first step, a bar at a time, or its next turn's): the loop a bar of CUT from
 * that bar's line, the playhead where it was in it, a bar longer each time it ends (bseq_run) till the take closes.
 * Where the note went down in it (just before the line: less than 0, the loop round once more first, bq.wrap) */
static float bseq_open(float at)
{
    float l = 0.0f, q = 6.0f * fm_floorf(at / 6.0f + 0.5f), o;
    uint32_t i;
    for (i = 0; i <= st.v[P_BQ_STEPS]; i++)
        l += (float)bseq_ticks((int32_t)i);
    o = q >= l ? l : 96.0f * fm_floorf(q / 96.0f);
    memset(bnote, SQ_REST, sizeof bnote);
    memset(blen, 0, sizeof blen);
    blen[0] = BQ_BAR;
    st.v[P_BQ_STEPS] = 0;
    bq.open = 1;
    bq.wrap = at < o;
    if (bseq_playing()) {
        bq.pos = 0;
        bq.t = at - o + (bq.wrap ? 96.0f : 0.0f);
        bq.moved = 1;
    }
    bseq_view();
    st.dirty = (uint8_t)sound_changed();
    return at - o;
}

static void bseq_begin(void)             /* from its first step, at the grid's next (a take under way closed) */
{
    bseq_close();
    bq.run = bq.wait = 1;
    bq.pos = bq.skip = -1;
}

static void bseq_stop(void)              /* its note let go (a take under way closed; an empty one's: the keys') */
{
    bseq_close();
    if (bq.run && bseq_has_notes())
        bass_stop();
    bq.run = 0;
    bq.pos = -1;
}

/* step i begins: its NOTE plays (GLIDE from the last), its CUT stops the bass, a HOLD changes nothing. LIVE, none of
 * it while the take's note is down (it has the bass), nor on the step a note just let go has written ahead */
#ifdef PLAT_HOST
static float bq_beat;
#endif
static void bseq_step(int32_t i)
{
    uint32_t b = bnote[i] & 127u;
    int quiet = bq.tn >= 0 || i == bq.skip;
    bq.pos = (int8_t)i;
    bq.skip = -1;
    if (quiet)
        return;
    if (b == SQ_REST) {
        if (bseq_has_notes())            /* (an empty one cuts nothing: the bass is the keys') */
            bass_stop();
    } else if (b != SQ_TIE) {
        bass_play(bseq_note(b), 100);
        bq.steps++;
#ifdef PLAT_HOST
        bq_beat = arp.tick;              /* (the tests: where on the grid the NOTE began) */
#endif
    }
}

/* an open take come to its loop's end: a bar of CUT more (0: no room, the take closes there) */
static int bseq_grow(void)
{
    uint32_t n = bseq_lay();
    if (n + 16u > BQ_CELLS)
        return 0;
    memset(bq_tl + n, SQ_REST, 16u);
    if (!bseq_fold(n + 16u, 0))
        return 0;
    bseq_fold(n + 16u, 1);
    return 1;
}

/* on the grid, after the arpeggiator has moved it (arp.tick): the steps that began since the last run; at the loop's
 * end round, an open take's a bar longer (no room: BASS SEQ FULL, LOOP CLOSED, round from there) */
static void bseq_run(void)
{
    float g = arp.tick, dg = g - bq.g, b;
    if (dg < -4800.0f)                   /* (the grid wraps at 9600) */
        dg += 9600.0f;
    else if (dg < 0.0f)
        dg = 0.0f;
    bq.g = g;
    if (!bq.run)
        return;
    if (bq.wait == 1) {                  /* begun: on the grid's next step, GLINT's while its sequence runs (the two
                                          * from the top together), else the next sixteenth */
        float h = g;
        b = 6.0f;
        if (st.v[P_SQ_TYPE] == SQT_SEQ && sq.run) {
            b = (float)ARP_TICKS[st.v[P_ARP_RATE] % 10u];
            h = seq_grid(g);             /* (its steps where GLINT's sequence has them) */
        }
        if (fm_floorf(h / b) == fm_floorf((h - dg) / b))
            return;
        bq.t = h - b * fm_floorf(h / b);
    } else if (bq.wait == 2) {           /* MIDI Start: from the grid's top */
        bq.t = g;
    } else {
        bq.t += dg;
    }
    if (bq.tn >= 0)                      /* (the take's note down: its time, from the first step if it came first) */
        bq.te = bq.wait ? bq.t : bq.te + dg;
    if (bq.wait) {
        bq.wait = 0;
        bseq_step(0);
    }
    while (bq.t >= (float)bseq_ticks(bq.pos)) {
        float c = bseq_at();
        bq.t -= (float)bseq_ticks(bq.pos);
        if (bq.pos < (int32_t)st.v[P_BQ_STEPS]) {
            bseq_step(bq.pos + 1);
        } else if (bq.open && !bq.wrap && bseq_grow()) {
            bseq_seek(c);
        } else {
            if (bq.open && !bq.wrap) {
                bq.open = 0;
                app_say("BASS SEQ FULL", "LOOP CLOSED");
            }
            bq.wrap = 0;
            bseq_step(0);
        }
    }
}

/* BASS II's PULSE: while the bass's note sounds (a key held or latched, a MIDI note, a NOTE of its sequence: bass_note),
 * on each of PULSE's divisions of the grid (MIDI clock, else TEMPO: arp.tick, from its beats), the note played again
 * from its ATTACK right on the division (frasil_bass_pulse: the same note and velocity, its pressure on, no glide), let
 * go a moment before it: the gap, a quarter of the division, 8 ms at least, 60 ms at most (a short breath, whatever the
 * tempo: at 120 BPM 16 ms on an eighth of a beat, 31 ms on a quarter, 60 ms from half a beat on). So the strikes fall
 * on the beat, as the sequence's NOTEs do. A step of the bass's sequence beginning on the division is its own: a NOTE
 * strikes there itself, a CUT ends the note (none then; a HOLD, the note going on, has its strike). A note played less
 * than a clock before the gap begins, or in it (a key, a MIDI note), is its attack already: none then */
/* the step of the bass's sequence that begins on the grid at `at` (within half a clock; g: the grid now), or -1 */
static int32_t bseq_step_at(float g, float at)
{
    float d;
    if (!bq.run || bq.wait || bq.tn >= 0)
        return -1;
    d = g + (float)bseq_ticks(bq.pos) - bq.t - at;
    if (d < -0.5f || d > 0.5f)
        return -1;
    return bq.pos < (int32_t)st.v[P_BQ_STEPS] ? bq.pos + 1 : 0;
}

static void pulse_run(void)
{
    uint32_t r = st.v[P_BS_PULSE] % NPULSE;
    float g = arp.tick, was = bp.g, per = (float)PULSE_TICKS[r], at, ms_tick, gap, lead, left;
    int fresh = bp.fresh;
    bp.g = g;
    if (bp.fresh) {
        float d = g - bp.ng;
        bp.fresh = (d < 0.0f ? d + 9600.0f : d) <= 3.0f;   /* (the grid wraps at 9600) */
    }
    ms_tick = plat_ms() - arp.clk_at < 500u && arp.clk_ms > 1.0f ? arp.clk_ms : 2500.0f / (float)(st.v[P_TEMPO] + 30u);
#ifdef PLAT_HOST
    if (r) {
        bass_view_t bv;
        frasil_bass_view(&bv);
        if (bv.pulses != bp.seen) {      /* a strike: where it fell, from its division */
            bp.seen = bv.pulses;
            bp.late = (int32_t)fm_floorf((g - per * fm_floorf(g / per + 0.5f)) * ms_tick + 0.5f);
        }
    }
#endif
    if (!r || bass_note < 0)
        return;
    gap = 0.25f * per * ms_tick;
    gap = gap < 8.0f ? 8.0f : gap > 60.0f ? 60.0f : gap;
    lead = gap / ms_tick;                /* the gap in clocks: the grid that far ahead, the division about to come */
    if (g < was - 4800.0f)
        was -= 9600.0f;                  /* (wrapped: 9600 is every division's multiple) */
    if (fm_floorf((g + lead) / per) == fm_floorf((was + lead) / per))
        return;
    at = per * fm_floorf((g + lead) / per);
    {
        int32_t nx = bseq_step_at(g, at);
        uint32_t b = nx >= 0 ? bnote[nx] & 127u : SQ_TIE;
        if (b != SQ_TIE && (b != SQ_REST || bseq_has_notes()))
            return;
    }
    if (fresh) {
        float d = g - bp.ng;
        d = d > 4800.0f ? d - 9600.0f : d < -4800.0f ? d + 9600.0f : d;
        if (d <= 1.0f)
            return;
    }
    left = (at - g) * ms_tick;           /* (to the division: the strike there, whatever the clock's steps) */
    frasil_bass_pulse((left < 8.0f ? 8.0f : left > 60.0f ? 60.0f : left) * 0.001f);
    bp.n++;
    bp.at = at >= 9600.0f ? at - 9600.0f : at;
}

/* a BASS key or note while the sequence is on (n: as the sequence keeps it, without OCTAVE; sound: as it sounds).
 * RECORD OFF: the sequence transposed from its first NOTE to this one along the scale; STEP: written at STEP, the
 * length carried on with it (bq.len), then on (it sounds while down); LIVE: it sounds at once and, the sequence
 * running, goes into the take from where it went down (the one down before it ends there, legato; on an empty loop,
 * the take's own loop from its bar: bseq_open) */
static void bseq_key(int n, int sound, uint32_t vel)
{
    uint32_t len = st.v[P_BQ_STEPS] + 1u, i;
    uint8_t b = (uint8_t)(n < 1 ? 1 : n > 126 ? 126 : n);
    if (st.v[P_BQ_REC] == 1u) {
        uint32_t c = st.v[P_BQ_STEP] % len;
        bnote[c] = b;
        blen[c] = bq.len;
        st.v[P_BQ_STEP] = (uint8_t)((c + 1u) % len);
        bseq_view();
        bass_play(sound, vel);
    } else if (st.v[P_BQ_REC] == 2u) {
        bass_play(sound, vel);
        if (bq.run) {
            float p;
            if (bq.tn >= 0)
                bseq_end(1);
            p = bseq_playing() ? bseq_at() - (bq.wrap ? 96.0f : 0.0f) : 0.0f;   /* (the take's bar the next) */
            for (len = st.v[P_BQ_STEPS] + 1u, i = 0; i < len; i++)
                if ((bnote[i] & 127u) != SQ_REST && (bnote[i] & 127u) != SQ_TIE)
                    break;
            if (i == len && !bq.open)
                p = bseq_open(p);
            bq.tn = (int8_t)b;
            bq.tp = p;
            bq.te = 0.0f;
        }
    } else {
        for (i = 0; i < len; i++)        /* from the first NOTE (not a CUT, not a HOLD) */
            if ((bnote[i] & 127u) != SQ_REST && (bnote[i] & 127u) != SQ_TIE)
                break;
        if (i < len)
            st.v[P_BQ_TRANS] = (uint8_t)clamp(14 + scale_steps(bnote[i] & 127, n, st.v), 0, 28);
    }
    st.dirty = (uint8_t)sound_changed();
}

/* its key or note let go, the one sounding (RECORD OFF, the sequence running: the bass is its own): its sound stops;
 * LIVE, another still down takes over, legato, else the take's note ends there (bseq_end) */
static void bseq_up(int sound)
{
    int h;
    if (bass_note != sound || (!st.v[P_BQ_REC] && bq.run))
        return;
    if (st.v[P_BQ_REC] == 2u && (h = bseq_held()) >= 0) {
        bseq_key(h, h, 100);
        return;
    }
    bass_stop();
    if (bq.tn >= 0)
        bseq_end(0);
}

static void bseq_set(int p, int32_t x)
{
    uint32_t c = st.v[P_BQ_STEP] % SC_BSEQ;
    if (p == P_BQ_NOTE || p == P_BQ_LEN || p == P_BQ_STEPS)   /* (a take's loop as it is turned now: round) */
        bq.open = bq.wrap = 0;
    if (p == P_BQ_STEP) {
        bseq_view();
    } else if (p == P_BQ_NOTE) {
        bnote[c] = (uint8_t)x;
    } else if (p == P_BQ_LEN) {
        blen[c] = (uint8_t)x;
        bq.len = (uint8_t)x;             /* (STEP: carried on from here) */
    } else if (p == P_BQ_CLEAR && x >= 2) {   /* turned twice: every step a CUT, a sixteenth long (a take begins
                                              * again with the next note) */
        memset(bnote, 0, sizeof bnote);
        memset(blen, 0, sizeof blen);
        bq.tn = -1;
        bq.open = bq.wrap = 0;
        st.v[P_BQ_CLEAR] = 0;
        bseq_view();
        app_say("BASS SEQUENCE", "CLEARED");
    } else if (p == P_BQ_REC) {
        if (x && !st.v[P_BQ_ON]) {       /* (RECORD: into the sequence, on) */
            st.v[P_BQ_ON] = 1;
            bseq_begin();
        }
        if (x != 2)                      /* (off LIVE: the take ends) */
            bseq_close();
        if (x == 1)                      /* STEP: the length carried on, the step on STEP's to begin with */
            bq.len = (uint8_t)(blen[c] % NBQLEN);
        bseq_view();
    } else if (p == P_BQ_ON) {
        if (x)
            bseq_begin();
        else {
            bseq_stop();
            if (st.v[P_BQ_REC]) {        /* (nothing to record into) */
                st.v[P_BQ_REC] = 0;
                bseq_view();
                rec_keys(P_BQ_REC);
            }
        }
    }
    st.dirty = (uint8_t)sound_changed();
}

/* a scene's MORPH under way: what each setting still lacks of the scene's at the start (morph_off goes from it to 0) */
static struct {
    uint8_t on;
    uint32_t t0, dur;
    float d[NSOUND];
    char name[SC_NAME + 1];
} mo;

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
    if (p == P_SPEAKER)                  /* at once (kept once nothing sounds, as the metronome is; ANIMATIONS too) */
        plat_speaker(x && !spk_page);
    if (p == P_SPEAKER)                  /* (SPK/JACK kept now, whatever plays: the room's quiet comes first) */
        st.prefs_due = 2;
    if ((p == P_ANIM || p == P_CLOCK) && !st.prefs_due)
        st.prefs_due = 1;
    if (p == P_CLOCK && x)               /* INTERNAL: TEMPO's grid at once, a clock coming in let go */
        arp.clk_at = plat_ms() - 60000u;
    if (p == P_SQ_TYPE || (p >= P_SQ_STEP && p <= P_SQ_CLEAR) || p == P_SQ_NEW || p == P_SQ_UNDO)
        seq_set(p, x);
    if (p >= P_BQ_STEP && p <= P_BQ_TRANS)
        bseq_set(p, x);
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
    if (p == P_SQ_REC || p == P_BQ_REC)
        rec_keys(p);
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
    if (p == P_EQ_PRESET && x >= 2) {    /* MASTER EQ's preset: its bands (OFF and CUSTOM leave them as they are) */
        uint32_t i;
        for (i = 0; i < 7u; i++) {
            st.v[P_EQ_LOW + i] = EQ_PRESETS[x - 2][i];
            morph_off[P_EQ_LOW + i] = mo.d[P_EQ_LOW + i] = 0.0f;
        }
    } else if (p >= P_EQ_LOW && p <= P_EQ_HIGHF && st.v[P_EQ_PRESET] >= 2u) {
        st.v[P_EQ_PRESET] = 1;           /* a band turned: CUSTOM, the bands as they are */
    }
    if (p < NSOUND) {
        st.dirty = (uint8_t)sound_changed();
        st.engine_dirty = 1;
    } else if (p == P_TEMPO) {           /* (the scene's TEMPO too) */
        st.dirty = (uint8_t)sound_changed();
    }
}

/* RECORD p turned (GLINT's or the bass's; by REC, K4, NEW TAKE, or gone with its sequencer), STEP and LIVE alike:
 * a recording begun has the keys at once, on its track (KEYS as they were kept, st.kb_rec); the other still
 * recording, they go to it; neither, back to what they played */
static void rec_keys(int p)
{
    int g = st.v[P_SQ_REC] != 0, b = st.v[P_BQ_REC] != 0;
    if (!g && !b) {
        if (st.rec_took)
            param_set(P_KB, st.kb_rec);
        st.rec_took = 0;
        return;
    }
    if (!st.rec_took) {
        st.kb_rec = st.v[P_KB];
        st.rec_took = 1;
    }
    param_set(P_KB, st.v[p] ? (p == P_BQ_REC ? 2 : 1) : g ? 1 : 2);
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    frasil_render(lr, n, (float)master_q12 * (1.0f / 4096.0f));
}

void app_panic(void) { frasil_panic(); }

/* the render ran over 90 % of its time (audio interrupt), or the load is over 82 % (load_watch, ahead of it): one step
 * lighter at once, and the header's dot for 3 s. The
 * main loop gives a step back after a calm (no overrun, the load at most 75 %): 4 s at first, twice as long each time
 * a step given back ran over again within 15 s (up to 64 s), 4 s again after a minute without one. (2.5-b8, the user:
 * a heavy scene glitched every few seconds, a step given back and taken again, the dot coming and going) */
static volatile uint32_t over_at;        /* when it last ran over (plat_ms), 0: never */
static volatile int shed_top;            /* the highest step since the last log (app_log_word) */
static void shed_to(int level)
{
    frasil_shed(level);
    if (frasil_shed_level() > shed_top)
        shed_top = frasil_shed_level();
}
void app_overload(void)
{
    over_at = plat_ms() | 1u;
    shed_to(frasil_shed_level() + 1);
}
static int holding_any(void);
/* the log's word: the step now (bits 0-7), the highest since the last log (8-15), anything playing (16: held, latched,
 * a sequence running: a test checks the FM-1 is at rest), and a part of the render (17-20, frasil.h FR_PROF_..., one
 * more at each log) with its share of the render's time since that part's last log, per mille (21-31) */
uint32_t app_log_word(void)
{
    static uint32_t last[FR_PROF_N], last_n[FR_PROF_N];
    static int part;
    uint32_t acc[FR_PROF_N], n, w, pm = 0, dn;
    frasil_profile_read(acc, &n);
    dn = n - last_n[part];
    if (dn)
        pm = (uint32_t)((float)(acc[part] - last[part]) / ((float)dn * ((float)PLAT_TICKS_US * 1e6f / 44100.0f)) *
                        1000.0f + 0.5f);
    last[part] = acc[part];
    last_n[part] = n;
    w = (uint32_t)(pm > 2047u ? 2047u : pm) << 21 | (uint32_t)part << 17 | (uint32_t)(holding_any() != 0) << 16 |
        (uint32_t)shed_top << 8 | (uint32_t)frasil_shed_level();
    part = part + 1 < FR_PROF_N ? part + 1 : 0;
    shed_top = frasil_shed_level();
    return w;
}
static int cpu_over(void) { return over_at && plat_ms() - over_at < 3000u; }

static void load_watch(void)
{
    static uint32_t calm_since, seen, back_at, calm_ms = 4000u, ahead_at;
    uint32_t now = plat_ms(), at = over_at;
    if (plat_cpu_pct() > 82u && frasil_shed_level() < 5 && now - ahead_at > 1000u) {   /* the load near the top: a
                                                                                       * step lighter before a render
                                                                                       * runs over (the steps glide:
                                                                                       * not heard), one a second */
        shed_to(frasil_shed_level() + 1);
        over_at = at = plat_ms() | 1u;   /* (as an overrun: no step back for the calm, the backoff the same) */
        ahead_at = now;
    }
    if (at != seen) {                    /* a new overrun: soon after a step given back, the next calm twice as long */
        seen = at;
        if (back_at && at - back_at < 15000u)
            calm_ms = calm_ms >= 32000u ? 64000u : calm_ms * 2u;
        back_at = 0;
    } else if (calm_ms > 4000u && now - at > 60000u) {
        calm_ms = 4000u;
    }
    if (!frasil_shed_level() || plat_cpu_pct() > 75u || now - at < 4000u) {
        calm_since = now;
        return;
    }
    if (now - calm_since > calm_ms) {
        frasil_shed(frasil_shed_level() - 1);
        calm_since = back_at = now;
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
    sc->bass = st.v[P_KEYS] || bseq_keys() || bass_note < 0 ? -1 : (int8_t)bass_sounding(bass_note);   /* as it
                                                                  * sounds; held notes are not kept, as the drones'
                                                                  * are not (nor the bass's sequence's) */
    for (i = 0; i < SC_SEQ; i++)
        sc->seq[i] = seq[i];
    memcpy(sc->bnote, bnote, sizeof bnote);
    memcpy(sc->blen, blen, sizeof blen);
    sc->bq_on = st.v[P_BQ_ON];
    sc->bq_steps = st.v[P_BQ_STEPS];
    sc->bq_trans = st.v[P_BQ_TRANS];
    sc->tempo = st.v[P_TEMPO];
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
    if (sc->bass >= 0)                   /* (kept as it sounds: played without its OCTAVE, the engine's) */
        bass_play(sc->bass - 12 * ((int)st.v[P_BS_OCTAVE] - 2), 100);
}

static void new_shard(scene_t *out);     /* (below) */

/* slot s with nothing saved in it: a factory scene (params.c FACTORY: made as NEW SHARD is, then its settings, or its
 * sound whole as an FM-1 stored it; its TEMPO and what it plays), or past them NEW SHARD, named by its number */
static void factory_get(uint32_t s, scene_t *out)
{
    const factory_t *f = &FACTORY[s < NFACTORY ? s : 0];
    uint32_t i, j, pass;
    if (s >= NFACTORY) {
        new_shard(out);
        memset(out->name, 0, SC_NAME);
        text_cat(out->name, "SCENE ");
        int_text((int32_t)s + 1, out->name + 6);
        return;
    }
    memset(out, 0, sizeof *out);
    memcpy(out->name, f->name, SC_NAME);
    if (f->whole) {
        memcpy(out->v, f->sound, FACTORY_NV);
        later_defaults(out->v, FACTORY_NV);
    } else {
        for (i = 0; i < NSOUND; i++)
            out->v[i] = PD[i].def;
        for (pass = 0; pass < 2u; pass++) {  /* its settings, over its GLINT preset's and the arpeggiator's defaults */
            for (i = 0; i < sizeof f->p / sizeof f->p[0] && f->p[i][0] != P_END; i++)
                out->v[f->p[i][0]] = f->p[i][1];
            if (!pass) {
                glint_preset(out->v, out->v[P_GL_PRESET]);
                arp_defaults(out->v);
            }
        }
    }
    for (i = 0; i < f->nch; i++)         /* its chords as latched, */
        memcpy(out->ch[out->nch++], f->ch[i], 5);
    for (i = 0; i < sizeof f->keys && f->keys[i]; i++) {   /* or the drones it latches: each key's chord */
        chord_t c;
        int root;
        chord_build(f->keys[i], out->v, &c, &root);
        out->ch[out->nch][0] = (int8_t)root;
        for (j = 0; j < 4u; j++)
            out->ch[out->nch][1 + j] = (int8_t)(j < c.k ? c.n[j] : -1);
        out->nch++;
    }
    for (i = 0; i < sizeof f->arp && f->arp[i]; i++)
        out->arp[out->narp++] = f->arp[i];
    out->bass = f->bass;
    memcpy(out->seq, f->seq, SC_SEQ);
    memcpy(out->bnote, f->bnote, SC_BSEQ);
    memcpy(out->blen, f->blen, SC_BSEQ);
    out->bq_on = f->bq_on;
    out->bq_steps = f->bq_steps;
    out->bq_trans = f->bq_trans;
    out->tempo = f->tempo;
}

/* a scene's name as text, t SC_NAME + 1 long (all sixteen used: no NUL in the scene) */
static char *name_text(const scene_t *sc, char *t)
{
    uint32_t i;
    for (i = 0; i < SC_NAME && sc->name[i]; i++)
        t[i] = sc->name[i];
    t[i] = 0;
    return t;
}

/* a scene's bass sequence empty: every step a CUT of a sixteenth, SEQUENCE off, all sixteen steps, not transposed */
static void bseq_empty(scene_t *sc)
{
    memset(sc->bnote, 0, 2u * SC_BSEQ);
    sc->bq_on = 0;
    sc->bq_steps = PD[P_BQ_STEPS].def;
    sc->bq_trans = PD[P_BQ_TRANS].def;
}

static void new_shard(scene_t *out)      /* NEW SHARD (params.c): made as a factory scene is, then its own settings */
{
    uint32_t i;
    *out = NEW_SHARD;
    glint_preset(out->v, out->v[P_GL_PRESET]);
    arp_defaults(out->v);
    later_defaults(out->v, P_BS_DRIVE);
    for (i = 0; i < sizeof NEW_SHARD_P / sizeof NEW_SHARD_P[0]; i++)
        out->v[NEW_SHARD_P[i][0]] = NEW_SHARD_P[i][1];
}

static void scene_get(uint32_t s, scene_t *out)   /* slot s, or NEW SHARD (SC_NEW) */
{
    if (s >= NSCENES)
        new_shard(out);
    else if (scn.used >> s & 1u)
        *out = scn.s[s];
    else
        factory_get(s, out);
}

static int grid_top(int start);          /* (below) */
static void seqs_top(int glint, int bass);

/* a scene's sound and what it plays, taken as a load takes them (from a slot: scene_load; a shard the River plays:
 * scene_audition) */
static void scene_take(const scene_t *sc)
{
    uint32_t i;
    for (i = 0; i < NSOUND; i++)
        st.v[i] = sc->v[i] > PD[i].max ? PD[i].max : sc->v[i];
    drone_take();
    for (i = 0; i < SC_SEQ; i++)
        seq[i] = sc->seq[i];
    seq_view();
    if (st.v[P_SQ_TYPE] != SQT_SEQ) {    /* (its sequence: from the top, below) */
        seq_note_off();
        sq.run = 0;
        if (st.v[P_SQ_REC]) {
            st.v[P_SQ_REC] = 0;
            rec_keys(P_SQ_REC);
        }
    }
    bq.tn = -1;                          /* (a take under way: gone with the loop it was in) */
    bq.open = bq.wrap = 0;
    memcpy(bnote, sc->bnote, sizeof bnote);   /* and the bass's, with its own SEQUENCE, SEQ STEPS, TRANSPOSE */
    memcpy(blen, sc->blen, sizeof blen);
    st.v[P_BQ_ON] = sc->bq_on ? 1 : 0;
    st.v[P_BQ_STEPS] = sc->bq_steps > PD[P_BQ_STEPS].max ? PD[P_BQ_STEPS].def : sc->bq_steps;
    st.v[P_BQ_TRANS] = sc->bq_trans > PD[P_BQ_TRANS].max ? PD[P_BQ_TRANS].def : sc->bq_trans;
    if (sc->tempo <= PD[P_TEMPO].max)    /* its TEMPO (a MIDI clock coming in plays on: the clock's) */
        st.v[P_TEMPO] = sc->tempo;
    bseq_view();
    if (!st.v[P_BQ_ON])                  /* (its sequence: from the top, below) */
        bseq_stop();
    arp.paused = 0;
    if (page_hidden(st.page))
        st.page = PG_GLINT;
    sound_mark();
    if (st.loading_morph)                /* (scene_go sets the glide first: no step to the new sound) */
        st.engine_dirty = 1;
    else
        engine_push();
    release_all();                       /* a scene starts clean: every note let go, the arpeggio stopped */
    arp.n = arp.pn = 0;
    arp.dirty = 0;
    arp_stop();
    {                                    /* then what it plays, if anything, and its sequences from the top: the beat
                                          * begins there, as PLAY starts it (following a MIDI clock, on its next
                                          * step: its beats kept) */
        int gs = st.v[P_SQ_TYPE] == SQT_SEQ, bs = st.v[P_BQ_ON], top = grid_top(0);
        if (sc->nch || sc->narp || sc->bass >= 0)
            scene_notes_play(sc);
        if (top) {
            seqs_top(gs, bs);
        } else {
            if (gs)
                seq_begin();
            if (bs)
                bseq_begin();
        }
    }
}

static void scene_load(uint32_t s)
{
    scene_t sc;
    scene_get(s, &sc);
    st.scene = (uint8_t)s;
    scene_take(&sc);
}

#ifdef PLAT_WEB
/* the River plays a shard (web.c): its scene as a load takes it, out of any MORPH; with `hold` (▶), one that plays
 * nothing holds a chord on its ROOT, in the third octave, as a key would (aud_root: its root, -1 none; what the shard
 * plays edited lets it go) */
static int aud_root = -1;
static void scene_audition(const scene_t *sc, int hold)
{
    uint32_t i, notes = 0;
    mo.on = 0;
    memset(morph_off, 0, sizeof morph_off);
    aud_root = -1;
    scene_take(sc);
    if (!hold)
        return;
    for (i = 0; i < SC_SEQ; i++)
        notes += (sc->seq[i] & 127u) != SQ_REST && (sc->seq[i] & 127u) != SQ_TIE;
    if (!sc->nch && !sc->narp && sc->bass < 0 && !(st.v[P_SQ_TYPE] == SQT_SEQ && notes)) {
        chord_t c;
        int root;
        chord_build(48 + (int)(st.v[P_ROOT] % 12u), st.v, &c, &root);
        lat[root] = c;
        bit_put(lit, root, 1);
        chord_on(&c, 100);
        aud_root = root;
    }
}

static void audition_stop(void)          /* every note let go, as PLAY held */
{
    release_all();
    arp.n = arp.pn = 0;
    arp.dirty = arp.paused = 0;
    arp_stop();
    seq_note_off();
    sq.run = 0;
    bseq_stop();
}
#endif

/* a backup streaming (app_backup_hold): no STORE till it ends (a pop-up says so) */
static int store_held(void)
{
    if (st.store_hold)
        popup(POP_WARN, "SCENES", "NOT STORED", "A BACKUP IS GOING TO USB.", "STORE AGAIN IN A MOMENT.", "");
    return st.store_hold;
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
    set.speaker_off = !st.v[P_SPEAKER];
    set.anim = st.v[P_ANIM];
    set.clock = st.v[P_CLOCK];
    set.load = st.v[P_LOAD];
    memcpy(set.bnote, bnote, sizeof bnote);
    memcpy(set.blen, blen, sizeof blen);
    set.bq_on = st.v[P_BQ_ON];
    set.bq_steps = st.v[P_BQ_STEPS];
    set.bq_trans = st.v[P_BQ_TRANS];
    return plat_store_save(OBJ_SETTINGS, &set, sizeof set);
}

/* the scenes into the store: their bass sequences first (OBJ_BSEQ), then the rest of them (OBJ_SCENES), each record
 * made in raw. 0: both taken */
static int scenes_save(void)
{
    uint32_t s;
    for (s = 0; s < 4u; s++)
        raw[s] = (uint8_t)(BSQ_MAGIC >> (8u * s));
    for (s = 0; s < NSCENES; s++)
        memcpy(raw + 4u + s * BSQ_SCENE, scn.s[s].seq, BSQ_SCENE);
    if (plat_store_save(OBJ_BSEQ, raw, BSQ_BYTES))
        return -1;
    memcpy(raw, &scn, 8u);               /* (magic, used, rsv) */
    for (s = 0; s < NSCENES; s++)
        memcpy(raw + 8u + s * SC_REC, &scn.s[s], SC_REC);
    return plat_store_save(OBJ_SCENES, raw, SCN_BYTES);
}

static scene_t rst;                      /* what PLAY held let go, for RESTORE (its notes alone) */
static uint8_t rst_valid;

/* LOAD, the metronome, the CCs learned, SPK/JACK, ANIMATIONS: kept (the sound and the rest of the record as SAVE left
 * them; never saved yet: all of it); a backup streaming, once it ends (set holds them meanwhile). Asked through
 * prefs_due, done once nothing sounds (prefs_quiet): writing the flash stops the render a moment, a click in what plays
 * (the user, 2.5-b28: METRONOME turned on or off clicked) */
static int prefs_quiet(void) { return !holding_any() && frasil_out_level() < 0.001f; }   /* (-60 dB: the tails gone) */
static void prefs_save(void);
static void prefs_later(void)            /* now if nothing sounds, else once nothing does */
{
    if (prefs_quiet() && !st.store_hold)
        prefs_save();
    else if (!st.prefs_due)
        st.prefs_due = 1;
}
static void prefs_save(void)
{
    if (st.store_hold) {
        st.prefs_due = 1;
        return;
    }
    st.prefs_due = 0;
    if (set.magic != SET_MAGIC) {
        store_all();
        return;
    }
    set.metro_off = !st.metro;
    set.speaker_off = !st.v[P_SPEAKER];
    set.anim = st.v[P_ANIM];
    set.clock = st.v[P_CLOCK];
    set.load = st.v[P_LOAD];
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0)
        store_old &= ~2u;
}

static void name_open(uint32_t s);      /* (STORE's name, below) */

static void scene_save(uint32_t s)
{
    scene_t sc;
    uint32_t i;
    char say2[4 + SC_NAME], t[SC_NAME + 1];
    if (store_held())
        return;
    scene_get(s, &sc);                   /* keeps its name */
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
    if (scenes_save() == 0 && store_all() == 0) {
        store_old = 0;
        sound_mark();
        say2[0] = (char)('0' + (s + 1u) / 10u);   /* "03 ABYSS" */
        say2[1] = (char)('0' + (s + 1u) % 10u);
        say2[2] = ' ';
        say2[3] = 0;
        text_cat(say2, name_text(&sc, t));
        app_say("STORED", say2);
        name_open(s);
    } else {
        popup(POP_WARN, "SCENES", "NOT STORED", "THE MEMORY DID NOT TAKE IT.", "TRY AGAIN.", "");
    }
}

/* SAVE held and REC: STORE, into the slot K1 points at on SCENES, else into the current scene. NEW SHARD is no slot: on
 * it, nothing (CHOOSE A SLOT); the current scene, into the first EMPTY slot (9 .. 16, nothing saved in it: a factory
 * scene stays), else SCENES on slot 16, to choose one */
static void scene_store(void)
{
    uint32_t s = st.page == PG_SCENES ? st.pick : st.scene;
    if (s >= NSCENES && st.page != PG_SCENES)
        for (s = NFACTORY; s < NSCENES && (scn.used >> s & 1u); s++)
            ;
    if (s < NSCENES) {
        scene_save(s);
        return;
    }
    if (st.page != PG_SCENES) {
        st.prev_page = st.page;
        st.page = PG_SCENES;
        st.pick = NSCENES - 1u;
        st.logo = st.help = 0;
    }
    app_say("STORE", "CHOOSE A SLOT");
}

/* ------------------------------------------------------------ a name --- */
/* STORE written: the scene's name on the whole screen (ui.c name_screen), to change or keep. OCT- and OCT+ move the
 * cursor along the sixteen places; K4 turns its letter (a space before A), K3 its digit (and - .); SEL (OK) stores the name (the
 * scenes written again if it changed); FX leaves it as it was stored. Meanwhile the keys and MIDI play; the other
 * buttons and the encoders wait */
static const char NM_LETTERS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ", NM_DIGITS[] = "0123456789-.";
static struct {
    uint8_t on, slot, cur, drawn;
    uint32_t btn;                        /* the buttons down last frame: a press acts */
    char name[SC_NAME + 1];              /* sixteen characters, a space for none */
} nm;

static void name_open(uint32_t s)
{
    uint32_t i, end = 0;
    for (i = 0; i < SC_NAME; i++) {
        end |= !scn.s[s].name[i];
        nm.name[i] = end ? ' ' : scn.s[s].name[i];
    }
    nm.name[SC_NAME] = 0;
    nm.slot = (uint8_t)s;
    nm.cur = 0;
    nm.on = 1;
    nm.drawn = 0;
    nm.btn = plat_buttons();             /* (SAVE and REC, still down from STORE: no press) */
    st.hold_done = ~0u;
}

static void name_turn(const char *set, int32_t n, int32_t d)
{
    int32_t i;
    for (i = 0; i < n && set[i] != nm.name[nm.cur]; i++)
        ;
    if (i == n)                          /* (a character of neither: from the set's start, or its end) */
        i = d > 0 ? -1 : 0;
    i = ((i + d) % n + n) % n;
    nm.name[nm.cur] = set[i];
    nm.drawn = 0;
}

static void name_close(int keep)
{
    char *d = scn.s[nm.slot].name;
    int32_t i, n = (int32_t)SC_NAME;
    nm.on = 0;
    while (n > 0 && nm.name[n - 1] == ' ')
        n--;
    if (!keep || !n)                     /* (all spaces: the name as stored) */
        return;
    for (i = 0; i < (int32_t)SC_NAME && (i < n ? d[i] == nm.name[i] : !d[i]); i++)
        ;
    if (i == (int32_t)SC_NAME)           /* unchanged */
        return;
    if (store_held())
        return;
    for (i = 0; i < (int32_t)SC_NAME; i++)
        d[i] = i < n ? nm.name[i] : 0;
    if (scenes_save() == 0) {
        char t[4 + SC_NAME];
        t[0] = (char)('0' + (nm.slot + 1u) / 10u);
        t[1] = (char)('0' + (nm.slot + 1u) % 10u);
        t[2] = ' ';
        for (i = 0; i < n; i++)
            t[3 + i] = nm.name[i];
        t[3 + n] = 0;
        app_say("NAMED", t);
    } else {
        popup(POP_WARN, "SCENES", "NOT NAMED", "THE MEMORY DID NOT TAKE IT.", "THE NAME STAYS AS IT WAS.", "");
    }
}

/* the name up, a frame's input: 1 still up (the buttons, for the rest of the frame, none) */
static int name_input(uint32_t btn)
{
    uint32_t down = btn & ~nm.btn, e;
    nm.btn = btn;
    for (e = 0; e < NE; e++) {
        int32_t d = plat_enc((int)e);
        if (d && e == EN_K4)
            name_turn(NM_LETTERS, (int32_t)sizeof NM_LETTERS - 1, d);
        else if (d && e == EN_K3)
            name_turn(NM_DIGITS, (int32_t)sizeof NM_DIGITS - 1, d);
    }
    if ((down >> B_OCTDN & 1u) && nm.cur > 0) {
        nm.cur--;
        nm.drawn = 0;
    }
    if ((down >> B_OCTUP & 1u) && nm.cur < SC_NAME - 1u) {
        nm.cur++;
        nm.drawn = 0;
    }
    if (down >> B_SEL & 1u)
        name_close(1);
    else if (down >> B_FX & 1u)
        name_close(0);
    else
        return 1;
    btn_prev = btn;                      /* (what is down now: used, no tap on its release) */
    st.hold_done = ~0u;
    return 0;
}

/* a scene, as SCENES' MORPH and LOAD have it: LOAD SND+NOTES lets everything go and plays the scene's notes (as
 * scene_load); LOAD SND ONLY leaves what plays (the drones, the bass, GLINT's arpeggio and sequence, with its SEQ and
 * the settings that play it) and changes the sound alone. MORPH, CUT or a time: the sound glides there, the settings
 * of a few choices (presets, MODEL, ROOT, TARGET ...) at once, each a hand turns out of it */
static const uint8_t PLAYS_P[8] = {P_SQ_TYPE, P_ARP_MODE, P_ARP_RATE, P_ARP_RANGE, P_ARP_CHANCE, P_SQ_LEN, P_SQ_GATE,
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
    if (st.v[P_LOAD]) {
        uint8_t kept[sizeof PLAYS_P];
        for (i = 0; i < sizeof PLAYS_P; i++)
            kept[i] = st.v[PLAYS_P[i]];
        for (i = 0; i < NSOUND; i++)
            st.v[i] = sc.v[i] > PD[i].max ? PD[i].max : sc.v[i];
        drone_take();
        st.scene = (uint8_t)s;
        sound_mark();                    /* (the scene as saved: its own sequences) */
        for (i = 0; i < SC_SEQ; i++)
            saved_seq[i] = sc.seq[i];
        memcpy(saved_bnote, sc.bnote, sizeof saved_bnote);
        memcpy(saved_blen, sc.blen, sizeof saved_blen);
        memcpy(saved_bq, &sc.bq_on, sizeof saved_bq);
        for (i = 0; i < sizeof PLAYS_P; i++)
            st.v[PLAYS_P[i]] = kept[i];
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
        name_text(&sc, mo.name);
    } else {
        char t[SC_NAME + 1];
        app_say("LOADED", name_text(&sc, t));
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
 * kept SPACE; before GLINT's SEQ, MODE's OFF, five and SEQ in SEQ and MODE, the delay's MODE in its TIME); GLINT's
 * RELEASE, where it kept SCATTER (every layout before FRSl, FRSJ and FRSn: scatter), as its DECAY let a glint go then
 * (glint_release_of) */
static void sound_over(uint8_t *d, const uint8_t *v, uint32_t nv, int scatter)
{
    uint32_t i;
    for (i = 0; i < nv; i++)
        d[i] = v[i];
    later_defaults(d, nv);
    if (nv == 64u)
        d[P_BS_MODEL] = PD[P_BS_MODEL].def;
    if (nv == 94u) {                     /* the second pages' first layout: what has changed since at its default (SEQ
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
        d[P_GD_TIME] = (uint8_t)(v[P_GD_TIME] % 12u);   /* (MODE gone: ping-pong always, since 2.5) */
    }
    if (scatter)
        d[P_GL_RELEASE] = glint_release_of(d[P_GL_DECAY]);
}

static void learn_scatter_gone(void)     /* a CC an earlier layout learned on SCATTER: learned no more */
{
    uint32_t j;
    for (j = 0; j < NLEARN; j++)
        if (set.learn_p[j] == P_GL_RELEASE)
            set.learn_cc[j] = set.learn_p[j] = 0;
}

/* before GLINT's SEQ, the arpeggio's and the sequence's notes were kept with GLINT's OCTAVE on them: without it now */
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

static int scenes_over(void)              /* the scenes in an earlier layout, if that is what the store holds: 1 */
{
    int n = plat_store_load(OBJ_SCENES, raw, sizeof raw);
    uint32_t k, s, j;
    for (k = 0; k < sizeof OLD_SCN / sizeof OLD_SCN[0]; k++) {
        const layout_t *l = &OLD_SCN[k];
        uint32_t rs = scn_bytes(l), nv = l->nv, nl = name_bytes(l);
        if (n != (int)(8u + NSCENES * rs) || rd32(raw) != l->magic)
            continue;
        for (s = 0; s < NSCENES; s++) {
            const uint8_t *o = raw + 8u + s * rs;
            scene_t *d = &scn.s[s];
            for (j = 0; j < SC_NAME; j++)
                d->name[j] = j < nl ? (char)o[j] : 0;
            o += nl;                     /* (the rest from the sound on) */
            sound_over(d->v, o, nv, l->extra < 4u);
            d->nch = o[nv];
            d->narp = o[1u + nv];
            for (j = 0; j < SC_CHORDS * 5u; j++)
                d->ch[j / 5u][j % 5u] = (int8_t)o[2u + nv + j];
            for (j = 0; j < SC_ARP; j++)
                d->arp[j] = j < d->narp && nv <= P_SQ_TYPE ? note_down((int8_t)o[2u + nv + SC_CHORDS * 5u + j], d->v)
                                                           : (int8_t)o[2u + nv + SC_CHORDS * 5u + j];
            d->bass = l->extra ? (int8_t)o[2u + nv + SC_CHORDS * 5u + SC_ARP] : -1;
            for (j = 0; j < SC_SEQ; j++)  /* (none, before it had one) */
                d->seq[j] = seq_bytes(l) ? o[3u + nv + SC_CHORDS * 5u + SC_ARP + j] : SQ_REST;
            if (nv <= P_SQ_TYPE)         /* (before SEQ: with GLINT's OCTAVE on them) */
                seq_down(d->seq, d->v);
        }
        scn.used = (uint16_t)(raw[4] | raw[5] << 8);
        scn.magic = SCN_MAGIC;
        return l->extra == 5u ? 2 : 1;   /* (2: its sequences in OBJ_BSEQ, FRBU's, as this one's) */
    }
    return 0;
}

static int settings_over(void)            /* the settings in an earlier layout, the same (the bass's sequence none) */
{
    int n = plat_store_load(OBJ_SETTINGS, raw, sizeof raw);
    uint32_t k, j;
    set.bq_steps = PD[P_BQ_STEPS].def;
    set.bq_trans = PD[P_BQ_TRANS].def;
    if (n == (int)FRSK_BYTES && rd32(raw) == SET_FRSK) {
        /* FRSK, 2.5's betas': this layout, its sound FRSO_NV long, ANIMATIONS not yet (its padding byte there: NORMAL).
         * A CC learned on a setting after the sound: on the same setting, its number moved on by the sounds added */
        sound_over(set.v, raw + 4, FRSO_NV, 0);
        memcpy((uint8_t *)&set + offsetof(settings_t, octave), raw + 4u + FRSO_NV, FRSK_BYTES - 4u - FRSO_NV);
        set.anim = 0;
        set.clock = 0;
        for (j = 0; j < NLEARN; j++)
            if (set.learn_p[j] >= FRSO_NV && set.learn_p[j] < NPARAM - (NSOUND - FRSO_NV))
                set.learn_p[j] = (uint8_t)(set.learn_p[j] + (NSOUND - FRSO_NV));
        set.magic = SET_MAGIC;
        return 1;
    }
    if (n == (int)FRSJ_BYTES && (rd32(raw) == SET_FRSJ || rd32(raw) == SET_FRSI)) {
        /* FRSJ: this layout, its sound FRSL_NV long; FRSI: that, SCATTER where GLINT's RELEASE is. A CC learned on a
         * setting after the sound: on the same setting, its number moved on by the sounds added */
        int sc = rd32(raw) == SET_FRSI;
        sound_over(set.v, raw + 4, FRSL_NV, sc);
        memcpy((uint8_t *)&set + offsetof(settings_t, octave), raw + 4u + FRSL_NV, FRSJ_BYTES - 4u - FRSL_NV);
        set.speaker_off = 0;             /* (MORPH's byte there, SPEAKER's now: on) */
        set.anim = 0;                    /* (its padding byte there) */
        set.clock = 0;
        for (j = 0; j < NLEARN; j++)
            if (set.learn_p[j] >= FRSL_NV && set.learn_p[j] < NPARAM - (NSOUND - FRSL_NV))
                set.learn_p[j] = (uint8_t)(set.learn_p[j] + (NSOUND - FRSL_NV));
        if (sc)
            learn_scatter_gone();
        set.magic = SET_MAGIC;
        return 1;
    }
    if ((n == (int)FRSH_BYTES && rd32(raw) == SET_FRSH) || (n == (int)FRSG_BYTES && rd32(raw) == SET_FRSG)) {
        /* FRSH: this layout, its sound FRSH_NV long; FRSG: FRSH's first bytes, the bass's sequence not yet after them.
         * A CC learned on a setting after the sound: on the same setting, its number moved on by the sounds added */
        sound_over(set.v, raw + 4, FRSH_NV, 1);
        memcpy((uint8_t *)&set + offsetof(settings_t, octave), raw + 4u + FRSH_NV,
               (n == (int)FRSH_BYTES ? FRSH_BYTES : FRSH_BSEQ) - 4u - FRSH_NV);
        set.speaker_off = 0;
        for (j = 0; j < NLEARN; j++)
            if (set.learn_p[j] >= FRSH_NV && set.learn_p[j] < NPARAM - (NSOUND - FRSH_NV))
                set.learn_p[j] = (uint8_t)(set.learn_p[j] + (NSOUND - FRSH_NV));
        learn_scatter_gone();
        set.magic = SET_MAGIC;
        return 1;
    }
    for (k = 0; k < sizeof OLD_SET / sizeof OLD_SET[0]; k++) {
        const layout_t *l = &OLD_SET[k];
        const uint8_t *o = raw + 4u + l->nv;
        if (n != (int)set_bytes(l) || rd32(raw) != l->magic)
            continue;
        sound_over(set.v, raw + 4, l->nv, 1);
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
        return 1;
    }
    return 0;
}

/* the state as at power-on (nothing latched, held or to undo, nothing playing; the keys the drones'), from the store:
 * the scenes and the settings (an earlier layout's made over), the sound and the rest as the settings have them, else
 * the first scene. At boot, and after a restore from USB. The sequences that are on wait for PLAY (tapped: from their
 * first steps, the beat beginning there; held: the scene's notes too): begun here, they started off the beat (a
 * sixteenth into a grid no top had set: the bass's NOTEs off PULSE's strikes), and alone */
static void state_load(void)
{
    uint32_t i;
    st.octave = OCT_DEF;
    st.metro = 1;
    st.glint = st.bass = 0;
    st.compare = st.help = st.rec_took = st.sustain = 0;
    st.prefs_due = 0;
    store_old = 0;
    rst_valid = seq_had = 0;             /* (nothing to restore or undo yet; the host's reboot keeps statics) */
    mo.on = 0;
    memset(morph_off, 0, sizeof morph_off);
    arp.latch = 1;                       /* the arpeggio latched from the start, empty */
    arp.n = 0;
    bass_note = -1;
    sq.run = 0;
    bq.run = bq.open = bq.wrap = 0;
    bq.tn = bq.skip = -1;
    for (i = 0; i < NPARAM; i++)
        st.v[i] = PD[i].def;
    int cur = plat_store_load(OBJ_SCENES, raw, sizeof raw) == (int)SCN_BYTES && rd32(raw) == SCN_MAGIC, over = 0;
    if (cur) {
        memcpy(&scn, raw, 8u);
        for (i = 0; i < NSCENES; i++)
            memcpy(&scn.s[i], raw + 8u + i * SC_REC, SC_REC);
    } else {
        scn.used = 0;
        over = scenes_over();
        store_old |= (uint8_t)(over != 0);
        scn.magic = SCN_MAGIC;
    }
    {                                    /* their sequences, basses and TEMPOs: FRBU's, GLINT's sequence first; FRBT's,
                                          * the bass from its notes on (GLINT's sequence in an earlier scenes' record);
                                          * FRBS's without TEMPO (the settings', below); none: each empty */
        int n = plat_store_load(OBJ_BSEQ, raw, sizeof raw), ok = n == (int)BSQ_BYTES && rd32(raw) == BSQ_MAGIC;
        int frbt = n == (int)(4u + NSCENES * FRBT_SCENE) && rd32(raw) == BSQ_FRBT;
        int frbs = n == (int)(4u + NSCENES * FRBS_SCENE) && rd32(raw) == BSQ_FRBS;
        for (i = 0; i < NSCENES; i++) {
            if (ok && (cur || over == 2))   /* (FRSo's scenes: their sequences there too) */
                memcpy(scn.s[i].seq, raw + 4u + i * BSQ_SCENE, BSQ_SCENE);
            else if (ok)                 /* (an earlier scenes' record, restored alone: its own sequence kept) */
                memcpy(scn.s[i].bnote, raw + 4u + i * BSQ_SCENE + SC_SEQ, FRBT_SCENE);
            else if (frbt || frbs)
                memcpy(scn.s[i].bnote, raw + 4u + i * (frbt ? FRBT_SCENE : FRBS_SCENE), frbt ? FRBT_SCENE : FRBS_SCENE);
            else
                bseq_empty(&scn.s[i]);
            if (!ok && (cur || over == 2))   /* (this scenes' record without its sequences: none) */
                memset(scn.s[i].seq, SQ_REST, SC_SEQ);
            if (!ok && !frbt)
                scn.s[i].tempo = 0xFFu;
        }
        if ((frbt || frbs) && scn.used)   /* (the store's still in an earlier layout: a backup saves them first) */
            store_old |= 1u;
    }
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC) {
        memset(&set, 0, sizeof set);     /* (nothing learned, the defaults, whatever was read) */
        store_old |= (uint8_t)(settings_over() << 1);
    }
    for (i = 0; i < NSCENES; i++)        /* scenes kept before their TEMPO: the one they were played at (the settings') */
        if (scn.s[i].tempo > PD[P_TEMPO].max)
            scn.s[i].tempo = set.magic == SET_MAGIC && set.tempo <= PD[P_TEMPO].max ? set.tempo : PD[P_TEMPO].def;
    if (set.magic == SET_MAGIC) {
        for (i = 0; i < NSOUND; i++)
            st.v[i] = set.v[i] > PD[i].max ? PD[i].max : set.v[i];
        drone_take();
        st.octave = (int8_t)clamp(set.octave, -3, 2);
        st.metro = !set.metro_off;
        st.v[P_MORPH] = 0;               /* (CUT at every start, whatever it was left on) */
        st.v[P_LOAD] = set.load ? 1 : 0;
        st.v[P_SPEAKER] = !set.speaker_off;
        st.v[P_ANIM] = set.anim ? 1 : 0;
        st.v[P_CLOCK] = set.clock == 1u;
        st.v[P_KEYS] = set.keys ? 1 : 0;
        st.v[P_CANAL] = set.channel > 17u ? PD[P_CANAL].def : set.channel;
        st.v[P_CH_GLINT] = set.ch_glint > 17u ? PD[P_CH_GLINT].def : set.ch_glint;
        st.v[P_CH_BASS] = set.ch_bass > 17u ? PD[P_CH_BASS].def : set.ch_bass;
        st.scene = set.scene == SC_NEW ? SC_NEW : set.scene % NSCENES;
        st.v[P_TEMPO] = set.tempo > PD[P_TEMPO].max ? PD[P_TEMPO].def : set.tempo;
        for (i = 0; i < SC_SEQ; i++)
            seq[i] = set.seq[i];
        seq_view();
        memcpy(bnote, set.bnote, sizeof bnote);
        memcpy(blen, set.blen, sizeof blen);
        bseq_view();
        st.v[P_BQ_ON] = set.bq_on ? 1 : 0;
        st.v[P_BQ_STEPS] = set.bq_steps > PD[P_BQ_STEPS].max ? PD[P_BQ_STEPS].def : set.bq_steps;
        st.v[P_BQ_TRANS] = set.bq_trans > PD[P_BQ_TRANS].max ? PD[P_BQ_TRANS].def : set.bq_trans;
    } else {                             /* (never saved: NEW SHARD, to begin from) */
        scene_load(SC_NEW);
    }
    plat_speaker(st.v[P_SPEAKER] && !spk_page);
    st.pick = st.scene;
    st.page = PG_SON;
    sound_mark();                        /* the sound saved last: nothing changed yet */
    engine_push();
}

void app_init(void)
{
    arp.clk_at = plat_ms() - 60000u;     /* no MIDI clock yet (else the first half second would follow one: the grid
                                          * still, a scene loaded then not from its top) */
    frasil_init();
    frasil_profile(plat_ticks);          /* (where the render's time goes: the log's word) */
    drone_init();
    state_load();
    st.booting = 1;
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

/* SEQ LENGTH as it plays now, 1 .. SC_SEQ: with what the LFOs add to it */
static uint32_t seq_len(void)
{
    float c = (float)st.v[P_SQ_LEN] + mod[P_SQ_LEN];
    return c <= 0.0f ? 1u : c >= (float)(SC_SEQ - 1u) ? SC_SEQ : (uint32_t)(c + 0.5f) + 1u;
}

/* a step's note as it plays: TRANSPOSE's steps along the scale (OCTAVE on it in the engine, as on every glint) */
static int seq_note(uint32_t b)
{
    int n = scale_shift((int)(b & 127u), (int)st.v[P_SQ_TRANS] - 14, st.v);
    return n > 127 ? 127 : n;
}

/* the sequencer on the grid: its note off at GATE's end (a TIE on the next step holds it on), the next step's on */
static void seq_run(uint32_t now, int32_t stepn, uint32_t period)   /* stepn: its own (seq_stepn) */
{
    uint32_t len = seq_len(), b, nb;
    int32_t pos, was = sq.n;
    uint32_t gate = (uint32_t)((float)period * (0.05f + 0.95f * (float)st.v[P_SQ_GATE] * (1.0f / 127.0f)));
    if (sq.sounding >= 0 && (int32_t)(now - sq.off_ms) >= 0)
        seq_note_off();
    sq.n = stepn;
    if (sq.restart) {                    /* MIDI Start: its first step on the grid's top */
        sq.restart = 0;
        sq.run = 1;
        sq.start = stepn;
        sq.pos = sq.skip = -1;
        was = stepn - 1;
    }
    if (!sq.run || stepn == was)
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

/* a GLINT key or note while the sequencer has them (seq_keys): RECORD OFF, the sequence transposed from its first note
 * to this one along the scale and, playing, begun again from its first step on the nearest beat (seq_due); STEP,
 * written at STEP, then on to the next (it sounds while down); LIVE, written on the step nearest the playhead (it
 * sounds as it is played; ahead of the playhead, that step skips this time round) */
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
        float g = seq_grid(grid_now()) / (float)ARP_TICKS[st.v[P_ARP_RATE] % 10u] - (float)sq.start, f;
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
        uint8_t t = st.v[P_SQ_TRANS];
        for (i = 0; i < len; i++)        /* from the first note (not a rest, not a tie) */
            if ((seq[i] & 127u) != SQ_REST && (seq[i] & 127u) != SQ_TIE)
                break;
        if (i < len)
            t = (uint8_t)clamp(14 + scale_steps(seq[i] & 127, n, st.v), 0, 28);
        if (sq.run) {                    /* playing: again from its first step on the nearest beat, TRANSPOSE with
                                          * it (seq_due): the one just gone, at once; in a beat's second half, the
                                          * next (what plays till then as it was) */
            float g = grid_now(), at = 24.0f * fm_floorf(g / 24.0f);
            if (g - at >= 12.0f)
                at += 24.0f;
            sq.at = at >= 9600.0f ? at - 9600.0f : at;
            sq.at_trans = t;
        } else {
            st.v[P_SQ_TRANS] = t;
        }
    }
    st.dirty = (uint8_t)sound_changed();
}

/* a key's restart (seq_key), the grid on its beat: the sequence from its first step there, its steps on that beat
 * from then on (sq.off), the one it is on now at once (seq_run: its NOTE sounds, as late as the key came; a TIE or a
 * REST, none), TRANSPOSE from then; already playing so, nothing again. Recording, or the sequence stopped meanwhile:
 * none */
static void seq_due(uint32_t ticks)
{
    float d = arp.tick - sq.at;
    uint8_t off = sq.off;
    int32_t start = sq.start;
    if (sq.at < 0.0f)
        return;
    if (!sq.run || st.v[P_SQ_REC]) {
        sq.at = -1.0f;
        return;
    }
    if (d < -4800.0f)                    /* (the grid wraps at 9600) */
        d += 9600.0f;
    else if (d > 4800.0f)
        d -= 9600.0f;
    if (d < 0.0f)
        return;
    sq.off = (uint8_t)((uint32_t)sq.at % ticks);
    sq.start = seq_stepn(sq.at, ticks);
    sq.at = -1.0f;
    if (sq.off == off && (sq.start - start) % (int32_t)seq_len() == 0 && st.v[P_SQ_TRANS] == sq.at_trans &&
        sq.pos >= 0)
        return;                          /* (its steps fall there already, as transposed: no note twice) */
    sq.n = seq_stepn(arp.tick, ticks) - 1;
    sq.pos = sq.skip = -1;
    seq_note_off();
    if (st.v[P_SQ_TRANS] != sq.at_trans) {
        st.v[P_SQ_TRANS] = sq.at_trans;
        st.dirty = (uint8_t)sound_changed();
    }
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
        seq_due(ticks);
        seq_run(now, seq_stepn(arp.tick, ticks), period);
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
        int note = arp.play[arp.idx % (int32_t)arp.pn] + 12 * (arp.idx / (int32_t)arp.pn);   /* (OCTAVE: the engine's) */
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

/* REC's metronome, while a sequence records LIVE (GLINT's, else the bass's): a tick on each of its beats (24 MIDI
 * clocks) counted from its first step, the higher one as it comes round to that step (a bass take's loop still open,
 * growing: on each bar line too); a turn that is not a whole number of beats, its beats from its top again. Begun on
 * a sequence playing, from its next beat. On STEP, or the sequence stopped, none. Its beats are REC's flash's too
 * (mtr.at) */
static struct {
    float at;                            /* the clocks into the sequence's beat now, -1: none */
    float t;                             /* the clocks into its turn at the last run, -1: it is about to begin (its
                                          * first step ticks), -2: none (as it comes, its beat ticks only if it began
                                          * since the last run) */
    float g;                             /* the grid at the last run */
    uint8_t src;                         /* the sequence it follows: 1 GLINT's, 2 the bass's, 0 none */
    float ck;                            /* the grid at its last tick (the tests) */
} mtr = {-1.0f, -2.0f, 0.0f, 0, 0.0f};

static void metro_run(void)
{
    float t = -1.0f, dg = arp.tick - mtr.g;
    uint32_t src = 0, k, tick, hi;
    int32_t b;
    if (dg < -4800.0f)                   /* (the grid wraps at 9600; MIDI Start puts it back to 0) */
        dg += 9600.0f;
    else if (dg < 0.0f)
        dg = 0.0f;
    mtr.g = arp.tick;
    if (!st.v[P_SQ_REC] && !st.v[P_BQ_REC])
        st.clicks = st.hiclicks = 0;
    if (st.v[P_SQ_REC] == 2u && sq.run) {   /* its step, and how far into it on the grid */
        src = 1;
        k = ARP_TICKS[st.v[P_ARP_RATE] % 10u];
        if (sq.pos >= 0 && !sq.restart)
            t = (float)((uint32_t)sq.pos * k) + seq_grid(arp.tick) - (float)sq.n * (float)k;
    } else if (st.v[P_BQ_REC] == 2u && bq.run) {   /* the steps before it, and how far into it */
        src = 2;
        if (bseq_playing())
            t = bseq_at();
    } else if (st.v[P_SQ_REC] == 2u || st.v[P_BQ_REC] == 2u) {   /* LIVE, stopped: PLAY starts it from the top */
        src = st.v[P_SQ_REC] == 2u ? 1u : 2u;
    }
    if (src != mtr.src || (src == 2u && bq.moved))   /* (another sequence, or none, or the bass's playhead put
                                                      * elsewhere: from its next beat) */
        mtr.t = -2.0f;
    mtr.src = (uint8_t)src;
    bq.moved = 0;
    if (t < 0.0f) {                      /* (not begun yet, or stopped: it will, from its first step) */
        mtr.at = -1.0f;
        if (src)
            mtr.t = -1.0f;
        return;
    }
    b = (int32_t)(t * (1.0f / 24.0f));
    mtr.at = t - 24.0f * (float)b;
    if (mtr.t == -2.0f)
        tick = mtr.at < dg;
    else
        tick = mtr.t < 0.0f || t < mtr.t || b != (int32_t)(mtr.t * (1.0f / 24.0f));
    mtr.t = t;
    hi = b == 0 || (src == 2u && bq.open && !(b & 3));
    if (tick && st.metro) {
        frasil_click((int)hi);
        st.clicks++;
        st.hiclicks += hi;
        mtr.ck = arp.tick;
    }
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
        int note = (int)n + (arp_rand() % 3u == 0 ? 12 : 0);   /* (OCTAVE on it in the engine) */
        while (note + 12 * (int)st.v[P_GL_OCTAVE] > 120)
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
        popup(POP_WARN, "MIDI LEARN", "TURN ONE FIRST", "TURN A SETTING ON THE FM-1,", "THEN HOLD GLO AND MOVE",
              "A KNOB ON YOUR CONTROLLER.");
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
    prefs_later();
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
    if (seq_keys()) {                    /* the sequencer's: the note sounding, louder and longer */
        if (sq.sounding >= 0)
            frasil_glint_pressure(sq.sounding, f);
        else if (st.v[P_SQ_REC])
            frasil_glint_pressure(n, f);
        return;
    }
    if (st.v[P_SQ_TYPE] == SQT_ARP) {
        n = bit_get(mgl, n) ? mgl_base[n] : n;
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
 * GLINT's and BASS's notes play as they come (no scale: the sequencer's own), their OCTAVE on them as on a key's (the
 * engine's: into the arpeggio and the sequences they go as they come, as a key's); the drones' go through the
 * harmony, as ever */
static int on_ch(int p, uint32_t c) { return st.v[p] > 16u || (st.v[p] && st.v[p] == c); }

static void midi_glint_on(int n, uint32_t vel)
{
    uint32_t i, held = 0;
    int sk = seq_keys();
    for (i = 0; i < 4u; i++)
        held |= mgl[i];
    bit_put(mgl, n, 1);
    mgl_base[n] = (int8_t)n;             /* (as it comes: OCTAVE on it in the engine, as on a key's) */
    bit_put(mgs, n, sk ? st.v[P_SQ_REC] != 0 : st.v[P_SQ_TYPE] != SQT_ARP);
    if (sk) {                            /* into the sequencer, as a key would */
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
    if (bseq_keys())                     /* into the bass's sequence, as a key would */
        bseq_key(n, n, vel);
    else
        bass_live(n, vel);
}

static void midi_bass_off(int n)          /* the one on top let go: back to the one under it, else silence */
{
    uint32_t i, j;
    int top = mb_n ? mbass[mb_n - 1] : -1;
    for (i = j = 0; i < mb_n; i++)
        if (mbass[i] != n)
            mbass[j++] = mbass[i];
    mb_n = j;
    if (bseq_keys()) {                   /* (the bass's sequence's) */
        bseq_up(n);
        return;
    }
    if (n != top || bass_note != n)
        return;                          /* not the one sounding (or the keys have the bass now) */
    if (mb_n)
        bass_live(mbass[mb_n - 1], 100);
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

static int holding_any(void)
{
    return lit[0] | lit[1] | lit[2] | lit[3] || bass_note >= 0 || arp.n || sq.run || bq.run;
}

/* the grid from its top, now: its beat and its bar begin here (MIDI Start; PLAY starting the transport, RELEASE ALL,
 * RESTORE). The LFOs on the grid from the top of their turn, the arpeggio from the top of its pattern; OCT's blink, REC's
 * flash, the metronome, PUMP and PULSE's divisions (one on the top itself) follow it. Each that follows the grid from
 * its last run starts from there (no jump round the 9600). Following a MIDI clock, only its own Start does it (start):
 * PLAY leaves the clock's beats as they are, 0 */
static int grid_top(int start)
{
    if (!start && plat_ms() - arp.clk_at < 500u)
        return 0;
    arp.clk = arp.clk_new = 0;
    arp.tick = 0.0f;
    arp.step_n = -1;
    arp.idx = -1;
    lfo_restart = 1;
    lfo_grid = mtr.g = bq.g = 0.0f;
    bp.g = -1.0f;
    return 1;
}

/* the sequences from their first steps at the grid's top (grid_top just done), at once: GLINT's, the bass's */
static void seqs_top(int glint, int bass)
{
    if (glint) {
        sq.restart = 1;
        sq.off = 0;
        sq.at = -1.0f;
    }
    if (bass) {
        bseq_begin();
        bq.wait = 2;
    }
}

/* RELEASE ALL: PLAY held, or MIDI's All Sound Off (CC 120, on any channel: the River sends it before it sends a shard
 * or a backup, so that the FM-1 plays nothing when the restore comes). Everything let go (the drones, the bass,
 * GLINT's notes, the arpeggio, the sequences), what was held kept for RESTORE (the sequences alone running: what was
 * kept before stays) */
static void sound_off(void)
{
    if (lit[0] | lit[1] | lit[2] | lit[3] || arp.n || (bass_note >= 0 && !st.v[P_KEYS] && !bseq_keys())) {
        scene_notes_take(&rst);          /* (notes a scene keeps: a sequence's own are not) */
        rst_valid = 1;
    }
    release_all();
    arp.n = arp.pn = 0;
    arp.dirty = 0;
    arp.paused = 0;
    seq_note_off();
    sq.run = 0;
    bseq_stop();
}

static void midi_in(void)
{
    uint32_t pkt, i;
    int port, root, dr, gl, bs;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st8 = (pkt >> 8) & 0xFFu, ch = st8 & 15u, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        uint32_t kind = st8 & 0xF0u;
        if ((st8 == 0xF8u || st8 == 0xFAu) && st.v[P_CLOCK])   /* CLOCK INTERNAL: clocks and Start not taken */
            continue;
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
        if (st8 == 0xFAu) {              /* start: the grid from its top, GLINT's sequence (SEQ on SEQ) and the bass's
                                          * (SEQUENCE on) from their first steps there */
            grid_top(1);
            seqs_top(st.v[P_SQ_TYPE] == SQT_SEQ, st.v[P_BQ_ON]);
            continue;
        }
        if (st8 < 0x80u || st8 >= 0xF0u)
            continue;
        if (kind == 0xB0u && d1 == 120u) {           /* all sound off: everything, whatever the channel */
            sound_off();
            continue;
        }
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
            if (bs && mb_n && mbass[mb_n - 1] == (int8_t)d1 && bass_note == (int)d1)
                frasil_bass_pressure(f);
        } else if (kind == 0xD0u) {                  /* the channel's: every note of the track */
            float f = (float)d1 * (1.0f / 127.0f);
            if (dr)
                st.swell_midi = f;
            if (gl)
                for (i = 0; i < 128u; i++)
                    if (bit_get(mgl, (int)i))
                        glint_note_press((int)i, f);
            if (bs && mb_n && bass_note == mbass[mb_n - 1])
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
            } else if (d1 == 123u) {             /* all notes off: the track's */
                if (dr)
                    release_drones();
                if (gl)
                    midi_glints_off();
                if (bs && mb_n) {
                    int own = bass_note == mbass[mb_n - 1];   /* (else the keys have the bass) */
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
        } else if (kind == 0xC0u && dr && d1 < NSCENES) {   /* Program Change: a scene, as SCENES' MORPH and LOAD */
            scene_go(d1);
        }
    }
}

/* ---------------------------------------------------------------- input --- */
/* a page come to from another button's (from: the page before; a tap, a page button held and SELECT's first detent,
 * HOME): the keys play its track. ARP's pages GLINT, SEQ's BASS; the drone's (EDIT's, ENV's) and HOME's DRONE. The
 * others (FX's, GLO's, SEL's HARMONY, LFO's, SCENES) leave them as they are, and every page does while a recording has
 * them (rec_keys). Within a button's pages they stay (its taps, round, SELECT's next detents), and SAVE back from
 * SCENES comes back as it left: KEYS turned by hand stay till another button's page; the header says KEYS when they
 * change */
static void keys_follow(uint32_t from)
{
    uint32_t b = PAGES[st.page].button;
    int32_t kb = b == B_ARP ? 1 : b == B_SEQ ? 2 : b == B_EDIT || b == B_ENV || b == B_HOME ? 0 : -1;
    if (kb < 0 || st.rec_took || PAGES[from].button == b || st.v[P_KB] == (uint8_t)kb)
        return;
    param_set(P_KB, kb);                 /* (no words in the header: the page's name stays; the keys' badge says it) */
}

static void page_button(uint32_t b)       /* (a page: the logo goes; PLAY and OCT leave it up) */
{
    uint32_t p, first = NPAGES, from = st.page;
    if (b != B_HOME)
        st.logo = 0;
    st.help = 0;
    for (p = 0; p < NPAGES; p++)
        if (PAGES[p].button == b && !page_hidden(p)) {
            if (first == NPAGES)
                first = p;
            if (p == st.page) {          /* again: its next page shown (SEQ's last two numbered apart) */
                uint32_t q = p + 1u;
                while (q < NPAGES && (PAGES[q].button != b || page_hidden(q)))
                    q++;
                if (q < NPAGES) {
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
    keys_follow(from);
}

/* a page button held and SELECT turned: that button's pages, a detent each, as its taps go (the hidden ones skipped),
 * round; from another button's page, its first (turned back, its last), KEYS as that page has them (keys_follow);
 * the button does nothing more when let go, and its hold, if it has acted already, is undone (hold_undo). SAVE's
 * SCENES apart: SAVE has SELECT's KEYS */
static uint32_t page_held(void)          /* the page button held (the last pressed), NB: none */
{
    uint32_t p, b, got = NB;
    for (p = 0; p < NPAGES; p++) {
        b = PAGES[p].button;
        if (b != B_SAVE && b_held(b) && (got == NB || st.hold_at[b] - st.hold_at[got] < 0x80000000u))
            got = b;
    }
    return got;
}

static void page_turn(uint32_t b, int32_t d)
{
    uint32_t p = st.page, k, from = st.page;
    if (PAGES[p].button != b)            /* (before its first) */
        p = d > 0 ? NPAGES - 1u : 0u;
    for (; d; d += d > 0 ? -1 : 1)
        for (k = 0; k < NPAGES; k++) {
            p = (p + (d > 0 ? 1u : NPAGES - 1u)) % NPAGES;
            if (PAGES[p].button == b && !page_hidden(p))
                break;
        }
    st.page = (uint8_t)p;
    keys_follow(from);
}

/* what a page button's hold did, undone once SELECT takes the button: FREEZE and the arpeggio's LATCH back as they
 * were (the notes LATCH off let go, again: arp_was; LATCH on, GLINT's SEQ as it was, its sequence playing or not,
 * recording or not, and the page), COMPARE over, SWELL down (swell_update); its words gone. HOME's help stays: SELECT
 * turns its pages */
static struct {
    int8_t held[16];
    float prs[16];
    uint8_t n;
    uint8_t typed;                       /* LATCH on turned SEQ to ARP: as it was, its sequence, RECORD, the page */
    uint8_t type, run, rec, page;
} arp_was;
static void say_clear(void);             /* (the screen, below) */
static void say_red(void);

static void hold_undo(uint32_t b)
{
    if (!(st.hold_acted >> b & 1u))
        return;
    st.hold_acted &= ~(1u << b);
    if (b == B_FX) {
        param_set(P_FREEZE, !st.v[P_FREEZE]);
    } else if (b == B_ARP) {
        arp.latch ^= 1u;
        if (arp.latch) {
            memcpy(arp.held, arp_was.held, sizeof arp.held);
            memcpy(arp.prs, arp_was.prs, sizeof arp.prs);
            arp.n = arp_was.n;
            arp.dirty = 1;
        } else {
            arp_keep_held();
        }
        if (arp_was.typed) {
            arp_was.typed = 0;
            param_set(P_SQ_TYPE, arp_was.type);
            if (arp_was.type == SQT_SEQ && !arp_was.run) {   /* (it was stopped: stopped again) */
                seq_note_off();
                sq.run = 0;
            }
            param_set(P_SQ_REC, arp_was.rec);
            st.page = arp_was.page;
        }
    } else if (b == B_EDIT) {
        st.compare = 0;
        st.engine_dirty = 1;
    }
    say_clear();
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
        say_red();                       /* (in bright red: the setting is its own again) */
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

/* OCT- held, a sound's preset turned (GLINT I's PRESET, BASS's MODEL, the drone's PRESETS): picked unheard, what plays
 * going on as it is, then taken when OCT- is let go; its press's octave step undone (OCT- was a hand then) */
static struct {
    uint8_t on, p, v;                    /* picking: the setting, the value picked */
    int8_t oct;                          /* the octave before OCT- went down */
} pv;
static uint32_t pv_shown(int p) { return pv.on && pv.p == (uint8_t)p ? pv.v : st.v[p]; }

/* PLAY's light: the transport's (a sequence on: lit while one runs, or the arpeggio plays), else lit while anything is
 * held (the drones, the bass, the arpeggio's notes, a key, a MIDI note); never for a tail alone: a stop puts it out at
 * once, the sound fading on as it does */
static int play_lit(void)
{
    uint32_t k, midi = mb_n;
    if (st.v[P_SQ_TYPE] == SQT_SEQ || st.v[P_BQ_ON])
        return sq.run || bq.run || (st.v[P_SQ_TYPE] == SQT_ARP && !arp.paused && arp.n);
    for (k = 0; k < 4u; k++)
        midi |= mid_on[k] | mgl[k];
    return holding_any() || keys_prev || midi;
}

static void encoders(void)
{
    uint32_t e, b;
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
        if (e == EN_SELECT && (b = page_held()) < NB) {   /* a page button held: its pages */
            b_used(b);
            hold_undo(b);
            page_turn(b, d);
            continue;
        }
        if (b_held(B_LFO) && e >= EN_K1) {   /* LFO held: the setting under it onto an LFO, the next, or off */
            b_used(B_LFO);
            lfo_route(st.page == PG_SCENES ? NPARAM : PAGES[st.page].p[e - EN_K1], d);
            continue;
        }
        if (st.page == PG_SCENES && (e == EN_K1 || e == EN_SELECT)) {   /* the slots, NEW SHARD above the first:
                                                                          * SELECT as K1 (KEYS stay as they are) */
            p = clamp((st.pick >= NSCENES ? -1 : (int32_t)st.pick) + d, -1, NSCENES - 1);
            st.pick = (uint8_t)(p < 0 ? SC_NEW : p);
            continue;
        }
        if (MACRO_OF[e] < NPARAM) {
            p = MACRO_OF[e];
        } else {
            p = PAGES[st.page].p[e - EN_K1];
            if (p >= NPARAM)                 /* an empty slot */
                continue;
        }
        if (st.page == PG_MIXER && b_held(B_OCTDN) && !b_held(B_OCTUP) && e >= EN_K1) {   /* MIXER, OCT- held: K1 .. K4
                                                                                        * each track's PAN (OCT- a
                                                                                        * hand, its step undone) */
            if (!(st.hold_done >> B_OCTDN & 1u)) {
                st.octave = pv.oct;
                b_used(B_OCTDN);
            }
            p = P_PN_DRONE + (int)(e - EN_K1);
            param_set(p, (int32_t)st.v[p] + accel(e, d));
            st.touched = (uint8_t)p;
            st.touched_enc = (uint8_t)e;
            st.touched_at = plat_ms();
            continue;
        }
        if (b_held(B_OCTDN) && !b_held(B_OCTUP) && (p == P_GL_PRESET || p == P_BS_MODEL || p == P_DR_PRESET)) {
            if (!pv.on) {                /* the first detent: OCT- a hand, its step undone; picking from what plays */
                st.octave = pv.oct;
                b_used(B_OCTDN);
                pv.on = 1;
                app_say("PICKING", "LET GO OCT-");
            } else if (pv.p != (uint8_t)p) {   /* (another one: the first taken as it was picked) */
                param_set(pv.p, pv.v);
            }
            if (pv.p != (uint8_t)p || pv.v > PD[p].max) {
                pv.p = (uint8_t)p;
                pv.v = st.v[p];
            }
            pv.v = (uint8_t)clamp((int32_t)pv.v + d, 0, PD[p].max);
            st.touched = (uint8_t)p;
            st.touched_enc = (uint8_t)e;
            st.touched_at = plat_ms();
            continue;
        }
        if (p == P_KEYS) {               /* KEYS lets the notes go: a second detent the same way, within 2 s */
            int8_t s = d > 0 ? 1 : -1;
            if (st.keys_arm != s || plat_ms() - st.keys_arm_at > 2000u) {
                st.keys_arm = s;
                st.keys_arm_at = plat_ms();
                app_say("KEY MODE", "TURN AGAIN");
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

/* REC tapped: into GLINT's sequencer, recording LIVE (the sequencer on, the keys GLINT's, its first page, the sequence
 * playing); the keys on BASS, into the bass's (SEQUENCE on, BASS SEQ, playing). Recording, STEP or LIVE, RECORD OFF
 * (both), the keys back to what they played (rec_keys), the page staying */
static void rec_tap(void)
{
    if (st.v[P_SQ_REC] || st.v[P_BQ_REC]) {
        static const char *const KB[3] = {"DRONE", "GLINT", "BASS"};
        param_set(P_SQ_REC, 0);
        param_set(P_BQ_REC, 0);
        app_say("RECORD OFF", KB[st.v[P_KB] % 3u]);
        return;
    }
    if (st.bass) {
        param_set(P_BQ_ON, 1);
        param_set(P_BQ_REC, 2);
        if (!bq.run)
            bseq_begin();
        st.logo = 0;
        st.help = 0;
        st.page = PG_BSEQ;
        app_say("RECORD", "LIVE");
        return;
    }
    seq_keep();                          /* (UNDO: the sequence before this take) */
    param_set(P_SQ_TYPE, (int32_t)SQT_SEQ);
    param_set(P_SQ_REC, 2);              /* (the keys GLINT's: rec_keys) */
    if (!sq.run)
        seq_begin();
    st.logo = 0;
    st.help = 0;
    st.page = PG_SEQ;
    app_say("RECORD", "LIVE");
}

/* with nothing playing, the scene again, its three tracks (RESTORE): what RELEASE ALL let go, else the scene's own notes
 * as saved (at power-on, say), and its sequences that are on from their first steps (GLINT's, the bass's; the
 * arpeggio from its notes), the beat beginning there. 0: something plays, or there is nothing to play */
static int scene_again(void)
{
    scene_t sc;
    const scene_t *n = &rst;
    int gs = st.v[P_SQ_TYPE] == SQT_SEQ, bs = st.v[P_BQ_ON];
    if (!rst_valid) {
        scene_get(st.scene, &sc);
        n = &sc;
    }
    if (holding_any() || !(rst_valid || n->nch || n->narp || n->bass >= 0 || gs || bs))
        return 0;
    rst_valid = 0;
    if (grid_top(0)) {
        scene_notes_play(n);             /* (after grid_top: the bass's note played on the top) */
        seqs_top(gs, bs);
    } else {
        scene_notes_play(n);
        if (gs)
            seq_begin();
        if (bs)
            bseq_begin();
    }
    return 1;
}

/* PLAY tapped: the transport (GLINT's sequence and the bass's stop together, or start together from their first step;
 * the arpeggio waits, or goes on from the top: with the bass's sequence, as it does); on SCENES, the scene K1 points
 * at (MORPH, LOAD), playing at once if nothing played. Started, the beat begins there (grid_top), the sequences' first steps at once; following a MIDI
 * clock, on its next step (its beats kept). Stopped, the grid goes on. The sequences say which: running, it stops;
 * not, it starts (the arpeggio playing alone beside them starts again with them). Nothing playing at all (at
 * power-on, after RELEASE ALL): the scene's three tracks, as held (scene_again). Tapped twice quick while anything
 * plays (not on SCENES): RELEASE ALL, as held (the first tap's stop already made) */
static void play_tap(void)
{
    static uint32_t twice;               /* when the last tap came while anything played (0: it did not) */
    int gs = st.v[P_SQ_TYPE] == SQT_SEQ, ga = st.v[P_SQ_TYPE] == SQT_ARP, bs = st.v[P_BQ_ON];
    uint32_t now = plat_ms() | 1u;
    if (st.page != PG_SCENES && twice && now - twice < PLAY_TWICE_MS) {
        twice = 0;
        sound_off();
        grid_top(0);
        app_say("RELEASE ALL", rst_valid ? "HOLD PLAY: RESTORE" : "");
        return;
    }
    twice = st.page != PG_SCENES && holding_any() ? now : 0u;
    if (st.page != PG_SCENES && scene_again()) {   /* nothing playing: its three tracks, as PLAY held */
        app_say("PLAY", "FROM THE TOP");
        return;
    }
    if (st.page == PG_SCENES) {          /* nothing playing: the scene loaded plays at once, as PLAY tapped plays a
                                          * scene from silence (power-on alone stays quiet); its own notes, not those
                                          * a RELEASE ALL kept for RESTORE */
        int idle = !holding_any();
        scene_go(st.pick);
        if (idle) {
            rst_valid = 0;
            scene_again();
        }
    } else if (gs || bs) {
        if ((gs && sq.run) || (bs && bq.run)) {
            seq_note_off();
            sq.run = 0;
            bseq_stop();
            if (ga) {
                arp.paused = 1;
                arp_stop();
            }
            app_say("SEQUENCE", "STOPPED");
        } else {
            if (grid_top(0)) {
                seqs_top(gs, bs);
            } else {
                if (gs)
                    seq_begin();
                if (bs)
                    bseq_begin();
            }
            if (ga) {
                arp.paused = 0;
                arp.dirty = 1;
            }
            app_say("SEQUENCE", "FROM THE TOP");
        }
    } else if (ga) {
        arp.paused ^= 1u;
        arp.dirty = 1;                   /* (back: its pattern from the top, the beat from here) */
        if (arp.paused)
            arp_stop();
        else
            grid_top(0);
        app_say("ARPEGGIO", arp.paused ? "WAITING" : "PLAYING");
    } else {
        app_say("HOLD PLAY", "RELEASE ALL");
    }
}

/* PLAY held: RELEASE ALL (sound_off) while anything plays; with nothing playing, the scene again (scene_again,
 * RESTORED). Either way the beat begins there (grid_top: following a MIDI clock, its beats kept) */
static void play_hold(void)
{
    if (scene_again()) {
        app_say("RESTORED", "");
        return;
    }
    sound_off();
    grid_top(0);
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
    case B_HOME:                         /* its page; the help closes; SOUND, MIXER, the logo, the river (KEYS as they
                                          * are: HOME's pages) */
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
    case B_SAVE:                         /* SCENES, and back where it came from (as it left: KEYS as they are) */
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
            st.hold_acted &= ~(1u << k);
        }
}

static void buttons(uint32_t btn)
{
    uint32_t down = btn & ~btn_prev, up = btn_prev & ~btn, k, now = plat_ms();
    for (k = 0; k < NB; k++)
        if ((up >> k & 1u) && !(st.hold_done >> k & 1u) &&
            !(k == B_PLAY && now - st.hold_at[B_PLAY] >= PLAY_SHOW_MS))   /* (PLAY let go while counting: nothing) */
            tap(k);
    if ((up >> B_OCTDN & 1u) && pv.on) { /* OCT- let go, a preset picked under it: taken now */
        pv.on = 0;
        param_set(pv.p, pv.v);
        st.touched = pv.p;
        st.touched_at = now;
    }
    for (k = 0; k < NB; k++) {
        if (!(down >> k & 1u))
            continue;
        if (k == B_OCTDN)
            pv.oct = st.octave;          /* (to undo, should it be a hand) */
        if (k == B_OCTDN || k == B_OCTUP) {   /* at once; both: back to the start */
            if ((btn >> B_OCTDN & 1u) && (btn >> B_OCTUP & 1u))
                st.octave = OCT_DEF;
            else
                st.octave = (int8_t)clamp(st.octave + (k == B_OCTUP ? 1 : -1), -3, 2);
        } else if (k == B_REC && (btn >> B_SAVE & 1u)) {   /* SAVE held, REC: STORE (SCENES' slot, else the scene's) */
            b_used(B_SAVE);
            b_used(B_REC);
            scene_store();
        } else if (k == B_PLAY && (btn >> B_OCTDN & 1u) && !(btn >> B_OCTUP & 1u)) {   /* OCT- held, PLAY: CUT,
                                                                                       * every sound gone now, the
                                                                                       * cloud's tail and the echoes
                                                                                       * too (RELEASE ALL, then the
                                                                                       * engine emptied: frasil_cut);
                                                                                       * OCT- a hand, its step undone;
                                                                                       * PLAY's tap and hold nothing */
            st.octave = pv.oct;
            b_used(B_OCTDN);
            b_used(B_PLAY);
            sound_off();
            grid_top(0);
            frasil_cut();
            app_say("CUT", "ALL SOUND");
        } else if (k == B_EDIT && (btn >> B_OCTDN & 1u) && !(btn >> B_OCTUP & 1u)) {   /* OCT- held, EDIT: COMPARE,
                                                                                       * the scene as saved, while EDIT
                                                                                       * stays down (OCT- a hand, its
                                                                                       * step undone) */
            st.octave = pv.oct;
            b_used(B_OCTDN);
            b_used(B_EDIT);
            st.hold_acted |= 1u << B_EDIT;   /* (as EDIT's hold was: SELECT turned with it undoes it, hold_undo) */
            st.compare = 1;
            st.engine_dirty = 1;
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
        case B_ARP:                      /* the arpeggio's latch; on, GLINT's SEQ on ARP too, as GLINT I turns it
                                          * (off, SEQ stays) */
            b_used(b);
            arp.latch ^= 1u;
            arp_was.typed = 0;
            if (!arp.latch) {            /* off: only the keys still down go on (the rest kept for hold_undo) */
                memcpy(arp_was.held, arp.held, sizeof arp_was.held);
                memcpy(arp_was.prs, arp.prs, sizeof arp_was.prs);
                arp_was.n = arp.n;
                arp_keep_held();
            } else if (st.v[P_SQ_TYPE] != SQT_ARP) {   /* (what it was, for hold_undo) */
                arp_was.typed = 1;
                arp_was.type = st.v[P_SQ_TYPE];
                arp_was.run = sq.run;
                arp_was.rec = st.v[P_SQ_REC];
                arp_was.page = st.page;
                param_set(P_SQ_TYPE, (int32_t)SQT_ARP);
            }
            app_say("ARP LATCH", !arp.latch ? "OFF" : arp_was.typed ? "ON, SEQ ON ARP" : "ON");
            break;
        case B_FX:                       /* FREEZE, the page staying */
            b_used(b);
            param_set(P_FREEZE, !st.v[P_FREEZE]);
            app_say("FREEZE", st.v[P_FREEZE] ? "ON" : "OFF");
            break;
        case B_REC:                      /* the metronome */
            b_used(b);
            st.metro ^= 1u;
            prefs_later();               /* (while anything sounds, once it no longer does: the flash written stops the
                                          * render a moment, a click) */
            app_say("METRONOME", st.metro ? "ON" : "OFF");
            break;
        case B_ENV:                      /* SWELL, while it is held (swell_update): the drone's, as its page is */
            b_used(b);
            break;
        case B_PLAY:
            b_used(b);
            play_hold();
            break;
        default:                         /* (SEL, SAVE, LFO, GLO: with something else; the rest, none) */
            break;
        }
        if (st.hold_done >> b & 1u)
            st.hold_acted |= 1u << b;
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
    if ((btn >> B_ENV & 1u) && now - st.hold_at[B_ENV] >= HOLD_MS && (st.hold_acted >> B_ENV & 1u))   /* (its hold's:
                                                                                          * none once SELECT took it) */
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
        int b = scale_snap(played, st.v), n = b, sk = seq_keys();   /* (OCTAVE on it in the engine) */
        key[k].kind = K_GLINT;
        key[k].root = (int8_t)(n > 127 ? 127 : n);
        key[k].gbase = (int8_t)b;
        key[k].gsound = sk ? st.v[P_SQ_REC] != 0 : st.v[P_SQ_TYPE] != SQT_ARP;
        if (sk) {
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
        int n = scale_snap(played, st.v);   /* (OCTAVE on it in the engine) */
        key[k].root = (int8_t)n;
        if (bseq_keys()) {               /* its sequence on (with notes, or RECORDing): into it (bseq_key) */
            key[k].kind = K_BSEQ;
            key[k].gbase = (int8_t)scale_snap(played, st.v);
            bseq_key(key[k].gbase, n, vel);
            return;
        }
        key[k].kind = K_BASS;
        if (!st.v[P_KEYS] && bass_note == key[k].root)
            key[k].was_lit = 1;          /* latched already: a tap lets it go */
        else
            bass_live(key[k].root, vel);
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
                bass_live(key[last].root, 100);
            else
                bass_stop();
        } else if (key[k].was_lit && !key[k].pressed_in && plat_ms() - key[k].t < TAP_MS) {
            bass_stop();                 /* latched: a tap on it */
        }
        break;
    case K_BSEQ:
        key[k].kind = K_NONE;            /* (not down any more, for bseq_held) */
        bseq_up(key[k].root);
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
    else if ((key[k].kind == K_BASS || key[k].kind == K_BSEQ) && bass_note == key[k].root)
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
        if (sq.sounding >= 0 && (kk = note_key(sq.sounding, 0)) >= 0)
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
        if (bass_note >= 0 && (kk = note_key(bass_note, 0)) >= 0)
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

#ifdef PLAT_HOST
static void enc_late_in(void);          /* (the tests' detents, come in during a pass: app_host_cmd) */
#endif

static void input(void)
{
    uint32_t btn = plat_buttons(), lb, mb, gb, kb, kd;
    if (pop.on) {                        /* a pop-up up: the buttons wait (the keys and MIDI play on) */
        if (popup_input(btn))
            btn = 0;
    } else if (nm.on && name_input(btn)) {   /* a name being written: the same */
        btn = 0;
    }
#ifdef PLAT_HOST
    enc_late_in();                       /* (the tests: detents come in now, as the device's scan may bring them) */
#endif
    btn_now = btn;
    buttons_down(btn);
    keys_scan();
    arp_run();
    bseq_run();
    pulse_run();                         /* (after the bass's sequence: a NOTE beginning on the division first) */
    delay_sync();
    midi_in();
    pump_sync();                         /* (after the MIDI: the clocks just come in) */
    lfo_run();
    sparkle_run();
    metro_run();
    morph_run();
    {                                    /* SCENES left: its LOAD kept, if it changed (MORPH: CUT at every start) */
        static uint8_t was;
        if (was == PG_SCENES && st.page != PG_SCENES &&
            (set.load != st.v[P_LOAD] || set.magic != SET_MAGIC))
            prefs_later();
        was = st.page;
    }
    if (st.prefs_due && !st.store_hold && (st.prefs_due == 2 || prefs_quiet()))   /* (kept: when nothing sounds) */
        prefs_save();
    if (frasil_pump_gain() < pump_low)
        pump_low = frasil_pump_gain();
    if (!pop.on && !nm.on)               /* (up, the encoders are theirs alone: a detent come in since they read them
                                          * waits for their next read, never the page under them: SCENES' LOAD) */
        encoders();
    buttons(btn);
    holds();
    swell_update(btn);
    if (st.engine_dirty)
        engine_push();
    if (pop.on) {                        /* a pop-up: every light out but SEL's, its one way out (back after) */
        oct_soft = 0;
        lights_set(1u << B_SEL, 0, 0, 0, 0);
        return;
    }
    if (nm.on) {                         /* a name: its answers lit, SEL (store) and FX (cancel); OCT at the mid level */
        oct_soft = 0;
        lights_set(1u << B_SEL | 1u << B_FX, 1u << B_OCTDN | 1u << B_OCTUP, 0, 0, 0);
        return;
    }
    /* the buttons' lights, four levels (lights_set): the function buttons at the mid level, their names readable in the
     * dark; lit, what is chosen or on: the page's button, FX while the cloud is held, SEQ swelling. PLAY in the glow,
     * lit while anything sounds. REC in the glow (brighter, it would read as recording: at power-on, say); while a
     * sequencer records on STEP, lit; LIVE, flashing on the sequence's beats (as the metronome counts them), twice as
     * quick as OCT. OCT- and OCT+ in the glow, a little
     * brighter on the beat (MIDI clock, else TEMPO; app_tick). SAVE lit while the sound differs from the scene's. LFO
     * in the glow while no LFO runs. ARP, off its pages, a flash on each of GLINT's steps; SEQ, off its pages, on each
     * of the bass's sequence's NOTEs */
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
    {
        static uint32_t steps_seen, flash_until;
        if (bq.steps != steps_seen) {
            steps_seen = bq.steps;
            flash_until = plat_ms() + 60u;
        }
        if (PAGES[st.page].button != B_SEQ && (int32_t)(flash_until - plat_ms()) > 0)
            lb |= 1u << B_SEQ;
    }
    if (!st.v[P_SQ_REC] && !st.v[P_BQ_REC])
        gb |= 1u << B_REC;
    else if (st.v[P_SQ_REC] != 2u && st.v[P_BQ_REC] != 2u)   /* STEP: lit, steady */
        lb |= 1u << B_REC;
    else if ((uint32_t)(mtr.at >= 0.0f ? mtr.at : arp.tick) % 12u < 3u)   /* LIVE: the first quarter of each
                                                                          * half beat, the sequence's (stopped: the
                                                                          * grid's) */
        lb |= 1u << B_REC;
    if (st.v[P_FREEZE])
        lb |= 1u << B_FX;
    oct_soft = (uint32_t)arp.tick % 24u < 6u;   /* the first quarter of each beat */
    if (st.swell_btn > 0.0f)
        lb |= 1u << B_ENV;
    if (play_lit())
        lb |= 1u << B_PLAY;
    keys_lights(&kb, &kd);
    lights_set(lb, mb & ~lb, gb & ~(lb | mb), kb, kd);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------- the screen --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint8_t red;                         /* its second line bold and bright red (say_red) */
} say;

static int text_is(const char *a, const char *b)
{
    uint32_t i;
    if (!a || !b)
        return 0;
    for (i = 0; a[i] && a[i] == b[i]; i++)
        ;
    return a[i] == b[i];
}

/* a gesture's words, in the header a moment; a restore's result (fm1-base's, main_fm1.c), a pop-up: the player's eyes
 * may be on the computer, it waits for HOME */
void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    if (text_is(a, "STOP FIRST")) {      /* (the River lets it all go first, CC 120: what is left, a key held down;
                                          * from a terminal, anything) */
        popup(POP_WARN, "RESTORE FROM USB", "STOP FIRST", "SOMETHING IS PLAYING: LET GO",
              "OF THE KEYS, OR HOLD PLAY TO", "RELEASE ALL, THEN TRY AGAIN.");
        return;
    }
    if (text_is(a, "RESTORE FROM USB")) {
        if (text_is(b, "RESTORED"))
            popup(POP_DONE, "RESTORE FROM USB", "RESTORED", "THE SCENES AND THE SETTINGS", "ARE BACK.", "");
        else if (text_is(b, "NOTHING CHANGED"))
            popup(POP_WARN, "RESTORE FROM USB", "NOT RESTORED", "FX, OR NO ANSWER IN A MINUTE.", "NOTHING CHANGED.", "");
        else
            popup(POP_WARN, "RESTORE FROM USB", "NOT RESTORED", "THE FILE DID NOT ARRIVE.", "NOTHING CHANGED.",
                  "TRY AGAIN.");
        return;
    }
    for (i = 0; i < 23u && a && a[i]; i++)
        say.msg[0][i] = a[i];
    say.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        say.msg[1][i] = b[i];
    say.msg[1][i] = 0;
    say.msg_until = plat_ms() + 1300u;
    say.red = 0;
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

static void say_clear(void) { say.msg_until = 0; }   /* the words gone (a hold undone: hold_undo) */
static void say_red(void) { say.red = 1; }

#include "ui.c"

/* the buttons down now taken as used: their release no tap (a restore's question answered with SEL or HOME, outside
 * the app's loop: SEL let go would open HARMONY) */
static void buttons_spent(void)
{
    btn_prev = btn_now = plat_buttons();
    st.hold_done = ~0u;
}

void app_ui_init(void)
{
    buttons_spent();
    touch_on(1);                             /* velocity and pressure: the keys' readings on */
    if (st.booting) {                        /* at power-on, the logo a moment, alive, then the river; again (after a
                                              * restore from USB was tried), the screen as it was */
        st.booting = 0;
        st.logo = 1;
        st.logo_until = plat_ms() + 4000u;
    }
    ui_init();
}

/* ------------------------------------------------------- PERFORM over USB --- */
/* Perform (perform.frasilsynth.com): a page on the computer shows the play as it goes, very large (a projector on a
 * stage, a film). The page asks and the FM-1 tells; nothing is built or sent while no page listens (a DAW on the port
 * never sees a byte), and never from the render: the main loop, once a UI frame, from what the screen knows already.
 * SysEx over USB-MIDI, F0 7D 46 50 cmd .. F7 ("FP"; design/perform/README.md):
 *   page -> FM-1  1 HELLO ver     listen (again every ~2 s: a page not heard for 5 s is gone; each one has SCENE and
 *                                 PAGE told again, a frame the page may have missed)
 *                 2 BYE           stop
 *   FM-1 -> page  1 HELLO ver, NPARAM (2), the version (ASCII)            the answer to each HELLO
 *                 2 SCENE slot (16: NEW SHARD), dirty, name (16 ASCII)     at HELLO, and when one of them changes
 *                 3 VALUES first (2), n, then n values (2 each)            every setting: at HELLO, after a load
 *                 4 VALUE p (2), v (2), enc (EN_*, 127: no knob's)          a setting that moved
 *                 5 PAGE page, octave + 3, keys (0 DRONE, 1 GLINT, 2 BASS)  when one of them changes
 *                 6 FRAME 20 a second, what sounds: the drones' 8 (note, level), GLINT's 4 (note, level as heard,
 *                   1/20 s since struck), the bass (note, level), the LFOs on (bits), then each LFO where in its turn
 *                   (0..127), its value ((-1..1) to 0..127) and its turns (3 bytes: RANDOM's and DRIFT's values, the
 *                   page's lfo_rnd as app.c's), then the tracks' meters (drones, bass, glints, the cloud); a note 127:
 *                   none; levels 0..127
 *                 7 BUTTON b (B_*), down
 * Two bytes: 7-bit high, low. Each frame goes whole or not at all, never waiting (plat_sysex_try): one that does not
 * fit now (the computer not reading, a backup or the screen reader on the wire) is tried again at the next UI frame.
 * A backup streaming (app_backup_hold): quiet. */
#ifndef PLAT_WEB
#define PF_VER 1u
#define PF_DUMP 48u                      /* settings in one VALUES (a SysEx ring's third) */
static struct {
    uint32_t heard, frame_at, btn, kd;   /* (kd: the keys held, as the page knows them) */
    uint8_t on, page, oct, keys, scene, dirty, dump;   /* dump: the next VALUES' first setting + 1 (0: none due) */
    uint8_t v[NPARAM];                   /* each setting as the page knows it */
} pf;

/* COMPUTER AUDIO on the page (HELLO's flags, bit 0), or the page gone (BYE, not heard for 5 s): the FM-1's own output
 * quiet, or as SPK/JACK says again */
static void pf_quiet(int q)
{
    if ((q != 0) == spk_page)
        return;
    spk_page = (uint8_t)(q != 0);
    plat_speaker(st.v[P_SPEAKER] && !spk_page);
}

static int pf_send(const uint8_t *b, uint32_t n)   /* F0 7D 46 50 b.. F7, whole or not at all: 0 = sent */
{
    uint8_t f[4u + 2u * PF_DUMP + 8u];
    uint32_t i;
    if (n + 5u > sizeof f)
        return -1;
    f[0] = 0xF0u, f[1] = 0x7Du, f[2] = 0x46u, f[3] = 0x50u;
    for (i = 0; i < n; i++)
        f[4u + i] = b[i] & 0x7Fu;
    f[4u + n] = 0xF7u;
    return plat_sysex_try(f, n + 5u);
}

static uint32_t pf_two(uint8_t *b, uint32_t v)
{
    b[0] = (uint8_t)(v >> 7 & 0x7Fu);
    b[1] = (uint8_t)(v & 0x7Fu);
    return 2u;
}

static uint8_t pf_note(int n) { return n < 0 || n > 126 ? 127u : (uint8_t)n; }
static uint8_t pf_lvl(float l) { return (uint8_t)(br_clamp01(l) * 127.0f + 0.5f); }

static int pf_scene(void)
{
    scene_t sc;
    char t[SC_NAME + 1];
    uint8_t b[3u + SC_NAME];
    uint32_t i;
    scene_get(st.scene, &sc);
    name_text(&sc, t);
    b[0] = 2u, b[1] = st.scene, b[2] = st.dirty != 0;
    for (i = 0; i < SC_NAME; i++)
        b[3u + i] = (uint8_t)(t[i] && t[i] < 0x7F ? t[i] : ' ');
    for (i = 0; i < SC_NAME && t[i]; i++)
        ;
    if (pf_send(b, 3u + i))
        return -1;
    pf.scene = st.scene, pf.dirty = st.dirty != 0;
    return 0;
}

/* the transport, each track playing (bits): 1 the drones (a chord latched or held, from the keys or MIDI), 2 GLINT
 * (its arpeggio going, not waiting; its sequence running), 4 the bass (its note held or latched; its sequence running) */
static uint8_t pf_playing(void)
{
    uint32_t k, dr = 0;
    int gl = (st.v[P_SQ_TYPE] == SQT_ARP && arp.n && !arp.paused) || (st.v[P_SQ_TYPE] == SQT_SEQ && sq.run);
    int bs = bass_note >= 0 || mb_n || (st.v[P_BQ_ON] && bq.run);
    for (k = 0; k < 4u; k++)
        dr |= lit[k] | mid_on[k] | sus[k];
    return (uint8_t)((dr != 0) | (gl != 0) << 1 | (bs != 0) << 2);
}

static void pf_frame(void)
{
    frasil_view_t v[FR_NV];
    glint_view_t g[GLINT_NV];
    bass_view_t bv;
    uint8_t b[1u + 2u * FR_NV + 3u * GLINT_NV + 3u + 5u * NLFO + 4u + 1u], *p = b;
    uint32_t i, on = 0;
    float m[4];
    frasil_view(v);
    frasil_glint_view(g);
    frasil_bass_view(&bv);
    *p++ = 6u;
    for (i = 0; i < FR_NV; i++) {
        *p++ = pf_note(v[i].level > 0.0f ? v[i].note : -1);
        *p++ = v[i].note < 0 ? 0u : pf_lvl(v[i].level);
    }
    for (i = 0; i < GLINT_NV; i++) {
        float l = g[i].level;
        *p++ = pf_note(l < 0.004f ? -1 : g[i].note);
        *p++ = g[i].note < 0 || l < 0.004f ? 0u : pf_lvl(1.0f + fm_log2f(l) * (1.0f / 8.0f));
        *p++ = (uint8_t)(g[i].t * 20.0f > 127.0f ? 127u : (uint32_t)(g[i].t * 20.0f));
    }
    *p++ = pf_note(bv.level > 0.0f ? bv.note : -1);
    *p++ = bv.note < 0 ? 0u : pf_lvl(bv.level);
    for (i = 0; i < NLFO; i++)
        on |= (uint32_t)(lfo[i].on != 0) << i;
    *p++ = (uint8_t)on;
    for (i = 0; i < NLFO; i++) {
        *p++ = pf_lvl(lfo[i].ph);
        *p++ = pf_lvl(0.5f + 0.5f * lfo[i].out);
        *p++ = (uint8_t)(lfo[i].cyc >> 14 & 0x7Fu);
        *p++ = (uint8_t)(lfo[i].cyc >> 7 & 0x7Fu);
        *p++ = (uint8_t)(lfo[i].cyc & 0x7Fu);
    }
    frasil_meters(m);
    for (i = 0; i < 4u; i++)
        *p++ = pf_lvl(m[i]);
    *p++ = pf_playing();
    pf_send(b, (uint32_t)(p - b));
}

/* a UI frame: what changed since the page last heard, then what sounds (20 a second) */
static void pf_tick(void)
{
    uint32_t now = plat_ms(), i, n, sent = 0, btn, k;
    uint8_t b[4u + 2u * PF_DUMP], keys = st.glint ? 1u : st.bass ? 2u : 0u;
    if (!pf.on)
        return;
    if (now - pf.heard > 5000u) {
        pf.on = 0;
        pf_quiet(0);
        return;
    }
    if (st.store_hold)                   /* a backup streaming: its frames alone on the wire */
        return;
    if ((st.scene != pf.scene || (st.dirty != 0) != pf.dirty) && pf_scene())
        return;                          /* (no room now: all of it at the next frame) */
    if (st.page != pf.page || st.octave + 3 != pf.oct || keys != pf.keys) {
        b[0] = 5u, b[1] = st.page, b[2] = (uint8_t)(st.octave + 3), b[3] = keys;
        if (pf_send(b, 4u))
            return;
        pf.page = st.page, pf.oct = (uint8_t)(st.octave + 3), pf.keys = keys;
    }
    for (i = n = 0; i < NPARAM; i++)
        n += st.v[i] != pf.v[i];
    if (n > 24u && !pf.dump)             /* a scene loaded: every setting again */
        pf.dump = 1u;
    if (pf.dump) {
        uint32_t first = pf.dump - 1u, cnt = NPARAM - first < PF_DUMP ? NPARAM - first : PF_DUMP;
        b[0] = 3u;
        pf_two(b + 1, first);
        b[3] = (uint8_t)cnt;
        for (i = 0; i < cnt; i++)
            pf_two(b + 4u + 2u * i, st.v[first + i]);
        if (pf_send(b, 4u + 2u * cnt))
            return;
        for (i = 0; i < cnt; i++)
            pf.v[first + i] = st.v[first + i];
        pf.dump = first + cnt >= NPARAM ? 0u : (uint8_t)(first + cnt + 1u);
    } else {
        for (i = 0; i < NPARAM && sent < 8u; i++) {
            if (st.v[i] == pf.v[i])
                continue;
            b[0] = 4u;
            pf_two(b + 1, i);
            pf_two(b + 3, st.v[i]);
            b[5] = i == st.touched && touched_now() ? st.touched_enc : 127u;
            if (pf_send(b, 6u))
                return;
            pf.v[i] = st.v[i];
            sent++;
        }
    }
    btn = plat_buttons();
    for (k = 0; k < NB; k++)
        if ((btn ^ pf.btn) >> k & 1u) {
            b[0] = 7u, b[1] = (uint8_t)k, b[2] = (uint8_t)(btn >> k & 1u);
            if (pf_send(b, 3u))
                return;
            pf.btn ^= 1u << k;
        }
    if (keys_prev != pf.kd) {            /* the hand on THE KEYS: the keys held (bit k: key k, F3 up), in four 7-bit bytes */
        b[0] = 8u;
        for (k = 0; k < 4u; k++)
            b[1u + k] = (uint8_t)(keys_prev >> (7u * k) & 0x7Fu);
        if (pf_send(b, 5u))
            return;
        pf.kd = keys_prev;
    }
    if (now - pf.frame_at >= 50u) {
        pf.frame_at = now;
        pf_frame();
    }
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    static const char ver[] = APP_VERSION;
    uint8_t b[4u + sizeof ver];
    uint32_t i;
    if (n < 4u || p[0] != 0x7Du || p[1] != 0x46u || p[2] != 0x50u)
        return 0;
    if (p[3] == 2u) {
        pf.on = 0;
        pf_quiet(0);
        return 1;
    }
    if (p[3] != 1u)
        return 1;
    pf_quiet(n > 5u && p[5] < 0x80u && (p[5] & 1u));   /* (HELLO: version, then flags: 1 COMPUTER AUDIO) */
    if (!pf.on) {                        /* a page arrives: everything, from the start */
        pf.on = 1;
        pf.scene = 0xFFu, pf.page = 0xFFu, pf.dump = 1u;
        pf.btn = plat_buttons();
        pf.kd = keys_prev;
        pf.frame_at = plat_ms();
    }
    pf.scene = 0xFFu, pf.page = 0xFFu;   /* (told again: one the page missed comes back within 2 s) */
    pf.heard = plat_ms();
    if (st.store_hold)                   /* a backup streaming: heard, not answered */
        return 1;
    b[0] = 1u, b[1] = PF_VER;
    pf_two(b + 2, NPARAM);
    for (i = 0; ver[i]; i++)
        b[4u + i] = (uint8_t)ver[i];
    pf_send(b, 4u + i);
    return 1;
}
#else
static void pf_tick(void) {}
#endif

void app_frame(void)
{
    input();
    load_watch();
    ui_frame();
    pf_tick();
}

/* --------------------------------------------- backup and restore over USB --- */
/* (device/backup.c, fm1.toml backup = true; app.h's APP_BACKUP_*). Playing: a restore waits (STOP FIRST). Anything
 * latched or held, a key down, a note from MIDI; GLINT's sequence or the bass's running with a note in it */
int app_backup_playing(void)
{
    uint32_t k, midi = mb_n, notes = 0, bnotes = 0;
    for (k = 0; k < 4u; k++)
        midi |= mid_on[k] | mgl[k];
    for (k = 0; k < SC_SEQ; k++)
        notes += (seq[k] & 127u) != SQ_REST && (seq[k] & 127u) != SQ_TIE;
    for (k = 0; k < SC_BSEQ; k++)
        bnotes += (bnote[k] & 127u) != SQ_REST && (bnote[k] & 127u) != SQ_TIE;
    return lit[0] | lit[1] | lit[2] | lit[3] || bass_note >= 0 || arp.n || (sq.run && notes) || (bq.run && bnotes) ||
           keys_prev || midi;
}

/* a backup starts, nothing playing: a record the store still has in an earlier layout saved in this one (as boot made
 * it over: the backup's scenes can go back one by one), SCENES' LOAD kept if it changed (else when SCENES is left). The sound as it is now is not stored: STORE is the player's */
void app_backup_flush(void)
{
    if ((store_old & 1u) && scenes_save() == 0)
        store_old &= ~1u;
    if (set.magic == SET_MAGIC &&
        (store_old & 2u || st.prefs_due || set.load != st.v[P_LOAD]))
        prefs_save();
}

void app_backup_hold(int on) { st.store_hold = (uint8_t)(on != 0); }

/* restored (SEL said yes; nothing was playing): every note let go, then everything as at power-on */
void app_backup_restored(void)
{
    release_all();
    arp.n = arp.pn = 0;
    arp.dirty = arp.paused = 0;
    arp_stop();
    seq_note_off();
    sq.run = 0;
    bseq_stop();
    state_load();
    buttons_spent();
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
    else if (!strcmp(what, "bassnote"))      /* the bass's note held, as it sounds (OCTAVE on it), -1 none */
        *got = bass_note < 0 ? -1 : bass_sounding(bass_note);
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
    } else if (!strcmp(what, "pulses")) {    /* PULSE: the notes let go and played again since last asked (pulse_run);
                                              * bpulses: those the bass played again after the gap, since last asked;
                                              * pulsebeat: the clocks into its beat the last fell on */
        *got = (int)bp.n;
        bp.n = 0;
    } else if (!strcmp(what, "pulselate")) {   /* how late the last strike fell after its division, in ms */
        *got = (int)bp.late;
    } else if (!strcmp(what, "bons")) {     /* the notes the bass struck since last asked (bass_on: not PULSE's) */
        static uint32_t was;
        bass_view_t bv;
        frasil_bass_view(&bv);
        *got = (int)(bv.ons - was);
        was = bv.ons;
    } else if (!strcmp(what, "bpulses")) {
        static uint32_t was;
        bass_view_t bv;
        frasil_bass_view(&bv);
        *got = (int)(bv.pulses - was);
        was = bv.pulses;
    } else if (!strcmp(what, "grid")) {     /* the grid now, MIDI clocks from its top (arp.tick); clickgrid: where the
                                              * metronome's last tick fell on it */
        *got = (int)arp.tick;
    } else if (!strcmp(what, "clickgrid")) {
        *got = (int)mtr.ck;
    } else if (!strcmp(what, "pulsebeat")) {
        *got = (int)bp.at % 24;
    } else if (!strcmp(what, "bqbeat")) {    /* the clocks into its beat the bass's last NOTE began (0: on it) */
        *got = (int)fm_floorf(bq_beat + 0.5f) % 24;
    } else if (!strcmp(what, "recled") || !strcmp(what, "fxled") || !strcmp(what, "octled") ||
               !strcmp(what, "saveled") || !strcmp(what, "playled") || !strcmp(what, "homeled") || !strcmp(what, "selled") ||
               !strcmp(what, "envled")) {
        uint32_t b = what[0] == 'r' ? B_REC : what[0] == 'f' ? B_FX : what[0] == 'e' ? B_ENV : what[1] == 'e' ? B_SEL
                   : what[0] == 's' ? B_SAVE
                   : what[0] == 'p' ? B_PLAY : what[0] == 'h' ? B_HOME : B_OCTUP;
        /* 0 off, 1 glow, 2 mid (OCT: its softer blink), 3 lit */
        *got = (leds_now >> b & 1u) ? 3 : (midleds_now >> b & 1u) ? 2 : b == B_OCTUP && oct_soft ? 2
             : (glowleds_now >> b & 1u) ? 1 : 0;
    } else if (!strcmp(what, "clicks")) {    /* the metronome's ticks; hiclicks: the higher ones; metro: it on */
        *got = (int)st.clicks;
    } else if (!strcmp(what, "hiclicks")) {
        *got = (int)st.hiclicks;
    } else if (!strcmp(what, "metro")) {
        *got = st.metro;
    } else if (!strcmp(what, "extclock")) {  /* a MIDI clock leading the grid now (one came in this half second) */
        *got = plat_ms() - arp.clk_at < 500u;
    } else if (!strcmp(what, "prefsdue")) {  /* the settings asked to be kept, not yet (something sounding) */
        *got = st.prefs_due;
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
    else if (!strcmp(what, "overlay"))   /* a value shown large over the strip */
        *got = overlay_on();
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
    else if (!strcmp(what, "seqnote"))   /* the note its step sounds (OCTAVE on it), -1: none */
        *got = sq.sounding < 0 ? -1 : sq.sounding + 12 * (int)st.v[P_GL_OCTAVE];
    else if (!strcmp(what, "seqsteps"))  /* the notes it has played */
        *got = (int)sq.steps;
    else if (!strcmp(what, "seqnotes")) {   /* its steps that hold a note */
        uint32_t i;
        *got = 0;
        for (i = 0; i < SC_SEQ; i++)
            *got += (seq[i] & 127u) != SQ_REST && (seq[i] & 127u) != SQ_TIE;
    } else if (!strncmp(what, "seqstep", 7))   /* seqstepN: step N's byte (its note, + 128 with ACCENT) */
        *got = seq[atoi(what + 7) % SC_SEQ];
    else if (!strcmp(what, "bseqrun"))   /* the bass's sequence playing; bseqpos: its step now, -1 none */
        *got = bq.run;
    else if (!strcmp(what, "bseqpos"))
        *got = bq.pos;
    else if (!strcmp(what, "bseqsteps")) /* the NOTEs it has played */
        *got = (int)bq.steps;
    else if (!strncmp(what, "bseq", 4))  /* bseqN: step N's note (0 CUT, 127 HOLD); blenN: its length (1/16 .. 4 BARS:
                                          * 0 .. 10) */
        *got = bnote[atoi(what + 4) % SC_BSEQ];
    else if (!strncmp(what, "blen", 4))
        *got = blen[atoi(what + 4) % SC_BSEQ];
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
    else if (!strcmp(what, "playing"))   /* a restore would wait (app_backup_playing: STOP FIRST) */
        *got = app_backup_playing();
    else if (!strcmp(what, "storeold"))  /* the records the store has in an earlier layout (bit 0 scenes, 1 settings) */
        *got = store_old;
    else if (!strcmp(what, "keyleds"))   /* the keys' lights last set: bit k = key k (lit or in the glow) */
        *got = (int)keyleds_now;
    else if (!strcmp(what, "naming"))    /* the name editor up (after STORE); namecur: its cursor */
        *got = nm.on;
    else if (!strcmp(what, "namecur"))
        *got = nm.cur;
    else if (!strncmp(what, "scnchar", 7))   /* scncharN: the current scene's name, its character N (0: none) */
        *got = (st.scene < NSCENES ? scn.s[st.scene].name : NEW_SHARD.name)[atoi(what + 7) % SC_NAME];
    else if (!strcmp(what, "pick"))      /* SCENES: the slot K1 points at (16: NEW SHARD) */
        *got = st.pick;
    else if (!strcmp(what, "scnacts"))   /* the words SCENES' box showed last (PLAY LOAD, SAVE + REC STORE: 4) */
        *got = ui.acts;
    else if (!strcmp(what, "setscene"))  /* the settings' scene byte, as last saved (16: NEW SHARD) */
        *got = set.scene;
    else if (!strcmp(what, "size_scene"))   /* the records' layouts, as schema_write has them: a scene (a shard) */
        *got = (int)sizeof(scene_t);
    else if (!strcmp(what, "size_scenes"))  /* OBJ_SCENES' record, OBJ_BSEQ's, the settings' */
        *got = (int)SCN_BYTES;
    else if (!strcmp(what, "size_bseqs"))
        *got = (int)BSQ_BYTES;
    else if (!strcmp(what, "size_settings"))
        *got = (int)sizeof(settings_t);
    else if (!strcmp(what, "at_scene"))     /* the settings' scene byte: where */
        *got = (int)offsetof(settings_t, scene);
    else if (!strcmp(what, "popup"))     /* a pop-up up: 1 a warning, 2 done */
        *got = pop.on ? 1 + pop.kind : 0;
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
    else if (!strcmp(what, "cloudlvl") || !strcmp(what, "dronelvl")) {   /* the cloud's (the drones') recent peak x 1000 */
        float m[4];
        frasil_meters(m);
        *got = (int)(m[what[0] == 'c' ? 3 : 0] * 1000.0f);
    }
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

/* ------------------------------------------------- the backup page's schema --- */
/* "schema FILE": what the backup page reads the records with (tools/website.sh writes it; website/assemble.py checks
 * it and puts it in the page): the records' layouts (and the earlier ones'), every setting with the screen's words for
 * each of its values, the pages, the names, the factory scenes. From the code itself, never copied by hand */
static void js_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\')
            fputc('\\', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

static void js_strs(FILE *f, const char *const *s, uint32_t n)
{
    uint32_t i;
    fputc('[', f);
    for (i = 0; i < n; i++) {
        if (i)
            fputc(',', f);
        js_str(f, s[i]);
    }
    fputc(']', f);
}

static void js_magic(FILE *f, uint32_t m)
{
    char t[5] = {(char)m, (char)(m >> 8), (char)(m >> 16), (char)(m >> 24), 0};
    js_str(f, t);
}

static void js_layouts(FILE *f, const layout_t *l, uint32_t n, int scenes)   /* (scenes: their names' length) */
{
    uint32_t i;
    fputc('[', f);
    for (i = 0; i < n; i++) {
        fprintf(f, "%s{\"magic\":", i ? "," : "");
        js_magic(f, l[i].magic);
        fprintf(f, ",\"nv\":%u,\"extra\":%u", l[i].nv, l[i].extra);
        if (scenes)                      /* (seq: GLINT's sequence in its record; FRSo's in OBJ_BSEQ) */
            fprintf(f, ",\"name\":%u,\"seq\":%u", name_bytes(&l[i]), seq_bytes(&l[i]) ? 1u : 0u);
        fputc('}', f);
    }
    fputc(']', f);
}

static int schema_write(const char *path)
{
    static const char *const BUTTON[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                           "PLAY", "REC", "OCT-", "OCT+"};
    FILE *f = fopen(path, "w");
    uint32_t p, x, i, k;
    char t[32];
    if (!f)
        return -1;
    seq_shift = bseq_shift = 0;          /* (NOTE's words: the step's own note) */
    fprintf(f, "{\"format\":\"frasil-schema\",\"version\":1,\n\"sizes\":{\"NSOUND\":%u,\"NPARAM\":%u,\"NSCENES\":%u,"
               "\"SC_CHORDS\":%u,\"SC_ARP\":%u,\"SC_SEQ\":%u,\"SC_BSEQ\":%u,\"NFACTORY\":%u,\"NLEARN\":%u},\n",
            (unsigned)NSOUND, (unsigned)NPARAM, NSCENES, SC_CHORDS, SC_ARP, SC_SEQ, SC_BSEQ, (unsigned)NFACTORY, NLEARN);
    /* a scene (a shard) whole, its bass's sequence last; "rec": its bytes in the scenes' record (OBJ_SCENES), the rest
     * in OBJ_BSEQ's (bseqs_t: after its magic, each scene's bnote then blen) */
    fprintf(f, "\"scene_t\":{\"size\":%u,\"name\":%u,\"name_len\":%u,\"v\":%u,\"nch\":%u,\"narp\":%u,\"ch\":%u,\"arp\":%u,\"bass\":%u,"
               "\"seq\":%u,\"bnote\":%u,\"blen\":%u,\"bq\":%u,\"tempo\":%u,\"rec\":%u},\n",
            (unsigned)sizeof(scene_t), (unsigned)offsetof(scene_t, name), SC_NAME, (unsigned)offsetof(scene_t, v),
            (unsigned)offsetof(scene_t, nch), (unsigned)offsetof(scene_t, narp), (unsigned)offsetof(scene_t, ch),
            (unsigned)offsetof(scene_t, arp), (unsigned)offsetof(scene_t, bass), (unsigned)offsetof(scene_t, seq),
            (unsigned)offsetof(scene_t, bnote), (unsigned)offsetof(scene_t, blen), (unsigned)offsetof(scene_t, bq_on),
            (unsigned)offsetof(scene_t, tempo), (unsigned)SC_REC);
    fprintf(f, "\"scenes_t\":{\"size\":%u,\"magic\":%u,\"used\":%u,\"s\":%u,\"rec\":%u},\n", (unsigned)sizeof(scenes_t),
            (unsigned)offsetof(scenes_t, magic), (unsigned)offsetof(scenes_t, used), (unsigned)offsetof(scenes_t, s),
            (unsigned)SCN_BYTES);
    fprintf(f, "\"bseqs_t\":{\"size\":%u,\"magic\":0,\"s\":4,\"scene\":%u},\n", (unsigned)BSQ_BYTES, (unsigned)BSQ_SCENE);
    fprintf(f, "\"settings_t\":{\"size\":%u,\"magic\":%u,\"v\":%u,\"octave\":%u,\"keys\":%u,\"channel\":%u,"
               "\"scene\":%u,\"tempo\":%u,\"ch_glint\":%u,\"ch_bass\":%u,\"seq\":%u,\"metro_off\":%u,\"speaker_off\":%u,"
               "\"load\":%u,\"learn_cc\":%u,\"learn_p\":%u,\"bnote\":%u,\"blen\":%u,\"bq_on\":%u,\"bq_steps\":%u,"
               "\"bq_trans\":%u,\"anim\":%u,\"clock\":%u},\n",
            (unsigned)sizeof(settings_t), (unsigned)offsetof(settings_t, magic), (unsigned)offsetof(settings_t, v),
            (unsigned)offsetof(settings_t, octave), (unsigned)offsetof(settings_t, keys),
            (unsigned)offsetof(settings_t, channel), (unsigned)offsetof(settings_t, scene),
            (unsigned)offsetof(settings_t, tempo), (unsigned)offsetof(settings_t, ch_glint),
            (unsigned)offsetof(settings_t, ch_bass), (unsigned)offsetof(settings_t, seq),
            (unsigned)offsetof(settings_t, metro_off), (unsigned)offsetof(settings_t, speaker_off),
            (unsigned)offsetof(settings_t, load), (unsigned)offsetof(settings_t, learn_cc),
            (unsigned)offsetof(settings_t, learn_p), (unsigned)offsetof(settings_t, bnote),
            (unsigned)offsetof(settings_t, blen), (unsigned)offsetof(settings_t, bq_on),
            (unsigned)offsetof(settings_t, bq_steps), (unsigned)offsetof(settings_t, bq_trans),
            (unsigned)offsetof(settings_t, anim), (unsigned)offsetof(settings_t, clock));
    fprintf(f, "\"magic\":{\"scenes\":");
    js_magic(f, SCN_MAGIC);
    fprintf(f, ",\"bseqs\":");
    js_magic(f, BSQ_MAGIC);
    fprintf(f, ",\"shard\":");
    js_magic(f, SHARD_MAGIC);
    fprintf(f, ",\"settings\":");
    js_magic(f, SET_MAGIC);
    fprintf(f, ",\"backup\":");
    js_magic(f, APP_BACKUP_MAGIC);
    fprintf(f, "},\n\"objs\":{\"settings\":%d,\"panel\":%d,\"scenes\":%d,\"bseqs\":%d},\n\"seq\":{\"rest\":%u,\"tie\":%u,"
               "\"accent\":%u},\n",
            OBJ_SETTINGS, OBJ_PANEL, OBJ_SCENES, OBJ_BSEQ, SQ_REST, SQ_TIE, SQ_ACC);
    fprintf(f, "\"bseq\":{\"cut\":%u,\"hold\":%u,\"ticks\":[", SQ_REST, SQ_TIE);   /* (a STEP LENGTH's, MIDI clocks) */
    for (i = 0; i < NBQLEN; i++)
        fprintf(f, "%s%u", i ? "," : "", BQ_TICKS[i]);
    /* a shard of an earlier layout: its name and NV sounds, then the rest of its scene up to the bass's sequence (to
     * REC: GLINT's sequence in it), then its bass's sequence (SIZE - REC bytes, from bnote on; none in FRSh's: an empty
     * one's, TEMPO at its default); SCATTER where GLINT's RELEASE is, made over (scatter_gone): FRSn, FRSm, FRSk, FRSi,
     * then FRSh */
    fprintf(f, "]},\n\"old_shards\":[{\"magic\":");
    js_magic(f, SHARD_FRSP);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0},{\"magic\":",
            (unsigned)(SC_NAME + FRSO_NV + SC_TAIL + FRBT_SCENE), FRSO_NV, (unsigned)(SC_NAME + FRSO_NV + SC_TAIL));
    js_magic(f, SHARD_FRSN);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0},{\"magic\":", (unsigned)(FRSL_REC + FRBT_SCENE),
            FRSL_NV, (unsigned)FRSL_REC);
    js_magic(f, SHARD_FRSM);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0},{\"magic\":", (unsigned)(FRSL_REC + FRBS_SCENE),
            FRSL_NV, (unsigned)FRSL_REC);
    js_magic(f, SHARD_FRSK);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":1},{\"magic\":", (unsigned)(FRSL_REC + FRBS_SCENE),
            FRSL_NV, (unsigned)FRSL_REC);
    js_magic(f, SHARD_FRSI);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":1},{\"magic\":", (unsigned)(FRSH_REC + FRBS_SCENE),
            FRSH_NV, (unsigned)FRSH_REC);
    js_magic(f, SCN_FRSH);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":1}],\n\"old_bseqs\":[{\"magic\":", (unsigned)FRSH_REC,
            FRSH_NV, (unsigned)FRSH_REC);
    js_magic(f, BSQ_FRBT);               /* (the basses' record with FRSl's scenes: a scene's FRBT_SCENE bytes, from its
                                          * bass's notes on: at that offset in this one's) */
    fprintf(f, ",\"scene\":%u,\"at\":%u},{\"magic\":", (unsigned)FRBT_SCENE, (unsigned)SC_SEQ);
    js_magic(f, BSQ_FRBS);               /* (the one before the scenes' TEMPO: a scene's FRBS_SCENE bytes) */
    fprintf(f, ",\"scene\":%u,\"at\":%u}],\n\"old_scenes\":", (unsigned)FRBS_SCENE, (unsigned)SC_SEQ);
    js_layouts(f, OLD_SCN, sizeof OLD_SCN / sizeof OLD_SCN[0], 1);
    fprintf(f, ",\n\"old_settings\":");
    js_layouts(f, OLD_SET, sizeof OLD_SET / sizeof OLD_SET[0], 0);
    /* the settings settings_over makes over byte by byte: this layout with NV sounds (a setting learned after them
     * numbered so), SIZE bytes, those after the sound to END as this one's (the bass's sequence: none, empty): FRSI,
     * FRSH, FRSG */
    fprintf(f, ",\n\"old_settings_over\":[{\"magic\":");
    js_magic(f, SET_FRSK);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":0,\"speaker\":1},{\"magic\":", FRSK_BYTES,
            FRSO_NV, (unsigned)(4u + FRSO_NV + offsetof(settings_t, anim) - offsetof(settings_t, octave)));
            /* (speaker: its byte SPK/JACK's already; the others', MORPH's) */
    js_magic(f, SET_FRSJ);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":0},{\"magic\":", FRSJ_BYTES, FRSL_NV, FRSJ_BYTES);
    js_magic(f, SET_FRSI);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":1},{\"magic\":", FRSJ_BYTES, FRSL_NV, FRSJ_BYTES);
    js_magic(f, SET_FRSH);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":1},{\"magic\":", FRSH_BYTES, FRSH_NV, FRSH_BYTES);
    js_magic(f, SET_FRSG);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":1}]", FRSG_BYTES, FRSH_NV, (unsigned)FRSH_BSEQ);
    /* the earlier layouts that kept SCATTER where GLINT's RELEASE is ("scatter" above; every OLD_SCN): made over,
     * RELEASE as its DECAY let a glint go then (sound_over: OF[DECAY]), a CC learned on it learned no more */
    fprintf(f, ",\n\"scatter_gone\":{\"p\":%d,\"decay\":%d,\"of\":[", (int)P_GL_RELEASE, (int)P_GL_DECAY);
    for (i = 0; i < 128u; i++)
        fprintf(f, "%s%u", i ? "," : "", glint_release_of(i));
    fprintf(f, "]}");
    /* settings whose earlier values read modulo a number now (any layout, as later_defaults has them): GLINT DELAY's
     * TIME, 12 .. 23 its ping-pong half till 2.5 */
    fprintf(f, ",\n\"wrap\":[[%d,12]]", (int)P_GD_TIME);
#define ID(n) fprintf(f, "%s\"" #n "\":%d", k++ ? "," : "", (int)(n))
    fprintf(f, ",\n\"ids\":{");
    k = 0;
    ID(P_ROOT), ID(P_SCALE), ID(P_CHORD), ID(P_VOICING), ID(P_DR_PRESET), ID(P_DR_FULL), ID(P_GL_PRESET);
    ID(P_GL_OCTAVE), ID(P_SQ_TYPE), ID(P_ARP_MODE), ID(P_ARP_RATE), ID(P_ARP_RANGE), ID(P_ARP_CHANCE), ID(P_SQ_LEN);
    ID(P_SQ_GATE), ID(P_SQ_TRANS), ID(P_BS_OCTAVE), ID(P_BS_MODEL), ID(P_KEYS), ID(P_CANAL), ID(P_TEMPO);
    ID(P_CH_GLINT), ID(P_CH_BASS), ID(P_MORPH), ID(P_LOAD), ID(P_TIMBRE), ID(P_GX_SPARK), ID(P_GX_BLUR);
    ID(P_GX_SINK), ID(P_GD_TIME), ID(P_L1_SHAPE), ID(P_L2_SHAPE), ID(P_L3_SHAPE), ID(P_MX_DRONE), ID(P_MX_BASS);
    ID(P_MX_GLINT), ID(P_MX_CLOUD), ID(PG_ARP), ID(PG_SEQ), ID(PG_SEQ2), ID(PG_SEQ3), ID(PG_SCENES), ID(SQT_ARP);
    ID(SQT_SEQ), ID(OCT_DEF), ID(P_BQ_ON), ID(P_BQ_STEPS), ID(P_BQ_TRANS), ID(P_BQ_LEN), ID(PG_BSEQ), ID(PG_BSEQ2), ID(P_SPEAKER), ID(P_ANIM), ID(P_CLOCK);
    ID(P_BS_ATTACK), ID(P_BS_DECAY), ID(P_BS_SUSTAIN), ID(P_BS_RELEASE), ID(P_BS_PULSE), ID(P_GL_ATTACK);
    ID(P_GL_DECAY), ID(P_GL_SUSTAIN), ID(P_GL_RELEASE), ID(P_SQ_STEP), ID(P_SQ_NOTE), ID(P_SQ_ACC), ID(P_SQ_CLEAR);
    ID(P_BQ_STEP), ID(P_BQ_NOTE), ID(P_BQ_CLEAR), ID(P_EQ_PRESET), ID(P_EQ_LOW), ID(P_EQ_HIGHF), ID(P_FL_TYPE);
    ID(P_FL_CUT), ID(P_FL_RESO), ID(P_FL_TRACKS), ID(P_TP_ON), ID(P_TP_SAT), ID(P_TP_WOBBLE), ID(P_TP_TRACKS), ID(PG_FILTER);
    ID(PG_TAPE), ID(PG_EQ), ID(PG_EQ2), ID(PG_ROUTING), ID(PG_SCREEN), ID(P_FL_PRE), ID(P_TP_PRE), ID(P_PUMP_CLOUD);
    ID(P_PN_DRONE), ID(P_PN_BASS), ID(P_PN_GLINT), ID(P_PN_CLOUD);
#undef ID
    fprintf(f, "},\n\"params\":[\n");
    for (p = 0; p < NPARAM; p++) {       /* each with its words for every value, as the screen writes them */
        uint32_t fm = PD[p].fmt;
        fprintf(f, "%s{\"name\":", p ? ",\n" : "");
        js_str(f, PD[p].name);
        fprintf(f, ",\"max\":%u,\"def\":%u,\"kind\":\"%s\",\"texts\":[", PD[p].max, PD[p].def,
                fm == F_LTGT ? "target" : PD[p].max <= 16u && fm != F_PCT ? "steps" : "gauge");
        for (x = 0; x <= PD[p].max; x++) {
            value_text((int)p, (uint8_t)x, t);
            if (x)
                fputc(',', f);
            js_str(f, t);
        }
        fprintf(f, "]}");
    }
    fprintf(f, "],\n\"pages\":[\n");
    for (i = 0; i < NPAGES; i++) {
        fprintf(f, "%s{\"name\":", i ? ",\n" : "");
        js_str(f, PAGES[i].name);
        fprintf(f, ",\"button\":\"%s\",\"p\":[", BUTTON[PAGES[i].button % NB]);
        for (k = 0; k < 4u; k++)
            fprintf(f, "%s%d", k ? "," : "", PAGES[i].p[k] < NPARAM ? (int)PAGES[i].p[k] : -1);
        fprintf(f, "]}");
    }
    fprintf(f, "],\n\"drone\":{\"names\":");
    js_strs(f, DRONE_NAME, NDPRESET + 1u);
    fprintf(f, ",\"p\":[");
    for (i = 0; i < NDRONE; i++)
        fprintf(f, "%s%u", i ? "," : "", DRONE_P[i]);
    fprintf(f, "],\"presets\":[");
    for (k = 0; k < NDPRESET; k++) {
        fprintf(f, "%s[", k ? "," : "");
        for (i = 0; i < NDRONE; i++)
            fprintf(f, "%s%u", i ? "," : "", DRONE_PRESET[k][i]);
        fputc(']', f);
    }
    fprintf(f, "]},\n\"glint\":{\"names\":");
    js_strs(f, GLINT_NAME, NGPRESET);
    fprintf(f, "},\n\"lfo\":{\"base\":[%u,%u,%u],\"nfree\":%u,\"shapes\":", LFO_BASE[0], LFO_BASE[1], LFO_BASE[2],
            LFO_NFREE);
    js_strs(f, LFO_SHAPE_NAME, 7);
    fprintf(f, ",\"targets\":[");
    for (i = 0; i < NLFO_TGT; i++) {     /* each in its track's colour, as the screen has it (tgt_col) */
        uint16_t c = tgt_col(i);
        fprintf(f, "%s{\"name\":", i ? "," : "");
        js_str(f, LFO_TGT_NAME[i]);
        fprintf(f, ",\"p\":%d,\"track\":\"%s\"}", LFO_TGT[i] < NPARAM ? (int)LFO_TGT[i] : -1,
                c == C_ICE ? "glint" : c == C_LAMP ? "bass" : c == C_FOG ? "drone" : "off");
    }
    fprintf(f, "]},\n\"notes\":");
    js_strs(f, NOTE_NAME, 12);
    fprintf(f, ",\"scales\":");
    js_strs(f, SCALE_NAME, NSCALES);
    fprintf(f, ",\"chords\":");
    js_strs(f, CHORD_NAME, NCHORDS);
    /* harmony.c's own: each scale's pitch classes (bit k: k semitones over the root), each chord's shape (the first
     * "diatonic" in scale steps, the rest in semitones), as chord_build makes a played note a chord */
    fprintf(f, ",\n\"harmony\":{\"scale_bits\":[");
    for (i = 0; i < NSCALES; i++)
        fprintf(f, "%s%u", i ? "," : "", SCALE_BITS[i]);
    fprintf(f, "],\"shapes\":[");
    for (i = 0; i < NCHORDS; i++) {
        fprintf(f, "%s[", i ? "," : "");
        for (k = 0; k < MAXCHORD && CHORD_SHAPE[i][k] >= 0; k++)
            fprintf(f, "%s%d", k ? "," : "", CHORD_SHAPE[i][k]);
        fputc(']', f);
    }
    fprintf(f, "],\"diatonic\":%d}", (int)CH_DIATONIC);
    fprintf(f, ",\n\"factory\":[");      /* each slot with nothing saved in it, as scene_get gives it: its bytes, hex */
    for (i = 0; i < NSCENES; i++) {
        scene_t sc;
        const uint8_t *b = (const uint8_t *)&sc;
        factory_get(i, &sc);
        fprintf(f, "%s\n\"", i ? "," : "");
        for (k = 0; k < sizeof sc; k++)
            fprintf(f, "%02x", b[k]);
        fputc('"', f);
    }
    fprintf(f, "]}\n");
    fclose(f);
    return 0;
}

/* the tests' own commands: "schema FILE" (above); "backuphold 1|0" (app_backup_hold, as a backup streaming);
 * "nosettings" (the settings' record gone, as a new FM-1 has none); "prefs" (the settings asked while something
 * sounded, kept now);
 * "oldstore L" saves the records in an earlier layout, L: 3 FRSV + FRS3 (before BASS), 4 FRSW + FRS4 (SPACE, 10,
 * where MODEL is now), 5 FRSX + FRS5, 6 FRSX + FRS6 (GLINT on channel 5, BASS on 6), 7 FRSY + FRS7, 8 FRSZ + FRS8,
 * 9 FRSa + FRS9, 10 FRSb + FRSA, 11 FRSc + FRSB, 12 FRSd + FRSC, 13 FRSe + FRSD, 14 FRSf + FRSE (the same channels):
 * slot 6 a scene named OLD with a chord on D3 (and the bass on A1), the settings with TEMPO at 100 BPM and slot 6
 * last; 15 FRSg, 16 FRSG (no OBJ_BSEQ), 17 FRSh + FRSH (from the records as last saved: app_host_cmd); for a reboot
 * (or a backup) to read and make over */
/* "enclate E N": N detents of encoder E (0 SELECT .. 6 K4) that come in during the next pass, once the name editor
 * or a pop-up has read the encoders (on the device the scan brings them any time) */
static int32_t enc_late[NE];
static void enc_late_in(void)
{
    uint32_t e;
    for (e = 0; e < NE; e++) {
        enc_acc[e] += enc_late[e];
        enc_late[e] = 0;
    }
}

/* the settings as last saved, into raw as FRSH kept them (the tests: oldstore 16, 17): the sound FRSH_NV long, a
 * setting learned after it numbered so; the magic the caller's */
static void set_frsh(void)
{
    uint32_t i;
    memcpy(raw, &set, 4u + FRSH_NV);
    memcpy(raw + 4u + FRSH_NV, (uint8_t *)&set + offsetof(settings_t, octave), FRSH_BYTES - 4u - FRSH_NV);
    for (i = 0; i < NLEARN; i++) {
        uint8_t *l = raw + offsetof(settings_t, learn_p) - (NSOUND - FRSH_NV) + i;
        if (*l >= NSOUND && *l < NPARAM)
            *l = (uint8_t)(*l - (NSOUND - FRSH_NV));
    }
}

/* the settings' record as FRSJ kept it (the sound FRSL_NV long; magic gm), the scenes' as FRSl did (GLINT's sequence in
 * each scene's record; magic sm), the basses' as FRBT did (from the bass's notes on), each from what is now */
static void old_frsj(uint32_t gm)
{
    uint32_t i;
    memcpy(raw, &set, 4u + FRSL_NV);
    memcpy(raw + 4u + FRSL_NV, (uint8_t *)&set + offsetof(settings_t, octave), FRSJ_BYTES - 4u - FRSL_NV);
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(gm >> (8u * i));
    for (i = 0; i < NLEARN; i++) {
        uint8_t *l = raw + offsetof(settings_t, learn_p) - (NSOUND - FRSL_NV) + i;
        if (*l >= NSOUND && *l < NPARAM)
            *l = (uint8_t)(*l - (NSOUND - FRSL_NV));
    }
    plat_store_save(OBJ_SETTINGS, raw, FRSJ_BYTES);
}

static void old_frsl(uint32_t sm)
{
    uint32_t i;
    uint8_t *o;
    memcpy(raw, &scn, 8u);
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(sm >> (8u * i));
    for (i = 0; i < NSCENES; i++) {
        o = raw + 8u + i * FRSL_REC;
        memcpy(o, &scn.s[i], SC_NAME + FRSL_NV);
        memcpy(o + SC_NAME + FRSL_NV, &scn.s[i].nch, SC_TAIL);
    }
    plat_store_save(OBJ_SCENES, raw, 8u + NSCENES * FRSL_REC);
}

/* the settings' record as 2.5's betas kept it (FRSK: the sound FRSO_NV long, no ANIMATIONS: its byte the padding,
 * written 0), the scenes' as they did (FRSo: each up to GLINT's sequence, the sound FRSO_NV long); OBJ_BSEQ as it is */
static void old_frsk(void)
{
    uint32_t i;
    memset(raw, 0, FRSK_BYTES);
    memcpy(raw, &set, 4u + FRSO_NV);
    memcpy(raw + 4u + FRSO_NV, (uint8_t *)&set + offsetof(settings_t, octave),
           offsetof(settings_t, anim) - offsetof(settings_t, octave));
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(SET_FRSK >> (8u * i));
    for (i = 0; i < NLEARN; i++) {
        uint8_t *l = raw + offsetof(settings_t, learn_p) - (NSOUND - FRSO_NV) + i;
        if (*l >= NSOUND && *l < NPARAM)
            *l = (uint8_t)(*l - (NSOUND - FRSO_NV));
    }
    plat_store_save(OBJ_SETTINGS, raw, FRSK_BYTES);
}

static void old_frso(void)
{
    uint32_t i, rec = SC_NAME + FRSO_NV + SC_TAIL - SC_SEQ;
    uint8_t *o;
    memcpy(raw, &scn, 8u);
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(SCN_FRSO >> (8u * i));
    for (i = 0; i < NSCENES; i++) {
        o = raw + 8u + i * rec;
        memcpy(o, &scn.s[i], SC_NAME + FRSO_NV);
        memcpy(o + SC_NAME + FRSO_NV, &scn.s[i].nch, SC_TAIL - SC_SEQ);
    }
    plat_store_save(OBJ_SCENES, raw, 8u + NSCENES * rec);
}

static void old_frbt(uint32_t bm, uint32_t n)   /* (FRBT's FRBT_SCENE a scene, FRBS's FRBS_SCENE) */
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(bm >> (8u * i));
    for (i = 0; i < NSCENES; i++)
        memcpy(raw + 4u + i * n, scn.s[i].bnote, n);
    plat_store_save(OBJ_BSEQ, raw, 4u + NSCENES * n);
}

/* the tests' own scene (testscene): 1.1's first factory scene, MIST, as it was (the tests were written on it; the
 * factory scenes since are another set), made as a factory scene was, loaded as slot 01 */
static void test_scene(void)
{
    static const scene_t MIST = {"MIST", {76, 89, 32, 19, 51, 44, 57, 25, 0, 70, 0, 89, 89, 83, 76, 38, 44, 51, 25, 38, 95,
                                          79, 64, 0, 95, 64, 13, 0, 25, 127, 50, 64, 57, 2, 0, 0, 0, 8},
                                 0, 0, {{0}}, {0}, -1, {0}, {0}, {0}, 0, 15, 14, 60};
    scene_t sc = MIST;
    glint_preset(sc.v, sc.v[P_GL_PRESET]);
    arp_defaults(sc.v);
    later_defaults(sc.v, P_BS_DRIVE);
    st.scene = 0;
    st.pick = 0;
    scene_take(&sc);
}

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
    if (!strcmp(cmd, "testscene")) {     /* testscene: the tests' own scene (test_scene), as if slot 01 were it */
        test_scene();
        return 1;
    }
    if (!strcmp(cmd, "backuphold")) {    /* backuphold 1|0: a backup streaming, as device/backup.c holds the saving */
        app_backup_hold(atoi(a));
        return 1;
    }
    if (!strcmp(cmd, "askscreen")) {     /* askscreen: the restore question's screen, as fm1-base's rs_ask draws it */
        app_backup_ask_screen();
        return 1;
    }
    if (!strcmp(cmd, "enclate")) {
        enc_late[atoi(a) % NE] += atoi(b);
        return 1;
    }
    if (!strcmp(cmd, "schema")) {        /* schema FILE: the backup page's (schema_write) */
        if (schema_write(a))
            printf("schema: cannot write %s\n", a);
        return 1;
    }
    if (!strcmp(cmd, "prefs")) {         /* prefs: the settings asked to be kept while something sounded, kept now (as
                                          * the quiet after it would), for a reboot or an old store */
        if (st.prefs_due)
            prefs_save();
        return 1;
    }
    if (!strcmp(cmd, "nosettings")) {    /* nosettings: the settings never saved, as on a new FM-1 (the scenes kept),
                                          * for a reboot */
        plat_store_save(OBJ_SETTINGS, raw, 0);
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 12) {   /* oldstore 15: the scenes as 1.1's first build kept them (FRSg,
                                                  * names of ten), the settings left as they are: slot 6 TENLETTERS, an
                                                  * arpeggio (60 64) and a sequence (72) on GLINT's OCTAVE 1 */
        ls = &OLD_SCN[11];
        rs = scn_bytes(ls);
        nv = ls->nv;
        memset(raw, 0, sizeof raw);
        for (i = 0; i < 4u; i++)
            raw[i] = (uint8_t)(ls->magic >> (8u * i));
        raw[4] = 1u << 5;
        o = raw + 8u + 5u * rs;
        memcpy(o, "TENLETTERS", 10);
        for (i = 0; i < nv; i++)
            o[10u + i] = st.v[i];
        o[10u + P_GL_OCTAVE] = 1;
        o[10u + P_SQ_TYPE] = SQT_ARP;
        o[11u + nv] = 2;                 /* narp */
        o[12u + nv + SC_CHORDS * 5u] = 60;
        o[13u + nv + SC_CHORDS * 5u] = 64;
        o[12u + nv + SC_CHORDS * 5u + SC_ARP] = 0xFF;   /* no bass */
        o[13u + nv + SC_CHORDS * 5u + SC_ARP] = 72;     /* the sequence's first step */
        plat_store_save(OBJ_SCENES, raw, 8u + NSCENES * rs);
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 13) {   /* oldstore 16: 1.1's records before the bass's sequence: the
                                                  * settings as last saved, in FRSG's layout (without it), and no
                                                  * OBJ_BSEQ (the scenes' record as it is) */
        set_frsh();
        for (i = 0; i < 4u; i++)
            raw[i] = (uint8_t)(SET_FRSG >> (8u * i));
        memset(raw + FRSH_BSEQ, 0, FRSG_BYTES - FRSH_BSEQ);
        plat_store_save(OBJ_SETTINGS, raw, FRSG_BYTES);
        plat_store_save(OBJ_BSEQ, raw, 0);
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 14) {   /* oldstore 17: 1.1's records before BASS's DECAY, SUSTAIN and
                                                  * PULSE (FRSh, FRSH): the scenes and the settings as last saved,
                                                  * without them (a CC learned on a setting after the sound numbered
                                                  * as it was); OBJ_BSEQ as it is */
        set_frsh();
        for (i = 0; i < 4u; i++)
            raw[i] = (uint8_t)(SET_FRSH >> (8u * i));
        plat_store_save(OBJ_SETTINGS, raw, FRSH_BYTES);
        memcpy(raw, &scn, 8u);
        for (i = 0; i < 4u; i++)
            raw[i] = (uint8_t)(SCN_FRSH >> (8u * i));
        for (i = 0; i < NSCENES; i++) {
            o = raw + 8u + i * FRSH_REC;
            memcpy(o, &scn.s[i], SC_NAME + FRSH_NV);
            memcpy(o + SC_NAME + FRSH_NV, &scn.s[i].nch, SC_TAIL);
        }
        plat_store_save(OBJ_SCENES, raw, 8u + NSCENES * FRSH_REC);
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 15) {   /* oldstore 18 [N]: 1.1's records before GLINT's RELEASE (FRSj,
                                                  * FRSI): the scenes and the settings as last saved, SCATTER (N, else
                                                  * its default 80) where RELEASE is; OBJ_BSEQ as it is */
        uint8_t sc = b && *b ? (uint8_t)atoi(b) : 80u;
        uint8_t keep = set.v[P_GL_RELEASE];
        set.v[P_GL_RELEASE] = sc;
        old_frsj(SET_FRSI);
        set.v[P_GL_RELEASE] = keep;
        old_frsl(SCN_FRSJ);
        for (i = 0; i < NSCENES; i++)
            raw[8u + i * FRSL_REC + SC_NAME + P_GL_RELEASE] = sc;
        plat_store_save(OBJ_SCENES, raw, 8u + NSCENES * FRSL_REC);
        old_frbt(BSQ_FRBT, FRBT_SCENE);
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 16) {   /* oldstore 19: 1.1's basses' record before the scenes' TEMPO (FRBS):
                                                  * each scene's bass as it is, without its TEMPO; the scenes' and the
                                                  * scenes' record as FRASIL kept it then (FRSl) */
        old_frsl(SCN_FRSL);
        old_frbt(BSQ_FRBS, FRBS_SCENE);
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 17) {   /* oldstore 20: 2.1's first builds' records, before MASTER EQ, MASTER
                                                  * FILTER and LOFI TAPE (FRSl, FRBT, FRSJ): the scenes, their basses and
                                                  * the settings as last saved, without them (a CC learned on a setting
                                                  * after the sound numbered as it was) */
        old_frsj(SET_FRSJ);
        old_frsl(SCN_FRSL);
        old_frbt(BSQ_FRBT, FRBT_SCENE);
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 18) {   /* oldstore 21: 2.5's betas' records (b6 .. b9), before ROUTING, the
                                                  * pans and ANIMATIONS (FRSo, FRSK; FRBU as now): the scenes and the
                                                  * settings as last saved, without them */
        old_frsk();
        old_frso();
        return 1;
    }
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
