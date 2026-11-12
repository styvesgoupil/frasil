/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Copyright (C) 2026 Charles Vestal (from fm1-base's template) */
/* FRASIL: the app. SELECT picks the track, DRONE, GLINT or BASS: the keys play it, and EDIT, ENV, ARP, SEQ and REC act on
 * it; the header wears its colour. KEYS (HOME's second page) says how the keys play it. The three left encoders are the
 * performance's hands (TRACK, its PRESET, its TIMBRE, BRIGHT or FILTER), K1 .. K4 the page's parameters; PLAY lets every
 * note go, ENV held swells the drone's voices, REC records into the track's sequence (held: the metronome), FREEZE
 * (CLOUD's K4) holds the cloud. MIDI in plays the drones too, through the same harmony. Nothing plays by itself
 * (SPARKLE's glints answer what the drones hold).
 * The FM-1's shell around FRASIL's engine (engine/: core.c the brain, dsp/ the sound, compiled apart: fm1.toml o2):
 * the store's records, the slots, STORE and the names, the pop-ups, the panel (the pages, the buttons and their holds,
 * the encoders, the keys' scan and lights), MIDI learn, the contract with the platform. Part of the unity build
 * (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in scope. */
#define FRASIL_SHELL                     /* (the engine in the FM-1's unity build: the platform's libc and clamp) */
#if PLAT_SMK37
#define FRS_NKEYS 32                     /* the SMK-37: the FM-1's 27, then 27..31 for its ten outer keys (keys_outer) */
#else
#define FRS_NKEYS NKEYS
#endif
#if defined PLAT_HOST && !defined PLAT_WEB
static uint8_t test_arp_log;             /* (arplog 1: each arpeggio's step and each set it takes, printed) */
#define ARP_TRACE(a, what, note)                                                                                       \
    do {                                                                                                               \
        if (test_arp_log)                                                                                              \
            printf("  arp %s %6u ms  grid %7.2f  %s %d\n", (a) == &arp ? "GLINT" : "BASS ", (unsigned)eng_ms(),       \
                   (double)(grid.wraps * 9600u + grid.tick), what, note);                                              \
    } while (0)
#endif
#include "core.c"                       /* (FRASIL's engine: every setting, the scenes, the brain; its hooks below) */
#include "pages.c"

/* PERFORM's LIVE (below, after PERFORM over USB): each input the FM-1 gives the engine, told to a page that asks, as it
 * is given, so that a fresh engine fed them in their order sounds as this one. app.c's param_set calls from here on are
 * the hand's (the panel's, MIDI learn's, the tests'): each one told; the engine's own, inside its calls, are not (the
 * same call makes them on the other side) */
#include "frasil_stream.h"               /* (the ops: SHARD STREAM's numbers, its args) */
/* the FM-1's gestures frasil_stream.h has no op for (yet), each one frs_ call; a reader that doesn't know one skips it
 * (an op's top bits: its args' length) */
#define LV_RELEASE 0x05u                 /* frs_release: PLAY tapped twice quick (RELEASE ALL) */
#define LV_RELEASE_ALL 0x06u             /* frs_release_all: PLAY held (RELEASE ALL, or RESTORE) */
#define LV_KEY_PRESSURE 0x48u            /* (key, pressure): frs_key_pressure (the iPad's FST_KEY_PRESSURE) */
#define LV_SWELL 0x49u                   /* (held, 0): frs_swell, ENV held */
#define LV_COMPARE 0x4Au                 /* (on, 0): frs_compare, OCT- and EDIT held */
#define LV_RECORD 0x4Bu                  /* (layer, 0): frs_record_layer, REC tapped */
#define LV_KEY_SINGLE 0x86u              /* (key, note, track, velocity): FST_KEY_DOWN's, a single note (SEL held:
                                          * frs_key_down's single) */
#define LV_ANC 0x7Fu                     /* lv_long's src: the ANCHOR's scene (else a slot, scene_get's) */
static uint8_t lv_on;                    /* a page wants them (HELLO's flags, bit 1) */
static uint8_t lv_cc_took;               /* the MIDI in hand was MIDI learn's (eng_cc): its setting told, not the CC */
static void lv_key(uint32_t k, int note, uint32_t vel, int single);
static void lv_key_up(uint32_t k);
static void lv_pressure(uint32_t k, uint8_t p);
static void lv_midi(uint32_t pkt);
static void lv_op(uint32_t op, uint32_t a0, uint32_t a1);
static void lv_long(uint32_t op, uint32_t src);
static void lv_set(uint32_t p, uint32_t v);
static void lv_preset(const uint8_t *in, uint32_t n);
static void lv_record_off(void);
static void lv_rec(uint32_t layer);
static void lv_watch(int swell);
static void lv_pump(void);
static void lv_lead(uint32_t how);
static void lv_wheels(uint32_t mod, int32_t bend);
#ifndef PLAT_WEB
static void lv_param_set(int p, int32_t x);
#define param_set(p, x) lv_param_set((p), (x))
#endif

#define SET_MAGIC 0x53535246u            /* "FRSS": the settings record (a new one when its layout changes) */
#define SET_FRSR 0x52535246u             /* "FRSR": 3.0.2-b5's test builds', this one with the LFOs' AMOUNTs in percent
                                          * (-100 % .. +100 %, 100 its 0 %): each made a multiplier, |v - 100| */
#define SET_FRSQ 0x51535246u             /* "FRSQ": 3.0.2's, before the LFOs' AMOUNTs and SWELL: this one to tp_dropout
                                          * (FRSQ_BYTES), the twelve at x1.00, SWELL 0 */
#define FRSQ_BYTES 284u
#define SET_FRSP 0x50535246u             /* "FRSP": 3.0's before LOFI TAPE II: this one to saver_off (FRSP_BYTES), the four
                                          * settings at their defaults */
#define FRSP_BYTES 280u
#define SET_FRSO 0x4F535246u             /* "FRSO": 3.0's before KEYS: this one to gl_vib, its padding (FRSO_BYTES) */
#define FRSO_BYTES ((uint32_t)((offsetof(settings_t, bs_bite) + 4u) & ~3u))
#define SET_FRSN 0x4E535246u             /* "FRSN": 2.7's first test builds' (FRSN_BYTES): this one to bq_len, its PLAYS
                                          * SEQUENCE then (1: on), no arpeggio on the bass, no GATE */
#define FRSN_BYTES 256u
#define SET_FRSM 0x4D535246u             /* "FRSM": 2.6's: FRSN's, the bass's sequence in sixteen steps of their own
                                          * lengths (bseq_over), no STEP LENGTH (FRSN_BYTES too) */
#define SET_FRSL 0x4C535246u             /* "FRSL": 2.5's (2.5 .. 2.5.1): this one, its sound FRSQ_NV long (FRSL_BYTES),
                                          * the settings learned numbered so */
#define FRSQ_NV 153u                     /* the sound before CHOIR III's SOFT (FRSq, FRSL, FRSr) */
#define FRSL_BYTES 252u
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
#define FRSH_BSEQ (FRSH_BYTES - 2u * OLD_BSEQ - 3u)
#define FRSG_BYTES 192u
#define SCN_MAGIC 0x73535246u            /* "FRSs": the scenes record (with their notes and the bass; names of
                                          * sixteen), each scene up to GLINT's sequence (SC_REC): the sequences in
                                          * OBJ_BSEQ (the sound outgrew the record) */
#define SCN_FRSQ 0x71535246u             /* "FRSq": 2.5's (OLD_SCN's last): this one, its sound FRSQ_NV long */
#define SCN_FRSO 0x6F535246u             /* "FRSo": 2.5's betas' (OLD_SCN's last): this one, its sound FRSO_NV long */
#define SCN_FRSL 0x6C535246u             /* "FRSl": the one before MASTER EQ and MASTER FILTER (OLD_SCN's last, the
                                          * sequence in each scene's record), GLINT's RELEASE in it */
#define SCN_FRSJ 0x6A535246u             /* "FRSj": the one before GLINT's RELEASE (OLD_SCN's last): this one, SCATTER
                                          * where RELEASE is */
#define SCN_FRSH 0x68535246u             /* "FRSh": the one before BASS's DECAY, SUSTAIN and PULSE (OLD_SCN's last) */
#define BSQ_MAGIC 0x58425246u            /* "FRBX": the scenes' sequences (OBJ_BSEQ), each GLINT's, then the bass's
                                          * steps, PLAYS, SEQ LENGTH, TRANSPOSE, the scene's TEMPO, the bass's RATE, its
                                          * arpeggio's MODE, RANGE, CHANCE, its GATE, BASS TONE's three, its arpeggio's
                                          * notes, its SPACE */
#define DSQ_MAGIC 0x52445246u            /* "FRDR": the scenes' drone sequences (OBJ_DSEQ, 3.0), each its settings (dq_on ..
                                          * dq_trans), then its steps' notes seven bits each (dsq_pack), then LOFI TAPE II's
                                          * four (tp_flutter ..) */
#define DSQ_FRDQ 0x51445246u             /* "FRDQ": 3.0's before them: a scene's DSQ_SET + DSQ_PACK bytes, the four at their
                                          * defaults */
#define UPR_MAGIC 0x50555246u            /* "FRUP": the player's presets (OBJ_UPRE, 3.0) */
#define NMS_MAGIC 0x4F4E5246u            /* "FRNO": what of the scenes OBJ_SCENES and the others have no room for (OBJ_NAMES:
                                          * names_save): the rest of their names, their LFOs' AMOUNTs, SWELL */
#define NMS_FRNN 0x4E4E5246u             /* "FRNN": 3.0.2-b5's test builds', this one with the AMOUNTs in percent (each
                                          * made a multiplier, |v - 100|: frs_scene_amt_over's) */
#define NMS_FRNM 0x4D4E5246u             /* "FRNM": 3.0.1's, the rest of the names alone (the AMOUNTs x1.00, SWELL 0) */
#define BSQ_FRBW 0x57425246u             /* "FRBW": 2.7's test builds' (b4 ..): to the arpeggio's notes, no SPACE */
#define BSQ_FRBV 0x56425246u             /* "FRBV": 2.7's first test builds': to RATE, PLAYS SEQUENCE then (1: on) */
#define BSQ_FRBU 0x55425246u             /* "FRBU": 2.6's (2.1 .. 2.6.1): to TEMPO, the bass's sixteen notes and their
                                          * lengths (bseq_over) */
#define BSQ_FRBT 0x54425246u             /* "FRBT": the one before (with FRSl): from the bass's notes on */
#define BSQ_FRBS 0x53425246u             /* "FRBS": the one before the scenes' TEMPO (FRBT's less one a scene) */
#if FRS_LEAD
#define SHARD_MAGIC 0x7E535246u          /* "FRS~" (card #152): a shard as the River and the engine keep it, "FRS}"'s
                                          * scene_t then the LEAD's settings (P_LD_WAVE .. P_LD_FRELEASE: an OBJ_LEAD
                                          * slot's entry without its flag), NLDS bytes after SWELL (the schema's
                                          * shard_t); "FRS}" and every earlier one: the LEAD MOSS, each its default */
#define SHARD_FRSBRACE 0x7D535246u
#else
#define SHARD_MAGIC 0x7D535246u
#endif
                                         /* "FRS}": a scene whole (scene_t: its sequences, its TEMPO, the bass's RATE,
                                          * arpeggio, BASS TONE, SPACE and GLINT's AUTO VIB after the rest, then the
                                          * drone's sequence, then LOFI TAPE II's four, then the name's rest, the LFOs'
                                          * AMOUNTs and SWELL), as the River keeps it, a shard; "FRS|": 3.0.2-b5's test
                                          * builds', the same bytes, the AMOUNTs in percent (frs_scene_amt_over); "FRS{": 3.0.2's before
                                          * those three (590 bytes); "FRSz": 3.0's before LOFI TAPE II (586 bytes); "FRSy": 3.0's before the
                                          * drone's sequence (324 bytes); "FRSx": 2.7's test builds' before AUTO VIB (323 bytes);
                                          * "FRSw": before SPACE (322 bytes); "FRSv":
                                          * 2.7's test builds' before BASS TONE (319 bytes); "FRSu": 2.7's
                                          * first test builds' (to the bass's RATE, PLAYS SEQUENCE then, 1: on); "FRSt":
                                          * 2.6's (to TEMPO, the bass in sixteen steps of their own lengths: bseq_over);
                                          * "FRSr": 2.5's (FRSQ_NV sounds);
                                          * "FRSp": 2.5's betas' (FRSO_NV
                                          * sounds); "FRSn": the one before MASTER EQ and
                                          * MASTER FILTER (FRSL_NV sounds); "FRSm": that without its TEMPO;
                                          * "FRSk" shards: without it, SCATTER where GLINT's
                                          * RELEASE is; "FRSi": before BASS's DECAY, SUSTAIN and PULSE too (FRSH_NV
                                          * sounds); "FRSh": those without the bass's too */
#define SHARD_FRSBAR 0x7C535246u
#define SHARD_FRSTP 0x7B535246u
#define SHARD_FRSZ 0x7A535246u
#define SHARD_FRSY 0x79535246u
#define SHARD_FRSX 0x78535246u
#define SHARD_FRSW 0x77535246u
#define SHARD_FRSU 0x75535246u
#define SHARD_FRST 0x74535246u
#define SHARD_FRSR 0x72535246u
#define SHARD_FRSP 0x70535246u
#define SHARD_FRSN 0x6E535246u
#define SHARD_FRSM 0x6D535246u
#define SHARD_FRSK 0x6B535246u
#define SHARD_FRSI 0x69535246u
#define OCT_DEF (-1)                     /* the keys start at F2: drones live low */
#define HELP_PAGES 5                     /* the help: three of buttons, the encoders, the keys */
#define HOLD_MS 400u                     /* a button held this long does its other thing (on release, a tap its own) */
#define PLAY_HOLD_MS 600u                /* PLAY's, which lets everything go: longer, the screen counting it down */
#define REC_HOLD_MS 800u                 /* REC's, METRONOME: longer, a slow press in a take a press (it turned it off, kept) */
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
    uint8_t bnote[SC_BSEQ];              /* the bass's sequence as last saved (till FRSN: its sixteen notes, then
                                          * their lengths) */
    uint8_t bq_on, bq_steps, bq_trans;   /* and its PLAYS, SEQ LENGTH, TRANSPOSE */
    uint8_t anim;                        /* ANIMATIONS LOW (0, as FRSK's padding byte reads: NORMAL) */
    uint8_t clock;                       /* CLOCK INTERNAL (0, as FRSL's padding byte reads till 2.5-b30: AUTO) */
    uint8_t bq_len;                      /* the bass's RATE (FRSN's: FRSM's padding byte there) */
    uint8_t ba_mode, ba_range, ba_chance, bq_gate;   /* its arpeggio's MODE, RANGE, CHANCE, its sequence's GATE (FRSO's) */
    uint8_t bs_weight, bs_punch, bs_bite;   /* BASS TONE's WEIGHT, PUNCH, BITE */
    uint8_t bs_space;                    /* the bass's SPACE (0, as FRSO's padding byte reads) */
    uint8_t gl_vib;                      /* GLINT's AUTO VIB (the same) */
    uint8_t ky[12];                      /* KEYS, the three tracks' (P_KY_DR_HARM ...) */
    uint8_t metro, metro_on, count_in;   /* GLO > RECORD: METRONOME, its last on value, COUNT-IN */
    uint8_t arp_snap;                    /* GLOBAL's ARP SNAP + 1 (0, as FRSP's padding byte reads before it: its
                                          * default, 1/4); in the padding, the record's size the same (280) */
    uint8_t saver_off;                   /* SCREENSAVER OFF (0, as FRSP's last padding byte reads: ON); in the padding
                                          * too, the record's size the same (280) */
    uint8_t tp_flutter, tp_tone, tp_noise, tp_dropout;   /* LOFI TAPE II's four (FRSQ: FRSP's end at saver_off, read with
                                                          * their defaults) */
    uint8_t lfo_amt[12];                 /* the LFOs' AMOUNTs, LFO 1's TARGET 1 .. LFO 3's TARGET 4 (P_L1_AMT1 ..; FRSS:
                                          * FRSQ's end at tp_dropout, read at x1.00; FRSR's in percent, made multipliers) */
    uint8_t swell;                       /* GENERATIVE FX's SWELL (the same: 0) */
    uint8_t head;                        /* GLO > SCREEN's HEADER: 1 DARK (0, as FRSS's padding byte reads: INVERT);
                                          * in the padding, the record's size the same (300) */
    int8_t a4;                           /* GLO > RECORD's TUNE A4 less 440 Hz, -25 .. +26 (0, as FRSS's padding byte
                                          * reads: 440); in the padding too (300); then one byte of it */
} settings_t;

typedef struct {
    uint32_t magic;
    uint16_t used;                       /* bit s: slot s has a saved scene */
    uint16_t rsv;
    scene_t s[NSCENES];
} scenes_t;                              /* (in the store as two records: scenes_save) */

/* the player's presets (the SAVE flow): sixteen of each track's, each the engine's bytes (eng_preset_capture: "FRp" ...,
 * its first byte 0: none, those after it none too), listed after the factory ones */
#define NUP 16u
typedef struct {
    uint32_t magic;
    uint8_t dr[NUP][PRESET_BYTES];
    uint8_t gl[NUP][PRESET_HEAD + 2u * sizeof PRESET_GL];
    uint8_t bs[NUP][PRESET_HEAD + 2u * sizeof PRESET_BS];
} upresets_t;
_Static_assert(sizeof(upresets_t) <= 3840u, "the player's presets fit a store object");

static settings_t set;
static scenes_t scn;
static upresets_t up;
static int8_t up_cur[3 + FRS_LEAD];      /* each track's: the preset of the player's taken or stored last, -1 none (a factory
                                          * one taken, a scene): turned since or not, INTO starts on it (the LEAD's
                                          * fourth: its own in OBJ_LEAD, app.c ldu_*) */

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
    {SCN_FRSQ, FRSQ_NV, 5},              /* "FRSq": with ROUTING and the pans (2.5, 2.5.1) */
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
 * GLINT's sequence), and OBJ_BSEQ, the rest of each (magic, each scene's sequence, bnote ... barp). A scene_t is the two
 * records' bytes one after the other */
#define SC_TAIL (2u + SC_CHORDS * 5u + SC_ARP + 1u + SC_SEQ)   /* after the sound, to the bass's sequence: nch .. seq */
#define SC_REC (SC_NAME + NSOUND + SC_TAIL - SC_SEQ)
#define FRSL_REC (SC_NAME + FRSL_NV + SC_TAIL)   /* FRSl's and FRSj's (the sequence in them) */
#define FRSH_REC (SC_NAME + FRSH_NV + SC_TAIL)   /* FRSh's (its sound FRSH_NV long) */
#define SCN_BYTES (8u + NSCENES * SC_REC)
#define FRBT_SCENE (2u * OLD_BSEQ + 4u) /* FRBT's: a scene's bass, its notes, its lengths, SEQUENCE, SEQ STEPS,
                                          * TRANSPOSE, TEMPO (FRBU's after GLINT's sequence, FRSt's last) */
#define FRBV_SCENE (SC_SEQ + SC_BSEQ + 5u)   /* FRBV's: GLINT's sequence, the bass's steps, PLAYS, SEQ LENGTH, TRANSPOSE,
                                              * TEMPO, RATE */
#define FRBW_SCENE (FRBV_SCENE + 8u + SC_ARP)   /* FRBW's: FRBV's, MODE, RANGE, CHANCE, GATE, BASS TONE's three, the notes'
                                                 * count and the notes */
#define BSQ_SCENE (FRBW_SCENE + 2u)      /* a scene's in OBJ_BSEQ: FRBW's, the bass's SPACE, GLINT's AUTO VIB */
#define FRBU_SCENE (SC_SEQ + FRBT_SCENE) /* FRBU's: GLINT's sequence, then FRBT's */
#define FRBS_SCENE (FRBT_SCENE - 1u)     /* FRBS's: without TEMPO */
#define BSQ_BYTES (4u + NSCENES * BSQ_SCENE)
#define DSQ_SET NDQ                      /* a scene's in OBJ_DSEQ: its drone sequence's settings, */
#define DSQ_PACK (SC_DSEQ * DQ_NOTES * 7u / 8u)   /* its notes, seven bits each (1 .. 126, 0 none, DQ_TIE), */
#define DSQ_SCENE_FRDQ (DSQ_SET + DSQ_PACK)   /* 230 bytes (FRDQ's) */
#define DSQ_SCENE (DSQ_SCENE_FRDQ + 4u)  /* 234: and LOFI TAPE II's four; sixteen scenes fit the object (scene_t has the notes a
                                          * byte each) */
#define DSQ_BYTES_FRDQ (4u + NSCENES * DSQ_SCENE_FRDQ)
#define DSQ_BYTES (4u + NSCENES * DSQ_SCENE)
/* a scene's name on the FM-1: SC_LONG characters, its first SC_NAME in the scene's name, the rest in its name_more
 * (scene_t's, the shard's FRS| has both). In the store the rest is in OBJ_NAMES (3.0.1), apart, with the LFOs' twelve
 * AMOUNTs and SWELL (3.0.3), so that every record 3.0 reads stays as it reads it (they do not fit OBJ_SCENES: 16 scenes
 * of SC_REC + 16 bytes are 3928, the object holds 3840). OBJ_NAMES: its magic, then each slot's: the sum of its record in
 * OBJ_SCENES as written with it (rec_sum), its SC_MORE characters (NUL-padded), its twelve AMOUNTs, its SWELL. Read back
 * only onto a scene whose record has that sum, so a scene never gets another's: a scene stored again by an earlier
 * FRASIL, or a restore of a backup without OBJ_NAMES, or one a River changed, has its first sixteen alone, its AMOUNTs at
 * +100 %, SWELL 0; a scene the River moved to another slot keeps them (names_load) */
#define SC_LONG 32u
#define SC_MORE (SC_LONG - SC_NAME)
#define NMS_MORE (SC_MORE + 12u + 1u)    /* scene_t's from name_more to swell: what OBJ_NAMES keeps of a scene */
#define NMS_SCENE (4u + NMS_MORE)        /* 33 */
#define NMS_BYTES (4u + NSCENES * NMS_SCENE)   /* 532 */
#define NMS_SCENE_FRNM (4u + SC_MORE)    /* FRNM's: the sum, the name's rest */
#define NMS_BYTES_FRNM (4u + NSCENES * NMS_SCENE_FRNM)
static uint8_t raw[8u + NSCENES * FRSL_REC];   /* an old record as it was saved (the largest, FRSl's scenes: 3800),
                                                * and the scenes' records as they are written and read (FRSs's:
                                                * 3672) */
static uint8_t store_old;                /* bit 0 the scenes, bit 1 the settings: made over from an earlier layout at
                                          * load, the store's still in it (a backup saves them first) */
_Static_assert(sizeof(scene_t) == SC_REC + BSQ_SCENE + DSQ_SET + SC_DSEQ * DQ_NOTES + 4u + NMS_MORE && offsetof(scene_t, seq) == SC_REC &&
                   offsetof(scene_t, tp_flutter) == FRSZ_SCENE && offsetof(scene_t, tp_dropout) == FRSZ_SCENE + 3u &&
                   offsetof(scene_t, name_more) == FRSZ_SCENE + 4u && sizeof(((scene_t *)0)->name_more) == SC_MORE &&
                   offsetof(scene_t, lfo_amt) == offsetof(scene_t, name_more) + SC_MORE &&
                   offsetof(scene_t, swell) == offsetof(scene_t, lfo_amt) + 12u && sizeof(scene_t) == 619u &&
                   SC_NAME_MORE == SC_MORE && SC_LFO_AMT == 12u && SC_NAME_ALL == SC_LONG &&
                   offsetof(scene_t, tp_tone) == offsetof(scene_t, tp_flutter) + 1u &&
                   offsetof(scene_t, tp_noise) == offsetof(scene_t, tp_flutter) + 2u &&
                   offsetof(scene_t, dnote) + SC_DSEQ * DQ_NOTES == FRSZ_SCENE &&
                   offsetof(scene_t, bnote) == SC_REC + SC_SEQ && offsetof(scene_t, nch) == SC_NAME + NSOUND &&
                   SC_BSEQ == 2u * OLD_BSEQ && offsetof(scene_t, bq_on) == SC_REC + SC_SEQ + SC_BSEQ &&
                   offsetof(scene_t, tempo) == SC_REC + SC_SEQ + FRBS_SCENE &&
                   offsetof(scene_t, bq_len) == SC_REC + SC_SEQ + FRBT_SCENE &&
                   offsetof(scene_t, ba_mode) == FRSU_SCENE && offsetof(scene_t, bs_bite) + 1u - offsetof(scene_t, bq_on) == NBQ &&
                   offsetof(scene_t, gl_vib) == offsetof(scene_t, bs_space) + 1u &&
                   offsetof(scene_t, dq_on) == SC_REC + BSQ_SCENE && offsetof(scene_t, dq_on) == FRSY_SCENE &&
                   offsetof(scene_t, gl_vib) + 1u == FRSY_SCENE && offsetof(scene_t, dnote) == offsetof(scene_t, dq_on) + NDQ &&
                   offsetof(scene_t, dq_trans) + 1u - offsetof(scene_t, dq_on) == NDQ &&
                   FRSU_SCENE == SC_REC + FRBV_SCENE && FRST_SCENE == SC_REC + FRBU_SCENE && FRSU_SCENE == 298u &&
                   FRST_SCENE == 297u,
               "a scene: its bytes, no padding, the sequences last; the bass's steps where the old notes and lengths "
               "were, RATE after TEMPO, the bass's arpeggio after it");
_Static_assert(P_BS_DRIVE == 55 && P_BS_RESO == 64 && P_PUMP_RATE == 65 && P_SH_TONE == 77 && P_PUMP_REL == 78 &&
                   P_CH_SHAPE == 81 && P_L1_SHAPE == 94 && P_SQ_LEN == 110 && P_DR_PRESET == 113 && P_L3_SHAPE == 115 &&
                   P_SQ_TYPE == 123 && P_BS_DECAY == FRSH_NV && P_EQ_PRESET == FRSL_NV && P_FL_PRE == FRSO_NV &&
                   P_CH_SOFT == FRSQ_NV,
               "the parameters added come after the old");
_Static_assert(FACTORY_NV == FRSL_NV, "the factory's whole sounds: FRSl's");
_Static_assert(P_GL_ATTACK < NSOUND && P_GL_DECAY < NSOUND && P_GL_SUSTAIN < NSOUND && P_GL_RELEASE < NSOUND &&
                   P_BS_ATTACK < NSOUND && P_BS_DECAY < NSOUND && P_BS_SUSTAIN < NSOUND && P_BS_RELEASE < NSOUND,
               "the LFOs' targets, in the sound (mod)");
_Static_assert(SCN_BYTES <= 3840u && BSQ_BYTES <= 3840u && DSQ_BYTES <= 3840u && DSQ_PACK * 8u == SC_DSEQ * DQ_NOTES * 7u,
               "the scenes' records fit a store object each (4 KiB less its 256-byte header)");
_Static_assert(offsetof(settings_t, bq_len) == offsetof(settings_t, clock) + 1u &&
                   offsetof(settings_t, clock) == offsetof(settings_t, anim) + 1u &&
                   offsetof(settings_t, anim) == offsetof(settings_t, bq_trans) + 1u &&
                   FRSN_BYTES == ((offsetof(settings_t, clock) + 4u) & ~3u) &&
                   offsetof(settings_t, ba_mode) == offsetof(settings_t, bq_len) + 1u &&
                   FRSO_BYTES >= offsetof(settings_t, ky) &&
                   offsetof(settings_t, bs_space) == offsetof(settings_t, bs_bite) + 1u &&
                   offsetof(settings_t, gl_vib) == offsetof(settings_t, bs_space) + 1u &&
                   FRSL_BYTES == 4u + FRSQ_NV + offsetof(settings_t, clock) + 1u - offsetof(settings_t, octave) &&
                   FRSK_BYTES == ((4u + FRSO_NV + offsetof(settings_t, anim) - offsetof(settings_t, octave) + 3u) & ~3u) &&
                   FRSJ_BYTES == ((4u + FRSL_NV + offsetof(settings_t, anim) - offsetof(settings_t, octave) + 3u) & ~3u) &&
                   offsetof(settings_t, bnote) == 192u + (NSOUND - FRSL_NV) &&
                   offsetof(settings_t, octave) ==
                   4u + NSOUND && offsetof(settings_t, bq_trans) + 1u - offsetof(settings_t, octave) ==
                   FRSH_BYTES - 4u - FRSH_NV && FRSH_BSEQ == 189u,
               "the settings: FRSH's (FRSG's: FRSF's 172 bytes, MORPH, LOAD and the learned CCs; 192 with its padding; "
               "then the bass's sequence), the sound three settings longer");
#if FRS_LEAD
#define P_AFTER_AMT P_LD_WAVE            /* (LEAD's settings after the FM-1's, the SMK-37's alone: in no record, never a byte's) */
#else
#define P_AFTER_AMT NPARAM
#endif
_Static_assert(NBQLEN == 9u && SC_BSEQ == 32u && SC_SEQ == SC_BSEQ && P_BQ_NLEN == P_CLOCK + 1 && P_GL_VIB_AUTO == 196 &&
                   P_DQ_ON == P_GL_VIB_AUTO + 1 && P_DQ_NOTES == P_DQ_STEP + 1 && P_COUNT_IN + 1 == P_ARP_SNAP &&
                   P_SAVER == P_ARP_SNAP + 1 && P_TP_FLUTTER == P_SAVER + 1 && P_TP_DROPOUT == P_TP_FLUTTER + 3 &&
                   P_L1_AMT1 == P_TP_DROPOUT + 1 && P_L2_AMT1 == P_L1_AMT1 + 4 && P_L3_AMT1 == P_L2_AMT1 + 4 &&
                   P_L3_AMT4 == P_L1_AMT1 + 11 && P_L3_AMT4 + 1 == P_AFTER_AMT && P_AFTER_AMT < 255,
               "the bass's sequence's settings (params.c PD: RATE's 9, STEP's and SEQ LENGTH's 32, LENGTH's 1 .. 32); "
               "LENGTH numbered last, then what came after it; the two sequences' steps alike (step_*)");

_Static_assert(sizeof(settings_t) == 300u && FRSP_BYTES == offsetof(settings_t, tp_flutter) &&
                   FRSQ_BYTES == offsetof(settings_t, lfo_amt) && offsetof(settings_t, swell) == FRSQ_BYTES + 12u &&
                   offsetof(settings_t, arp_snap) == 278u && offsetof(settings_t, saver_off) == 279u &&
                   offsetof(settings_t, head) == offsetof(settings_t, swell) + 1u &&
                   offsetof(settings_t, a4) == offsetof(settings_t, head) + 1u,
               "FRSP's 280 bytes (ARP SNAP and SCREENSAVER in its padding), LOFI TAPE II's four after them (FRSQ's 284), "
               "then the twelve AMOUNTs and SWELL, HEADER and TUNE A4 in the padding");
_Static_assert(sizeof raw >= sizeof(upresets_t) && sizeof raw >= SCN_BYTES && sizeof raw >= BSQ_BYTES && sizeof raw >= DSQ_BYTES && sizeof raw >= sizeof(settings_t) &&
                   sizeof raw >= 8u + NSCENES * FRSH_REC && sizeof raw >= NMS_BYTES, "raw holds the largest");
static uint32_t name_bytes(const layout_t *l) { return l->extra >= 3u ? SC_NAME : OLD_NAME; }
static uint32_t seq_bytes(const layout_t *l) { return l->extra >= 2u && l->extra != 5u ? SC_SEQ : 0u; }
static uint32_t scn_bytes(const layout_t *l)
{
    return name_bytes(l) + l->nv + 2u + SC_CHORDS * 5u + SC_ARP + (l->extra ? 1u : 0u) + seq_bytes(l);
}
static uint32_t set_bytes(const layout_t *l) { return (4u + l->nv + 5u + 2u * (l->extra ? 1u : 0u) + seq_bytes(l) + 3u) & ~3u; }
static uint32_t rd32(const uint8_t *b) { return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static uint32_t rec_sum(const uint8_t *b, uint32_t n)   /* a scene's record, summed (FNV-1a: OBJ_NAMES knows it by it) */
{
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

/* the FM-1's own state: its pages, the panel's buttons and encoders, the slots (the engine's: core.c st) */
static struct {
    uint32_t logo_until;                 /* at power-on: it gives way to the river by itself (0: stays)*/
    uint32_t hold_at[NB];                /* when each button went down */
    uint32_t hold_done;                  /* bit b: b's hold has acted, or b went with something (SEL and a key, SAVE
                                          * and a key or REC, LFO and an encoder, GLO and a CC, a page button and
                                          * SELECT): no tap on release */
    uint32_t hold_acted;                 /* bit b: b's hold has acted, this press (SELECT turned with it undoes it) */
    uint32_t keys_arm_at;                /* KEYS' LATCH turned OFF: the first detent, waiting for the second */
    uint32_t touched_at;
    uint32_t enc_at[NE];                 /* the last detent of each encoder (acceleration) */
    int8_t octave;                       /* -3 .. +2 */
    uint8_t logo;                        /* the logo's screen (HOME again, and at power-on) */
    uint8_t page;                        /* PG_* */
    uint8_t scene;                       /* the slot loaded or saved last, 0 .. 15 */
    uint8_t pick;                        /* SCENES: the slot K1 points at */
    uint8_t prev_page;                   /* the page SCENES came from (SAVE again goes back) */
    int8_t keys_arm;
    uint8_t help, help_page;             /* HOME held: the help on the screen until HOME again; SELECT its pages */
    uint8_t touched_enc;                 /* the encoder that turned it (EN_*: the screen's badge) */
    prm_t touched;                       /* the parameter turned last (the screen shows it a moment) */
    uint8_t store_hold;                  /* a backup streaming to USB: nothing saved until it ends (app_backup_hold) */
    uint8_t prefs_due;                   /* prefs_save asked: 1 once nothing sounds (prefs_quiet), 2 now (SPK/JACK) */
    uint8_t booting;                     /* app_init done, the UI not started yet (app_ui_init: the logo) */
    uint8_t metro_on;                    /* METRONOME's last on value (REC held comes back to it) */
    uint8_t edit_map;                    /* EDIT held: its three keys up */
    uint32_t oct_at;                     /* OCT+, OCT-: when, the octave large a moment */
    int8_t oct_v;
    int8_t oct_was;                      /* the octave before OCT- or OCT+ went down alone (the other with it: ANCHOR,
                                          * that step undone) */
    int8_t anc_oct;                      /* the octave ANCHOR keeps (back with it) */
    uint8_t anc_both;                    /* OCT- and OCT+ down together, ANCHOR's: till one is let go */
    uint8_t anc_arm;                     /* ... on an ANCHOR set: MORPH at their release, BACK held HOLD_MS */
    uint32_t anc_at;
} fm __attribute__((used));              /* (kept whole, as the engine's st is: the RAM as it was) */

static uint32_t keys_prev, btn_prev, btn_now;
static uint32_t flashes[2];              /* ARP's and SEQ's flashes (the tests) */
static uint8_t spk_page;                 /* a PERFORM page plays the FM-1's USB audio (COMPUTER AUDIO): its own output
                                          * quiet meanwhile, SPK/JACK as it is (the page can only quiet it) */

static int b_held(uint32_t b) { return btn_now >> b & 1u; }
static void b_used(uint32_t b) { fm.hold_done |= 1u << b; }   /* (its release no tap) */
static uint32_t leds_now, midleds_now, glowleds_now, keyleds_now, keymid_now;   /* the lights last set (the tests) */
static volatile uint8_t oct_soft;        /* OCT- and OCT+ in their blink now (app_tick lights them) */
static float pump_low = 1.0f;            /* the drones' lowest pump gain since the tests last asked */

/* the lights: the buttons lit, at the mid level and in the glow, the keys lit and in the glow. The glow is felucca-bsp's
 * DIM HI (~1/30 of lit); the mid level its steady one (lit one frame in eight, the glow between: ~15 %), which the
 * platform's plat_lights does not offer: on the FM-1 it is set here, in fm1_led_mid (the unity build has it in scope).
 * OCT's blink is softer still (app_tick): the glow and every other mid frame, ~9 % */
#if !defined PLAT_HOST && !PLAT_SMK37    /* (the SMK-37's lights: plat_lights alone, no key LEDs, no mid level) */
static uint8_t oct_ml[FM1_NCOL];         /* OCT- and OCT+ in fm1_led_mid's columns */
#endif
static void lights_set(uint32_t lit, uint32_t mid, uint32_t glow, uint32_t keys, uint32_t keys_glow, uint32_t keys_mid)
{
    leds_now = lit;
    midleds_now = mid;
    glowleds_now = glow;
    keyleds_now = keys | keys_glow | keys_mid;
    keymid_now = keys_mid & ~keys;
    plat_lights(lit, keys, mid | glow, keys_glow | keys_mid, 2);
#if !defined PLAT_HOST && !PLAT_SMK37
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
        for (i = 0; i < NKEYS; i++) {    /* (a key at the mid level, as the function buttons: its id 14 on, as
                                          * plat_lights has them) */
            uint8_t q = led_pos[14u + i];
            if (q != 0xFF && (keys_mid >> i & 1u) && !(keys >> i & 1u))
                ml[q >> 3] |= (uint8_t)(1u << (q & 7u));
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
#if !defined PLAT_HOST && !PLAT_SMK37
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

/* ---------------------------------------------------------- the screensaver --- */
/* SCREENSAVER (GLO > SCREEN's K2, ON by default): IDLE TIME (K3, 1 .. 60 minutes, ten at first) without a touch of the panel (a button, a knob, a key;
 * MIDI, a PERFORM page and the River are not touches: they play, the screen sleeps on), the screen black and the logo
 * drifting on it, a pixel a step on both axes, bouncing off the edges, its colour the next at each bounce (ui.c
 * saver_screen: only its old and new boxes drawn again). The sound, the sequences, MIDI, the clock and a take go on as
 * they were: the screen alone sleeps (it costs less than the river). The touch that wakes it only wakes it: the button
 * or key that did it is dropped till it is let go (no press, no tap, no hold, no note: a sleepy hand on a lit key does
 * not let a drone go), the knob that did it turns nothing till it has rested SAVER_GRACE_MS (SAVER_GRACE_MAX at most:
 * one turning on is heard). What was on the screen comes back as it was (a page, the help, a pop-up, a name). A pop-up
 * that comes meanwhile (a restore from the River) wakes it. The lights stay as they are (they tell what plays).
 * THE CORNER: each time it comes, the logo starts where (and goes the way) that brings it into a corner, both edges on
 * the same step, about a minute later (saver_aim); then it drifts on as before (the next corner some 12 minutes on).
 * Every corner is celebrated: CORNER! and ice crystals bursting from it, SAVER_PARTY_MS (ui.c saver_party), then the
 * drift again; a touch meanwhile wakes it as ever */
#include "frasil_saver.h"                /* SAVER_W x SAVER_H: the logo, smaller (tools/gen_logo.py) */
#define SAVER_MIN 10u                    /* IDLE TIME at first: ten minutes */
#define SAVER_STEP_MS 25u                /* the drift: a pixel a step on both axes, 40 steps a second */
#define SAVER_GRACE_MS 350u
#define SAVER_GRACE_MAX 1500u
#define SAVER_MAXX ((int32_t)(240u - SAVER_W))   /* the logo's top left: 0 .. these */
#define SAVER_MAXY ((int32_t)(240u - SAVER_H))
#define SAVER_AIM ((int32_t)(60000u / SAVER_STEP_MS))   /* the first corner: this many steps in (a minute), give or take */
#define SAVER_AIM_JIT ((int32_t)(1500u / SAVER_STEP_MS))   /* (+/- 1.5 s, the start never twice the same) */
#define SAVER_AIM_EDGE 24                /* a start this near two edges is a start in a corner: another */
static struct {
    uint32_t idle_at;                    /* the last touch (plat_ms) */
    uint32_t after;                      /* idle this long: IDLE TIME's (the tests' saveridle: less) */
    uint32_t woke_at, enc_until;         /* woken: when; the knobs' detents dropped till then */
    uint32_t btn_spent, keys_spent;      /* down when it woke: dropped till let go */
    uint32_t step_at;                    /* the drift's last step (ui.c) */
    uint32_t steps, bounces;             /* since it came (the tests) */
    uint32_t party_at;                   /* a corner's celebration: when it began */
    uint32_t min_at;                     /* IDLE TIME last turned (its cell lamp a moment, as a turned setting's) */
    uint32_t corners, corner_step;       /* corners since it came, the step of the first (the tests) */
    int16_t x, y;                        /* the logo's top left */
    int16_t aim;                         /* the step the first corner was aimed at (the tests) */
    int8_t dx, dy;
    uint8_t on, drawn, col;
    uint8_t party;                       /* celebrating a corner (ui.c saver_party) */
    uint8_t min;                         /* IDLE TIME, minutes (1 .. 60) */
} saver = {.min = SAVER_MIN};

/* GLO > SCREEN's K4, HEADER (no setting in the schema: settings_t.head): INVERT, the band in the track's colour and
 * everything on it dark (as it always was); DARK, the band dark and everything on it in the track's colour (ui.c) */
static struct {
    uint8_t dark;
    uint32_t at;                         /* last turned (its cell lamp a moment, as a turned setting's) */
} hdr;

/* GLO > RECORD's K3, TUNE A4 (no setting in the schema: settings_t.a4): the frequency every voice's A4 sounds at, 415 ..
 * 466 Hz a hertz a detent, 440 at first; the notes, MIDI and the scenes as they are, the pitch alone moves (the engine's
 * a4_l2, with TAPE SPEED). A quick turn stops at 432 and at 440 as it passes them */
#define A4_LO 415u
#define A4_HI 466u
static struct {
    uint16_t hz;
    uint32_t at;                         /* last turned (its cell lamp a moment) */
} a4 = {.hz = 440u};

static void a4_take(uint32_t hz)
{
    a4.hz = (uint16_t)(hz >= A4_LO && hz <= A4_HI ? hz : 440u);
    a4_l2 = a4.hz == 440u ? 0.0f : fm_log2f((float)a4.hz * (1.0f / 440.0f));   /* (440: 0 exactly, as it was) */
    st.engine_dirty = 1;
}

static int32_t a4_catch(int32_t was, int32_t v)   /* a turn from was to v: past 432 or 440, there (the nearer first) */
{
    int32_t a = v > was ? 432 : 440, b = v > was ? 440 : 432;
    if ((was < a && v > a) || (was > a && v < a))
        return a;
    return (was < b && v > b) || (was > b && v < b) ? b : v;
}

/* SCREENSAVER and IDLE TIME in the one byte FRSP had left (settings_t.saver_off), 3.0's two values as they were: 0 ON,
 * 1 OFF (ten minutes, both); 2 .. 61 ON after 1 .. 60 minutes, 62 .. 121 OFF with them (ten: always 0 or 1); above, a
 * later FRASIL's: ON, ten. (3.0 reads any byte but 0 as OFF: a 3.0.1 ON with another IDLE TIME, OFF there) */
static uint8_t saver_byte(void)
{
    uint32_t m = saver.min;
    return (uint8_t)(st.v[P_SAVER] ? (m == SAVER_MIN ? 0u : 1u + m) : (m == SAVER_MIN ? 1u : 61u + m));
}

static void saver_unbyte(uint32_t b)
{
    st.v[P_SAVER] = b == 0u || (b >= 2u && b <= 61u) || b > 121u;
    saver.min = (uint8_t)(b >= 2u && b <= 61u ? b - 1u : b >= 62u && b <= 121u ? b - 61u : SAVER_MIN);
}

/* a step of the drift: a pixel on both axes, an axis turned back at its edge (bit 0: x turned, bit 1: y) */
static int saver_move(int16_t *x, int16_t *y, int8_t *dx, int8_t *dy)
{
    int b = 0;
    if (*x + *dx < 0 || *x + *dx > SAVER_MAXX) {
        *dx = (int8_t)-*dx;
        b |= 1;
    }
    if (*y + *dy < 0 || *y + *dy > SAVER_MAXY) {
        *dy = (int8_t)-*dy;
        b |= 2;
    }
    *x = (int16_t)(*x + *dx);
    *y = (int16_t)(*y + *dy);
    return b;
}

/* the logo in a corner: on an edge both ways (the next step turns both) */
static int saver_cornered(int32_t x, int32_t y) { return (x == 0 || x == SAVER_MAXX) && (y == 0 || y == SAVER_MAXY); }

/* where an axis starts so as to be at edge c (0 or m) in n steps going d (+1, -1): n steps back from c, the bounces
 * unfolded (a triangle wave of period 2m); its way there in *v. 0: it would start on an edge (another start) */
static int saver_back(int32_t c, int32_t m, int32_t d, int32_t n, int16_t *p, int8_t *v)
{
    int32_t u = (c - d * n) % (2 * m);
    if (u < 0)
        u += 2 * m;
    if (u == 0 || u == m)
        return 0;
    *p = (int16_t)(u < m ? u : 2 * m - u);
    *v = (int8_t)(u < m ? d : -d);
    return 1;
}

/* the start, from r: a corner, the way in, a step count about SAVER_AIM; run back from the corner. Each axis is on an
 * edge every m steps (128 across, 219 down: no factor shared), so both on the same step again only 128 x 219 = 28032
 * steps on: the aimed corner is the first. A start on an edge, or near two (a corner), is drawn again */
static int saver_aim(uint32_t r, int16_t *x, int16_t *y, int8_t *dx, int8_t *dy)
{
    uint32_t k;
    for (k = 0; k < 64u; k++, r = r * 1664525u + 1013904223u) {
        int32_t n = SAVER_AIM - SAVER_AIM_JIT + (int32_t)((r >> 16) % (2u * SAVER_AIM_JIT + 1u));
        int32_t ddx = (r & 1u) ? 1 : -1, ddy = (r & 2u) ? 1 : -1;
        if (!saver_back((r & 4u) ? SAVER_MAXX : 0, SAVER_MAXX, ddx, n, x, dx) ||
            !saver_back((r & 8u) ? SAVER_MAXY : 0, SAVER_MAXY, ddy, n, y, dy))
            continue;
        if ((*x < SAVER_AIM_EDGE || *x > SAVER_MAXX - SAVER_AIM_EDGE) &&
            (*y < SAVER_AIM_EDGE || *y > SAVER_MAXY - SAVER_AIM_EDGE))
            continue;
        return (int)n;
    }
    saver_back(0, SAVER_MAXX, -1, SAVER_AIM, x, dx);   /* (never: 64 draws all refused) the top left's */
    saver_back(0, SAVER_MAXY, -1, SAVER_AIM, y, dy);
    return SAVER_AIM;
}
#if defined PLAT_HOST && !defined PLAT_WEB
static uint32_t saver_out;               /* (the tests: steps that left the logo outside the screen) */
static uint32_t saver_seed;              /* (the tests: saverseed, the next start's draw; 0: plat_ms's) */
static struct {
    uint32_t n[3], ms[3], ticks[3], tmax[3];   /* the UI's frames, awake [0], asleep [1], a corner's [2]: how many,
                                                * over what, their time, the longest */
    uint32_t draws, px;                  /* the saver's steps drawn, the pixels they sent */
    uint32_t pframes, prun, ppx, pmax;   /* a corner's frames drawn, their runs (blits), pixels, the most in one */
} ui_cost;                               /* (uicost) */
#endif

static void saver_wake(void)
{
    uint32_t now = plat_ms();
    saver.on = saver.party = 0;
    saver.idle_at = saver.woke_at = now;
    saver.enc_until = now + SAVER_GRACE_MS;
}

static void saver_touch(void) { saver.idle_at = plat_ms(); }

/* the buttons read (first in input): those that woke it dropped till let go; any down, a touch */
static uint32_t saver_buttons(uint32_t btn)
{
    saver.btn_spent &= btn;
    btn &= ~saver.btn_spent;
    if (btn && saver.on) {
        saver_wake();
        saver.btn_spent |= btn;
        return 0;
    }
    if (btn)
        saver_touch();
    return btn;
}

/* the keys read (keys_scan): the same */
static uint32_t saver_keys(uint32_t keys)
{
    saver.keys_spent &= keys;
    keys &= ~saver.keys_spent;
    if (keys && saver.on) {
        saver_wake();
        saver.keys_spent |= keys;
        return 0;
    }
    if (keys)
        saver_touch();
    return keys;
}

static uint32_t welcome_buttons(uint32_t btn);   /* (Learn's welcome closed as the screensaver wakes: below, learn_tick) */
static uint32_t welcome_keys(uint32_t keys);
static int welcome_knob(void);

/* an encoder's detents (every read of them goes through here): one wakes it; the knob that did, dropped while it
 * turns on (SAVER_GRACE_MS of rest, SAVER_GRACE_MAX at most), and any knob that soon after a wake */
static int32_t enc_read(int e)
{
    int32_t d = plat_enc(e);
    uint32_t now, cap;
    if (!d)
        return 0;
    now = plat_ms();
    if (saver.on)
        saver_wake();
    if ((int32_t)(saver.enc_until - now) > 0) {
        cap = saver.woke_at + SAVER_GRACE_MAX;
        saver.enc_until = (int32_t)(cap - (now + SAVER_GRACE_MS)) < 0 ? cap : now + SAVER_GRACE_MS;
        saver.idle_at = now;
        return 0;
    }
    saver_touch();
    return d;
}

/* each UI frame: ON and idle long enough, it comes; OFF (a restore, a PERFORM page's SET), it goes. Never in the
 * browser's FM-1 (the River's, the editor's: no screen to keep, and they play it from the page, no touch) */
static void saver_tick(void)
{
    uint32_t now = plat_ms(), r;
#ifdef PLAT_WEB
    if (now)
        return;
#endif
    if (saver.on && !st.v[P_SAVER])
        saver_wake();
    if (saver.on || !st.v[P_SAVER] || now - saver.idle_at < (saver.after ? saver.after : saver.min * 60000u))
        return;
    r = now * 2654435761u;               /* (where it starts and which way: toward a corner a minute on) */
#if defined PLAT_HOST && !defined PLAT_WEB
    r = saver_seed ? saver_seed : r;
#endif
    saver.on = 1;
    saver.drawn = 0;
    saver.steps = saver.bounces = 0;
    saver.corners = saver.corner_step = 0;
    saver.party = 0;
    saver.col = 0;
    saver.aim = (int16_t)saver_aim(r ^ (r >> 15), &saver.x, &saver.y, &saver.dx, &saver.dy);
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

static uint32_t spk_turned;              /* the encoders turned since the last UI frame, bit EN_* (speak.c: the readouts) */
static void spk_popup(void);             /* (the screen reader's, speak.c) */
static void spk_msg(const char *a, const char *b, uint32_t flags);

static void popup(int kind, const char *kicker, const char *title, const char *l1, const char *l2, const char *l3)
{
    pop_copy(pop.kicker, kicker, sizeof pop.kicker);
    pop_copy(pop.title, title, sizeof pop.title);
    pop_copy(pop.line[0], l1, sizeof pop.line[0]);
    pop_copy(pop.line[1], l2, sizeof pop.line[1]);
    pop_copy(pop.line[2], l3, sizeof pop.line[2]);
    pop.kind = (uint8_t)kind;
    pop.on = 1;
    if (saver.on) {                      /* (it wants the eyes: the screen wakes, nothing dropped) */
        saver.on = saver.party = 0;
        saver_touch();
    }
    pop.drawn = 0;
    pop.btn = plat_buttons();            /* (SEL already down closes it only pressed again) */
    fm.hold_done = ~0u;                  /* the buttons down now: no tap, no hold */
    spk_popup();                         /* and to a screen reader: a warning heard whole (speak.c) */
}

/* a pop-up up, a frame's input: SEL pressed closes it (its release and its hold nothing); the encoders' turns
 * dropped. 1: still up (the buttons, for the rest of the frame, none) */
static int popup_input(uint32_t btn)
{
    uint32_t down = btn & ~pop.btn, e;
    pop.btn = btn;
    for (e = 0; e < NE; e++)
        enc_read((int)e);
    if (!(down >> B_SEL & 1u))
        return 1;
    pop.on = 0;
    btn_prev = btn;                      /* (what is down now: used, no tap on its release) */
    fm.hold_done = ~0u;
    return 0;
}
/* ARP and SEQ, each track's, over its PLAYS: on, its pages show; off, its first alone, its ON/OFF alone */
static int ways_on(uint32_t track, uint32_t way)
{
    return track == 0u ? way == SQT_SEQ && st.v[P_DQ_ON] : st.v[track == 1u ? P_SQ_TYPE : P_BQ_ON] == way;
}

static int page_off(uint32_t p)          /* a page that waits for its ARP or its SEQ */
{
    return (p == PG_ARP2 && !ways_on(1u, SQT_ARP)) || (p == PG_BARP2 && !ways_on(2u, SQT_ARP)) ||
           ((p == PG_SEQ2 || p == PG_SEQ3) && !ways_on(1u, SQT_SEQ)) ||
           ((p == PG_BSEQ2 || p == PG_BSEQ3) && !ways_on(2u, SQT_SEQ)) ||
           ((p == PG_DSEQ2 || p == PG_DSEQ3) && !ways_on(0u, SQT_SEQ)) || (p == PG_TAPE2 && !st.v[P_TP_ON]);
}

/* a button's pages: the selected track's, and those every track has; the flow's two SAVE pages, its own; LOFI TAPE II
 * only while TAPE is on (skipped, not greyed, when the pages are stepped through) */
static int page_hidden(uint32_t p)
{
    return (PAGES[p].track != T_ANY && PAGES[p].track != st.v[P_KB]) || page_off(p) || p == PG_SAVE || p == PG_SAVEAS;
}

/* the setting under knob i of page p. KEYS: the selected track's. A first page of ARP or SEQ turned OFF: its ON/OFF alone.
 * RECORD on LIVE a track's steps page is no step editor: what a take asks for (LIVE_K) */
static const prm_t LIVE_K[3][4] = {{P_DQ_LEN, P_DQ_RATE, P_DQ_UNDO, P_KY_DR_STACK},
                                     {P_SQ_LEN, P_ARP_RATE, P_SQ_UNDO, NPARAM},
                                     {P_BQ_STEPS, P_BQ_LEN, P_BQ_UNDO, NPARAM}};

static prm_t page_knob(uint32_t p, uint32_t i)
{
#if FRS_LEAD
    if (p == PG_KEYS && st.lead)         /* (LEAD: one note as played, no KEYS of its own) */
        return NPARAM;
#endif
    if (p == PG_KEYS)                    /* (GLINT's LATCH none: its keys sound while down, card #143) */
        return st.v[P_KB] == 1u && i == 2u ? (prm_t)NPARAM : (prm_t)(P_KY_DR_HARM + 4u * st.v[P_KB] + i);
    if (i && ((p == PG_ARP && !ways_on(1u, SQT_ARP)) || (p == PG_BARP && !ways_on(2u, SQT_ARP)) ||
              (p == PG_SEQ && !ways_on(1u, SQT_SEQ)) || (p == PG_BSEQ && !ways_on(2u, SQT_SEQ)) ||
              (p == PG_DSEQ && !ways_on(0u, SQT_SEQ))))
        return NPARAM;
    if (p == PG_DSEQ2 && st.v[P_DQ_REC] == 2u)
        return LIVE_K[0][i & 3u];
    if (p == PG_SEQ2 && st.v[P_SQ_REC] == 2u)
        return LIVE_K[1][i & 3u];
    if (p == PG_BSEQ2 && st.v[P_BQ_REC] == 2u)
        return LIVE_K[2][i & 3u];
    return PAGES[p].p[i];
}

static void page_back(void)              /* the page gone with its ARP or SEQ: that button's first of the track */
{
    if (page_off(fm.page))
        fm.page = fm.page == PG_ARP2 ? PG_ARP : fm.page == PG_BARP2 ? PG_BARP : fm.page <= PG_SEQ3 ? PG_SEQ
                  : fm.page == PG_BSEQ2 || fm.page == PG_BSEQ3 ? PG_BSEQ : PG_DSEQ;
}

static int flow_store(void);             /* (the SAVE flow, below) */
static int rec_any(void) { return st.v[P_SQ_REC] || st.v[P_BQ_REC] || st.v[P_DQ_REC]; }

#if defined PLAT_HOST && !defined PLAT_WEB
static uint64_t render_hash = 0xCBF29CE484222325u;   /* (the tests: every sample rendered, FNV-1a: "renderhash") */
static uint64_t render_frames;
static int host_slow_us = -1;            /* FRASIL_HOST_SLOW=US: one block's render in 32 US longer (6000, over its 5.8 ms,
                                          * for 1 or any value under 100), a loaded Mac's spikes without loading this
                                          * one (card #85) */
#define PZ_N (4u * 44100u)
static struct {                          /* (the tests: the output's pitch, "pitch HZ" on: its left side kept, up to four
                                          * seconds, then the frequency within 100 cents of HZ its spectrum peaks at,
                                          * a Hann window on it: expect pitchhz, pitchcents) */
    float x[PZ_N];
    uint32_t n;
    uint8_t on;
    double near, hz, ref;                /* (hz: found, 0 not yet) */
} pz;

static struct {                          /* (the tests: a step at the blocks' edges, "edges" on: the output's second
                                          * difference squared at each block's first samples against its middle's,
                                          * expect edgedb: a setting's glide stepped once a block clicks there, as
                                          * MASTER EQ's HIGH did before 3.1: eq_turn.txt) */
    double edge, mid;
    float p[3];                          /* (the block before's last three, left and right summed) */
    uint8_t on;                          /* (1: its first block only kept, 2: measuring) */
} eg;

static double pz_at(double f)            /* the spectrum's magnitude at f, squared (the samples windowed already) */
{
    double w = 2.0 * 3.14159265358979 * f / (double)FRASIL_FS, c = cos(w), sn = sin(w), zr = 1.0, zi = 0.0, re = 0.0,
           im = 0.0, t;
    uint32_t i;
    for (i = 0; i < pz.n; i++) {         /* (z: e^-jwi, turned a sample at a time) */
        re += pz.x[i] * zr;
        im += pz.x[i] * zi;
        t = zr * c + zi * sn;
        zi = zi * c - zr * sn;
        zr = t;
    }
    return re * re + im * im;
}

static double pz_pitch(void)             /* the window on, then a cent at a time, then a twentieth around the best */
{
    double best = 0.0, bm = -1.0, c, lo;
    uint32_t i;
    if (pz.hz > 0.0 || pz.n < 4410u || pz.near <= 0.0)
        return pz.hz;
    pz.on = 0;
    for (i = 0; i < pz.n; i++)
        pz.x[i] *= (float)(0.5 - 0.5 * cos(2.0 * 3.14159265358979 * (double)i / (double)pz.n));
    for (c = -100.0; c <= 100.0; c += 1.0) {
        double m = pz_at(pz.near * pow(2.0, c / 1200.0));
        if (m > bm)
            bm = m, best = c;
    }
    for (lo = best - 1.0, c = lo; c <= lo + 2.0; c += 0.05) {
        double m = pz_at(pz.near * pow(2.0, c / 1200.0));
        if (m > bm)
            bm = m, best = c;
    }
    return pz.hz = pz.near * pow(2.0, best / 1200.0);
}
#endif
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    frasil_render(lr, n, (float)master_q12 * (1.0f / 4096.0f));
#if defined PLAT_HOST && !defined PLAT_WEB
    {
        uint32_t i;
        if (host_slow_us < 0) {
            const char *s = getenv("FRASIL_HOST_SLOW");
            host_slow_us = !s || !*s ? 0 : atoi(s) >= 100 ? atoi(s) : 6000;
        }
        if (host_slow_us && !(render_frames / n & 31u)) {
            struct timespec t = {host_slow_us / 1000000, host_slow_us % 1000000 * 1000L};
            nanosleep(&t, 0);
        }
        for (i = 0; i < 2u * n; i++) {
            render_hash ^= (uint32_t)lr[i];
            render_hash *= 0x100000001B3u;
        }
        for (i = 0; pz.on && i < n && pz.n < PZ_N; i++)
            pz.x[pz.n++] = (float)lr[2u * i];
        if (eg.on && n >= 8u) {
            float s[6];
            uint32_t h = n / 2u - 3u;
            for (i = 0; i < 3u; i++) {
                s[i] = eg.p[i];
                s[3u + i] = (float)lr[2u * i] + (float)lr[2u * i + 1u];
            }
            for (i = 1; eg.on == 2 && i < 5u; i++) {
                double d = (double)s[i - 1u] - 2.0 * (double)s[i] + (double)s[i + 1u],
                       e = (double)lr[2u * (h + i - 1u)] + lr[2u * (h + i - 1u) + 1u] -
                           2.0 * ((double)lr[2u * (h + i)] + lr[2u * (h + i) + 1u]) + lr[2u * (h + i + 1u)] +
                           lr[2u * (h + i + 1u) + 1u];
                eg.edge += d * d;
                eg.mid += e * e;
            }
            for (i = 0; i < 3u; i++)
                eg.p[i] = (float)lr[2u * (n - 3u + i)] + (float)lr[2u * (n - 3u + i) + 1u];
            eg.on = 2;
        }
        render_frames += n;
    }
#endif
}

void app_panic(void) { frasil_panic(); }

/* the render ran over 90 % of its time (audio interrupt), or the load is over 82 % (load_watch, ahead of it): one step
 * lighter at once, and the header's dot for 3 s. The
 * main loop gives a step back after a calm (no overrun, the load at most 75 %): 4 s at first, twice as long each time
 * a step given back ran over again within 15 s (up to 64 s), 4 s again after a minute without one. (2.5-b8, the user:
 * a heavy scene glitched every few seconds, a step given back and taken again, the dot coming and going) */
#if defined PLAT_HOST && !defined PLAT_WEB
static uint32_t host_load;               /* the simulator's load: its script's ("cpuload N", 0 at first), never the
                                          * Mac's time, or a loaded Mac shed the sound and the tests' values changed from
                                          * pass to pass (card #85); the FM-1 and the web page measure theirs */
#define cpu_load() host_load
#else
#define cpu_load() plat_cpu_pct()
#endif
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

/* the header's battery (ui.c batt_draw, #125): the divider's reading (plat_battery_raw, read a second apart) in millivolts
 * (APP_BATT_RAW_4V2: what it reads at 4.20 V), smoothed over about half a minute (a chord's load lowers it a little,
 * USB raises it: neither makes the colour jump), then a percent along a Li-ion cell's curve. The level drawn moves
 * only once the reading has gone 2 % from it; its colour (green; yellow at 30 % and under; red at 10 % and under)
 * stays until the level is 3 % back past the line it crossed. No reading (the browser; the simulator until its script
 * gives one) or one of nothing (no divider): no battery, and the next reading starts it again, unsmoothed; its first
 * ten seconds followed quicker (the first reading comes as the sound and Bluetooth start, the cell at its lowest) */
static const int16_t BATT_MV[] = {3400, 3500, 3600, 3650, 3700, 3800, 3900, 4000, 4100, 4200};
static const int8_t BATT_PCT[] = {0, 4, 10, 18, 28, 47, 64, 79, 90, 100};
static struct {
    int32_t mv16;                        /* the millivolts smoothed, x 16; 0: none yet */
    int32_t raw;                         /* the last reading (-1 none) */
    uint32_t at;                         /* when (plat_ms | 1; 0: never) */
    int8_t pct;                          /* the level drawn, 0..100; -1: no battery */
    uint8_t zone;                        /* its colour: 0 green, 1 yellow, 2 red */
    uint8_t n;                           /* the readings since it started, up to ten */
} batt = {0, -1, 0, -1, 0, 0};

static int batt_curve(int32_t mv)       /* a Li-ion cell's level at mv, 0..100 */
{
    uint32_t i;
    if (mv <= BATT_MV[0])
        return 0;
    for (i = 1; i < sizeof BATT_MV / sizeof BATT_MV[0]; i++)
        if (mv < BATT_MV[i])
            return BATT_PCT[i - 1] +
                   (int)((mv - BATT_MV[i - 1]) * (BATT_PCT[i] - BATT_PCT[i - 1]) / (BATT_MV[i] - BATT_MV[i - 1]));
    return 100;
}

static uint8_t batt_zone(int p, uint8_t z)   /* level p's colour, z the one drawn: down at once, up 3 % past the line */
{
    if (p <= 10 || (z == 2u && p < 13))
        return 2u;
    if (p <= 30 || (z && p < 33))
        return 1u;
    return 0;
}

static void batt_tick(void)             /* each UI frame: a reading a second */
{
    uint32_t now = plat_ms();
    int32_t mv;
    int p;
    if (batt.at && now - batt.at < 1000u)
        return;
    batt.at = now | 1u;
    batt.raw = plat_battery_raw();       /* (0: not read yet, -1 here as before) */
    if (batt.raw <= 0)
        batt.raw = -1;
    if (batt.raw < 16) {                 /* (none, or nothing on the divider) */
        batt.mv16 = 0;
        batt.pct = -1;
        batt.n = 0;
        return;
    }
    mv = batt.raw * 4200 / APP_BATT_RAW_4V2;
    batt.mv16 = batt.mv16 ? batt.mv16 + (mv * 16 - batt.mv16) / (batt.n < 10u ? 4 : 32) : mv * 16;
    batt.n += batt.n < 10u;
    p = batt_curve(batt.mv16 / 16);
    if (batt.pct < 0 || p >= batt.pct + 2 || p <= batt.pct - 2) {
        batt.zone = batt_zone(p, batt.pct < 0 ? 0u : batt.zone);
        batt.pct = (int8_t)p;
    }
}

static void load_watch(void)
{
    static uint32_t calm_since, seen, back_at, calm_ms = 4000u, ahead_at;
    uint32_t now = plat_ms(), at = over_at;
    if (cpu_load() > 82u && frasil_shed_level() < FR_SHED_TOP && now - ahead_at > 1000u) {   /* the load near the
                                                                                                 * top: a step lighter
                                                                                                 * before a render runs
                                                                                                 * over (the steps glide:
                                                                                                 * not heard), one a
                                                                                                 * second */
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
    if (!frasil_shed_level() || cpu_load() > 75u || now - at < 4000u) {
        calm_since = now;
        return;
    }
    if (now - calm_since > calm_ms) {
        frasil_shed(frasil_shed_level() - 1);
        calm_since = back_at = now;
    }
}

static void prefs_later(void);
static void scene_get(uint32_t s, scene_t *out)   /* slot s, or NEW SHARD (SC_NEW) */
{
    if (s >= NSCENES)
        new_shard(out);
    else if (scn.used >> s & 1u)
        *out = scn.s[s];
    else
        factory_get(s, out);
}

/* slot s's name as text, t SC_LONG + 1 long: a saved scene's first SC_NAME, then the rest (name_more); a factory scene's,
 * an empty slot's, NEW SHARD's (SC_NEW) as scene_get makes them */
static char *scene_name(uint32_t s, char *t)
{
    uint32_t i, n;
    if (s < NSCENES && (scn.used >> s & 1u)) {
        for (n = 0; n < SC_NAME && scn.s[s].name[n]; n++)
            t[n] = scn.s[s].name[n];
        for (i = 0; n == SC_NAME + i && i < SC_MORE && scn.s[s].name_more[i]; i++)
            t[n++] = scn.s[s].name_more[i];
        t[n] = 0;
        return t;
    }
    {
        scene_t sc;
        scene_get(s, &sc);
        return name_text(&sc, t);
    }
}

/* t (NUL-ended, SC_LONG at most) as slot s's name: its first SC_NAME in the scene's name, the rest in its name_more */
static void name_put(uint32_t s, const char *t)
{
    uint32_t i, end = 0;
    for (i = 0; i < SC_LONG; i++) {
        end = end || !t[i];
        if (i < SC_NAME)
            scn.s[s].name[i] = end ? 0 : t[i];
        else
            scn.s[s].name_more[i - SC_NAME] = end ? 0 : t[i];
    }
}

#if FRS_LEAD
/* each scene's LEAD (card #144, admin #605), the SMK-37's: its own object (OBJ_LEAD), the scene record as it was (the
 * FM-1's, the River's, the iPad's, Perform's: byte for byte). FRLD: u32 "FRLD", u8 its version (3), u8 its slots
 * (NSCENES), u8 an entry's bytes, u8 its settings; then each slot's entry: u8 1 saved with a LEAD (0 none), then LEAD's
 * settings as the knobs have them, P_LD_WAVE .. P_LD_FRELEASE (its MIXER level and PRESET with them, the filter's ADSR
 * last: NLDS). Read and written whole, a slot changed at a time (on the stack: no copy of them all in RAM). A slot
 * without one (saved before, a factory scene, NEW SHARD, an unknown layout): MOSS, LEAD's defaults; the same scene
 * sounds the same.
 * The player's LEAD presets (admin #628) after the scenes' entries, the same object (a build before them reads the
 * scenes' part alone, LDP_BYTES, and writes it back so: their presets gone, the scenes kept): u8 'U', u8 its presets
 * (NUP), u8 a preset's bytes, u8 its settings (NLDS); then each preset: u8 1 saved (0 none, those after it none too),
 * its name (SC_NAME, 0 after it), LEAD's settings as the knobs have them (a scene entry's: its MIXER level in it,
 * never taken). PRESET's list has them after the factory ones; read on the stack when wanted (no copy in RAM) */
#define LDP_MAGIC 0x444C5246u            /* "FRLD" */
#define LDP_VER 4u                       /* (4 and 3 the filter's ADSR in; 2, 1 without it: read with the amp's in its
                                          * place, as they sounded; PRESET as stored in each, a number past the presets
                                          * CUSTOM: eng_lead_take, admin #870, #875) */
#define LDP_HEAD 8u
#define LDP_ENTRY (1u + NLDS)
#define LDP_BYTES (LDP_HEAD + NSCENES * LDP_ENTRY)
#define LDP_NLDS2 (P_LD_DRIVE - P_LD_WAVE + 1)   /* (versions 1 and 2: their settings, their entry, their bytes) */
#define LDP_ENTRY2 (1u + LDP_NLDS2)
#define LDP_BYTES2 (LDP_HEAD + NSCENES * LDP_ENTRY2)
#define LDU_HEAD 4u                      /* (the player's part: its head, a preset's bytes, the whole object's) */
#define LDU_ENTRY (1u + SC_NAME + NLDS)
#define LDP_ALL (LDP_BYTES + LDU_HEAD + NUP * LDU_ENTRY)
_Static_assert(LDP_ALL <= PLAT_STORE_MAX && LDU_ENTRY <= 255u && P_MX_LEAD > P_LD_WAVE && P_MX_LEAD < P_LD_FRELEASE,
               "LEAD's scenes and the player's LEAD presets in one object, its MIXER level among their settings");
_Static_assert(LDP_BYTES <= PLAT_STORE_MAX && NSCENES <= 255u && P_LD_FATTACK == P_LD_DRIVE + 1 &&
                   P_LD_FRELEASE == P_LD_FATTACK + 3 && P_LD_RELEASE == P_LD_ATTACK + 3,
               "LEAD's scenes in one object, the filter's ADSR after the rest, each four in a row");
static void ldu_none(uint8_t *b)         /* the player's part, none of theirs (an object without it: a build's before) */
{
    uint8_t *u = b + LDP_BYTES;
    memset(u, 0, LDP_ALL - LDP_BYTES);
    u[0] = 'U';
    u[1] = NUP;
    u[2] = LDU_ENTRY;
    u[3] = NLDS;
}
static int ldp_read(uint8_t *b)          /* the object into b (LDP_ALL): 1 this layout's (earlier versions made over:
                                          * 1 and 2 each entry the filter's ADSR its amp's; and so written at the next
                                          * STORE; without the player's part, none of theirs) */
{
    uint32_t s, k;
    int n = plat_store_load(OBJ_LEAD, b, LDP_ALL), v;
    if (n < (int)LDP_HEAD || ((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24) !=
                                 LDP_MAGIC || b[5] != NSCENES)
        return 0;
    v = b[4];
    if ((v == (int)LDP_VER || v == 3) && (n == (int)LDP_BYTES || n == (int)LDP_ALL) && b[6] == LDP_ENTRY &&
        b[7] == NLDS) {
        if (n != (int)LDP_ALL || v == 3 || b[LDP_BYTES] != 'U' || b[LDP_BYTES + 1u] != NUP ||
            b[LDP_BYTES + 2u] != LDU_ENTRY || b[LDP_BYTES + 3u] != NLDS)
            ldu_none(b);
        b[4] = LDP_VER;
        return 1;
    }
    if ((v != 1 && v != 2) || n != (int)LDP_BYTES2 || b[6] != LDP_ENTRY2 || b[7] != LDP_NLDS2)
        return 0;
    for (s = NSCENES; s-- > 0;) {        /* (the last first: each entry further on than it was) */
        uint8_t *e = b + LDP_HEAD + s * LDP_ENTRY;
        const uint8_t *o = b + LDP_HEAD + s * LDP_ENTRY2;
        for (k = LDP_ENTRY2; k-- > 0;)   /* (its last byte first: the two overlap; the FM-1's libc has no memmove) */
            e[k] = o[k];
        for (k = 0; k < 4u; k++)
            e[1u + P_LD_FATTACK - P_LD_WAVE + k] = e[1u + P_LD_ATTACK - P_LD_WAVE + k];
    }
    b[4] = LDP_VER;
    b[6] = LDP_ENTRY;
    b[7] = NLDS;
    ldu_none(b);
    return 1;
}
static void ldp_fresh(uint8_t *b)        /* none yet, or an earlier layout: every slot none, none of the player's */
{
    memset(b, 0, LDP_BYTES);
    b[0] = (uint8_t)LDP_MAGIC;
    b[1] = (uint8_t)(LDP_MAGIC >> 8);
    b[2] = (uint8_t)(LDP_MAGIC >> 16);
    b[3] = (uint8_t)(LDP_MAGIC >> 24);
    b[4] = LDP_VER;
    b[5] = NSCENES;
    b[6] = LDP_ENTRY;
    b[7] = NLDS;
    ldu_none(b);
}
static void ldp_load(uint32_t s)         /* slot s's LEAD into the settings (none: MOSS), as the scene's (saved_ld: the
                                          * engine's eng_lead_take, a shard's LEAD's way there: card #152) */
{
    uint8_t b[LDP_ALL], m[NLDS];
    const uint8_t *e = 0;
    if (s < NSCENES && ldp_read(b) && b[LDP_HEAD + s * LDP_ENTRY] == 1u)
        e = b + LDP_HEAD + s * LDP_ENTRY + 1u;
    if (!e)
        eng_lead_none(m);
    eng_lead_take(e ? e : m);            /* (each past its max its default) */
}
static int ldp_save(uint32_t s)          /* the settings' LEAD into slot s's entry: 0 written */
{
    uint8_t b[LDP_ALL];
    uint8_t *e = b + LDP_HEAD + s * LDP_ENTRY;
    if (s >= NSCENES)
        return -1;
    if (!ldp_read(b))                    /* (none yet, or an earlier layout: every other slot none) */
        ldp_fresh(b);
    e[0] = 1;
    memcpy(e + 1, &st.v[P_LD_WAVE], NLDS);
    return plat_store_save(OBJ_LEAD, b, LDP_ALL);
}

/* the player's LEAD presets (admin #628): OBJ_LEAD's own part, as the others' presets of the player's (up) are theirs */
static const uint8_t *ldu_at(const uint8_t *b, uint32_t i) { return b + LDP_BYTES + LDU_HEAD + i * LDU_ENTRY; }
static uint32_t ldu_count(const uint8_t *b)
{
    uint32_t n = 0;
    while (n < NUP && ldu_at(b, n)[0] == 1u)
        n++;
    return n;
}
static uint32_t ldu_n(void)              /* how many */
{
    uint8_t b[LDP_ALL];
    return ldp_read(b) ? ldu_count(b) : 0u;
}
static void ldu_name_of(const uint8_t *b, uint32_t i, char *o)   /* its name (SC_NAME + 1 at most) */
{
    const uint8_t *e = ldu_at(b, i) + 1u;
    uint32_t k;
    for (k = 0; k < SC_NAME && e[k]; k++)
        o[k] = (char)e[k];
    o[k] = 0;
}
static void ldu_name(uint32_t i, char *o)
{
    uint8_t b[LDP_ALL];
    o[0] = 0;
    if (i < NUP && ldp_read(b))
        ldu_name_of(b, i, o);
}
static int ldu_in_use(void)              /* the one taken or stored last, as the LEAD sounds now (its level aside); -1 */
{
    uint8_t b[LDP_ALL];
    const uint8_t *v;
    uint32_t i;
    int c = up_cur[3];
    if (c < 0 || !ldp_read(b) || ldu_at(b, (uint32_t)c)[0] != 1u)
        return -1;
    v = ldu_at(b, (uint32_t)c) + 1u + SC_NAME;
    for (i = 0; i < NLDS; i++)
        if (P_LD_WAVE + i != P_MX_LEAD && v[i] != st.v[P_LD_WAVE + i])
            return -1;
    return c;
}
static void ldu_take(uint32_t i)         /* preset i into LEAD's settings, every one but its MIXER level (MODE turned:
                                          * the voices let go, as a factory one) */
{
    uint8_t b[LDP_ALL];
    const uint8_t *v;
    uint32_t k;
    if (i >= NUP || !ldp_read(b) || ldu_at(b, i)[0] != 1u)
        return;
    v = ldu_at(b, i) + 1u + SC_NAME;
    for (k = 0; k < NLDS; k++) {
        uint32_t p = P_LD_WAVE + k;
        if (p != P_MX_LEAD && p != P_LD_PRESET)
            param_set((int)p, lead_in(p, v[k]));
    }
    k = P_LD_PRESET - P_LD_WAVE;         /* (its PRESET the one it was made from, not applied: the cell's name; one
                                          * past the presets CUSTOM) */
    st.v[P_LD_PRESET] = lead_in(P_LD_PRESET, v[k]);
    st.dirty = (uint8_t)sound_changed();
    lv_lead(0u);                         /* (that PRESET in place: the LEAD as it is, card #152) */
    up_cur[3] = (int8_t)i;
}
static int ldu_save(uint32_t at, const char *name)   /* LEAD's settings as they are into preset at, so named: 0 written */
{
    uint8_t b[LDP_ALL], *e;
    uint32_t k, end = 0;
    if (at >= NUP)
        return -1;
    if (!ldp_read(b))
        ldp_fresh(b);
    e = b + LDP_BYTES + LDU_HEAD + at * LDU_ENTRY;
    e[0] = 1;
    for (k = 0; k < SC_NAME; k++) {
        end = end || !name[k];
        e[1u + k] = end ? 0 : (uint8_t)name[k];
    }
    memcpy(e + 1u + SC_NAME, &st.v[P_LD_WAVE], NLDS);
    return plat_store_save(OBJ_LEAD, b, LDP_ALL);
}
#endif

static void scene_load(uint32_t s)
{
    scene_t sc;
    scene_get(s, &sc);
    fm.scene = (uint8_t)s;
    scene_take(&sc);
    lv_long(FST_SCENE_NOW, s);
    up_cur[0] = up_cur[1] = up_cur[2] = -1;
}

#ifdef PLAT_HOST
static uint8_t test_keys_stay;           /* (basic.txt's older parts: KEYS as they set them, whatever is loaded: keysstay) */
static uint8_t test_step_moves;          /* (their RECORD STEP moving STEP on, the engine's other way: stepmoves) */
static uint8_t test_rec_anywhere;        /* (their takes set from any page: recanywhere) */
#else
#define test_keys_stay 0
#define test_step_moves 0
#define test_rec_anywhere 0
#endif
/* a slot, as SCENES' MORPH and LOAD have it (eng_scene_go) */
static void scene_go(uint32_t s)
{
    scene_t sc;
    scene_get(s, &sc);
    fm.scene = (uint8_t)s;
    eng_scene_go(&sc);
#if FRS_LEAD
    ldp_load(s);                         /* (its LEAD: its own object's; none, MOSS) */
#endif
    lv_long(FST_SCENE, s);
    lv_lead(1u);                         /* (its LEAD after it, as the scene's: card #152) */
    up_cur[0] = up_cur[1] = up_cur[2] = -1;   /* (the scene's sound now, no preset's) */
#if FRS_LEAD
    up_cur[3] = -1;                      /* (its LEAD too) */
#endif
    if (s >= NSCENES && !test_keys_stay) {   /* NEW SCENE: KEYS back to their defaults */
        uint8_t was[P_KY_BS_STACK + 1u - P_KY_DR_HARM];
        uint32_t p;
        memcpy(was, &st.v[P_KY_DR_HARM], sizeof was);
        eng_keys_defaults();
        for (p = P_KY_DR_HARM; p <= P_KY_BS_STACK; p++)   /* (each that moved, as frs_keys_defaults sets them) */
            if (was[p - P_KY_DR_HARM] != st.v[p])
                lv_set(p, st.v[p]);
        if (set.magic == SET_MAGIC)
            prefs_later();
    }
}

/* a backup streaming (app_backup_hold): no STORE till it ends (a pop-up says so) */
static int store_held(void)
{
    if (fm.store_hold)
        popup(POP_WARN, "SCENES", "NOT STORED", "A BACKUP IS GOING TO USB.", "STORE AGAIN IN A MOMENT.", "");
    return fm.store_hold;
}

static void settings_keys(void)          /* KEYS, METRONOME, COUNT-IN into the record */
{
    uint32_t i;
    for (i = 0; i < sizeof set.ky; i++)
        set.ky[i] = st.v[P_KY_DR_HARM + i];
    set.keys = st.v[P_KEYS];
    set.metro_off = !st.metro;
    set.metro = st.v[P_METRO];
    set.metro_on = fm.metro_on;
    set.count_in = st.v[P_COUNT_IN];
    set.arp_snap = (uint8_t)(st.v[P_ARP_SNAP] + 1u);
}

static int store_all(void)
{
    uint32_t i;
    set.magic = SET_MAGIC;
    for (i = 0; i < NSOUND; i++)
        set.v[i] = kept_v(i);
    set.octave = fm.octave;
    set.keys = st.v[P_KEYS];
    set.channel = st.v[P_CANAL];
    set.ch_glint = st.v[P_CH_GLINT];
    set.ch_bass = st.v[P_CH_BASS];
    set.scene = fm.scene;
    set.tempo = st.v[P_TEMPO];
    for (i = 0; i < SC_SEQ; i++)
        set.seq[i] = seq[i];
    set.metro_off = !st.metro;
    set.speaker_off = !st.v[P_SPEAKER];
    set.anim = st.v[P_ANIM];
    set.saver_off = saver_byte();
    set.head = hdr.dark;
    set.a4 = (int8_t)((int32_t)a4.hz - 440);
    set.clock = st.v[P_CLOCK];
    set.load = st.v[P_LOAD];
    memcpy(set.bnote, bnote, sizeof bnote);
    set.bq_on = st.v[P_BQ_ON];
    set.bq_steps = st.v[P_BQ_STEPS];
    set.bq_trans = st.v[P_BQ_TRANS];
    set.bq_len = st.v[P_BQ_LEN];
    set.ba_mode = st.v[P_BA_MODE];
    set.ba_range = st.v[P_BA_RANGE];
    set.ba_chance = st.v[P_BA_CHANCE];
    set.bq_gate = st.v[P_BQ_GATE];
    set.bs_weight = st.v[P_BS_WEIGHT];
    set.bs_punch = st.v[P_BS_PUNCH];
    set.bs_bite = st.v[P_BS_BITE];
    set.bs_space = st.v[P_BS_SPACE];
    set.gl_vib = st.v[P_GL_VIB_AUTO];
    set.tp_flutter = st.v[P_TP_FLUTTER];
    set.tp_tone = st.v[P_TP_TONE];
    set.tp_noise = st.v[P_TP_NOISE];
    set.tp_dropout = st.v[P_TP_DROPOUT];
    for (i = 0; i < 12u; i++)
        set.lfo_amt[i] = st.v[P_L1_AMT1 + i];
    set.swell = st.v[P_SWELL];
    settings_keys();
    return plat_store_save(OBJ_SETTINGS, &set, sizeof set);
}

/* a scene's drone sequence's notes (SC_DSEQ x DQ_NOTES bytes, each 0 .. 127) in OBJ_DSEQ: seven bits each, the first
 * note's in the first byte's low bits, on up (note j at bits 7j .. 7j + 6 of the little-endian stream) */
static void dsq_pack(const uint8_t *n, uint8_t *o)
{
    uint32_t j, b;
    memset(o, 0, DSQ_PACK);
    for (j = 0; j < SC_DSEQ * DQ_NOTES; j++) {
        uint32_t v = n[j] & 127u;
        b = 7u * j;
        o[b >> 3] |= (uint8_t)(v << (b & 7u));
        if ((b & 7u) > 1u)
            o[(b >> 3) + 1u] |= (uint8_t)(v >> (8u - (b & 7u)));
    }
}

static void dsq_unpack(const uint8_t *o, uint8_t *n)
{
    uint32_t j, b;
    for (j = 0; j < SC_DSEQ * DQ_NOTES; j++) {
        uint32_t v;
        b = 7u * j;
        v = (uint32_t)o[b >> 3] >> (b & 7u);
        if ((b & 7u) > 1u)
            v |= (uint32_t)o[(b >> 3) + 1u] << (8u - (b & 7u));
        n[j] = (uint8_t)(v & 127u);
    }
}

static int nms_any(const uint8_t *m)     /* a scene's NMS_MORE bytes (name_more .. swell): anything but none */
{
    uint32_t i;
    for (i = 0; i < 12u; i++)
        if (m[SC_MORE + i] != PD[P_L1_AMT1 + i].def)
            return 1;
    return m[0] || m[SC_MORE + 12u] != PD[P_SWELL].def;
}

/* the rest of the names, the AMOUNTs and SWELL into the store (OBJ_NAMES), after OBJ_SCENES' record (still in raw: each
 * slot's sum taken from it). Not written while every saved scene has them as none and the store holds none (names of
 * sixteen, +100 %, SWELL 0: the flash written as 3.0 did). 0: taken */
static uint8_t names_stored;             /* the store's OBJ_NAMES holds something (a name's rest, an AMOUNT, SWELL) */
static int names_save(void)
{
    uint32_t s, sum[NSCENES], any = 0;
    for (s = 0; s < NSCENES; s++) {
        sum[s] = rec_sum(raw + 8u + s * SC_REC, SC_REC);
        if (scn.used >> s & 1u)
            any |= (uint32_t)nms_any((const uint8_t *)scn.s[s].name_more);
    }
    if (!any && !names_stored)
        return 0;
    for (s = 0; s < 4u; s++)
        raw[s] = (uint8_t)(NMS_MAGIC >> (8u * s));
    for (s = 0; s < NSCENES; s++) {
        uint8_t *o = raw + 4u + s * NMS_SCENE;
        o[0] = (uint8_t)sum[s], o[1] = (uint8_t)(sum[s] >> 8), o[2] = (uint8_t)(sum[s] >> 16), o[3] = (uint8_t)(sum[s] >> 24);
        memcpy(o + 4, scn.s[s].name_more, NMS_MORE);   /* (name_more, lfo_amt, swell: one after the other) */
    }
    if (plat_store_save(OBJ_NAMES, raw, NMS_BYTES))
        return -1;
    names_stored = any != 0;
    return 0;
}

/* the rest of the names, the AMOUNTs and SWELL from the store, each onto the scene whose record has the sum it was
 * written with: its own slot's first, then any other's (a River that moved the scene). sum: each slot's, from OBJ_SCENES
 * as read (0: an earlier layout's record, or none: every scene's none, more_none). FRNM's (3.0.1): the names' rest alone.
 * A rest only after a first SC_NAME all used, as names_save wrote it; a byte that is no character (below the space,
 * past ~): none from there; an AMOUNT or SWELL past its max: its default */
static void names_load(const uint32_t *sum)
{
    uint32_t s, k, j;
    int n = plat_store_load(OBJ_NAMES, raw, sizeof raw), frnn = n == (int)NMS_BYTES && rd32(raw) == NMS_FRNN;
    int ok = (n == (int)NMS_BYTES && rd32(raw) == NMS_MAGIC) || frnn;
    int frnm = n == (int)NMS_BYTES_FRNM && rd32(raw) == NMS_FRNM;
    uint32_t sz = ok ? NMS_SCENE : NMS_SCENE_FRNM;
    names_stored = 0;
    for (s = 0; s < NSCENES; s++)
        more_none(&scn.s[s]);
    for (k = 0; (ok || frnm) && k < NSCENES; k++)
        names_stored |= ok ? nms_any(raw + 4u + k * sz + 4u) : raw[4u + k * sz + 4u] != 0;
    for (s = 0; (ok || frnm) && sum && s < NSCENES; s++) {
        scene_t *d = &scn.s[s];
        if (!(scn.used >> s & 1u))
            continue;
        for (k = 0; k < NSCENES; k++) {
            const uint8_t *o = raw + 4u + ((s + k) % NSCENES) * sz;
            if (rd32(o) != sum[s])
                continue;
            for (j = 0; d->name[SC_NAME - 1u] && j < SC_MORE && o[4u + j] >= 0x20u && o[4u + j] < 0x7Fu; j++)
                d->name_more[j] = (char)o[4u + j];
            for (j = 0; ok && j < 12u; j++)
                d->lfo_amt[j] = o[4u + SC_MORE + j] > PD[P_L1_AMT1 + j].max && !frnn ? (uint8_t)PD[P_L1_AMT1 + j].def
                                                                                     : o[4u + SC_MORE + j];
            if (frnn)                    /* (FRNN's percent: multipliers, one past 200 x1.00) */
                amt_pct_over(d->lfo_amt);
            if (ok)
                d->swell = o[4u + SC_MORE + 12u] > PD[P_SWELL].max ? (uint8_t)PD[P_SWELL].def : o[4u + SC_MORE + 12u];
            break;
        }
    }
    if ((frnm || frnn) && names_stored && scn.used)
        store_old |= 1u;                 /* (the store's still in 3.0.1's layout, or 3.0.2-b5's: a backup saves it
                                          * first) */
}

/* the scenes into the store: their drone sequences first (OBJ_DSEQ), their bass sequences (OBJ_BSEQ), then the rest of
 * them (OBJ_SCENES), each record made in raw, then the rest of their names (OBJ_NAMES; not with names 0: 3.0's way, the
 * tests' store30). 0: all taken */
static int scenes_store(int names);
static int scenes_save(void) { return scenes_store(1); }
static int scenes_store(int names)
{
    uint32_t s;
    for (s = 0; s < 4u; s++)
        raw[s] = (uint8_t)(DSQ_MAGIC >> (8u * s));
    for (s = 0; s < NSCENES; s++) {
        uint8_t *o = raw + 4u + s * DSQ_SCENE;
        memcpy(o, &scn.s[s].dq_on, DSQ_SET);
        dsq_pack(&scn.s[s].dnote[0][0], o + DSQ_SET);
        memcpy(o + DSQ_SCENE_FRDQ, &scn.s[s].tp_flutter, 4u);   /* (LOFI TAPE II's four) */
    }
    if (plat_store_save(OBJ_DSEQ, raw, DSQ_BYTES))
        return -1;
    for (s = 0; s < 4u; s++)
        raw[s] = (uint8_t)(BSQ_MAGIC >> (8u * s));
    for (s = 0; s < NSCENES; s++)
        memcpy(raw + 4u + s * BSQ_SCENE, scn.s[s].seq, BSQ_SCENE);
    if (plat_store_save(OBJ_BSEQ, raw, BSQ_BYTES))
        return -1;
    memcpy(raw, &scn, 8u);               /* (magic, used, rsv) */
    for (s = 0; s < NSCENES; s++)
        memcpy(raw + 8u + s * SC_REC, &scn.s[s], SC_REC);
    if (plat_store_save(OBJ_SCENES, raw, SCN_BYTES))
        return -1;
    return names ? names_save() : 0;
}

/* LOAD, the metronome, the CCs learned, SPK/JACK, ANIMATIONS, SCREENSAVER: kept (the sound and the rest of the record
 * as SAVE left them; never saved yet: all of it); a backup streaming, once it ends (set holds them meanwhile). Asked
 * through prefs_due, done once nothing sounds (prefs_quiet): writing the flash stops the render a moment, a click in
 * what plays (the user, 2.5-b28: METRONOME turned on or off clicked) */
static int prefs_quiet(void) { return !holding_any() && frasil_out_level() < 0.001f; }   /* (-60 dB: the tails gone) */
static void prefs_save(void);
static void prefs_later(void)            /* now if nothing sounds, else once nothing does */
{
    if (prefs_quiet() && !fm.store_hold)
        prefs_save();
    else if (!fm.prefs_due)
        fm.prefs_due = 1;
}
static void prefs_save(void)
{
    if (fm.store_hold) {
        fm.prefs_due = 1;
        return;
    }
    fm.prefs_due = 0;
    if (set.magic != SET_MAGIC) {
        store_all();
        return;
    }
    settings_keys();
    set.speaker_off = !st.v[P_SPEAKER];
    set.anim = st.v[P_ANIM];
    set.saver_off = saver_byte();
    set.head = hdr.dark;
    set.a4 = (int8_t)((int32_t)a4.hz - 440);
    set.clock = st.v[P_CLOCK];
    set.load = st.v[P_LOAD];
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0)
        store_old &= ~2u;
}

static void name_open(uint32_t s);      /* (STORE's name, below) */

/* the sound into slot s; name: its name (0: as the slot has it, an empty slot the drone's preset's). 1: stored, said */
static int scene_write(uint32_t s, const char *name)
{
    scene_t sc;
    uint32_t i;
    char say2[4 + SC_LONG], t[SC_LONG + 1];
    scene_get(s, &sc);                   /* keeps its name */
    if (!(scn.used >> s & 1u))           /* (a factory scene's, an empty slot's: sixteen at most) */
        memset(sc.name_more, 0, SC_MORE);
    if (!(scn.used >> s & 1u) && s >= NFACTORY && st.v[P_DR_PRESET]) {   /* an empty slot: the drone's preset's name (NUL
                                                                          * after it: "SCENE 8"'s 8 not left in the
                                                                          * record) */
        memset(sc.name, 0, SC_NAME);
        text_cat(sc.name, DRONE_NAME[st.v[P_DR_PRESET] % (NDPRESET + 1u)]);
    }
    anchor_settle();                     /* (ANCHOR's way back under way: stored where it goes) */
    for (i = 0; i < NSOUND; i++)
        sc.v[i] = kept_v(i);
    scene_notes_take(&sc);
    scn.magic = SCN_MAGIC;
    scn.s[s] = sc;
    if (name)                            /* (SC_LONG at most: the rest in name_more) */
        name_put(s, name);
    scn.used |= (uint16_t)(1u << s);
    fm.scene = (uint8_t)s;
#if FRS_LEAD                             /* (and its LEAD, its own object: ldp_save) */
    if (scenes_save() == 0 && store_all() == 0 && ldp_save(s) == 0) {
#else
    if (scenes_save() == 0 && store_all() == 0) {
#endif
        store_old = 0;
        sound_mark();
        lv_long(FST_STORE, s);           /* (frs_scene_store: the scene as saved now, the sound marked so) */
        say2[0] = (char)('0' + (s + 1u) / 10u);   /* "03 ABYSS" */
        say2[1] = (char)('0' + (s + 1u) % 10u);
        say2[2] = ' ';
        say2[3] = 0;
        text_cat(say2, scene_name(s, t));
        app_say("STORED", say2);
        return 1;
    }
    popup(POP_WARN, "SCENES", "NOT STORED", "THE MEMORY DID NOT TAKE IT.", "TRY AGAIN.", "");
    return 0;
}

static void scene_save(uint32_t s)
{
    if (!store_held() && scene_write(s, 0))
        name_open(s);
}

/* SAVE held and REC: STORE, into the slot K1 points at on SCENES, else into the current scene. NEW SHARD is no slot: on
 * it, nothing (CHOOSE A SLOT); the current scene, into the first EMPTY slot (9 .. 16, nothing saved in it: a factory
 * scene stays), else SCENES on slot 16, to choose one */
static void scene_store(void)
{
    uint32_t s = fm.page == PG_SCENES ? fm.pick : fm.scene;
    if (s >= NSCENES && fm.page != PG_SCENES)
        for (s = NFACTORY; s < NSCENES && (scn.used >> s & 1u); s++)
            ;
    if (s < NSCENES) {
        scene_save(s);
        return;
    }
    if (fm.page != PG_SCENES) {
        fm.prev_page = fm.page;
        fm.page = PG_SCENES;
        fm.pick = NSCENES - 1u;
        fm.logo = fm.help = 0;
    }
    app_say("STORE", "CHOOSE A SLOT");
}

/* ------------------------------------------------------------ a name --- */
/* STORE written: the scene's name on the whole screen (ui.c name_screen), to change or keep. OCT- and OCT+ move the
 * cursor along the places (a scene's SC_LONG, in two rows; a preset's SC_NAME); K4 turns its letter (a space before A), K3
 * its digit (and - .); SEL (OK) stores the name (the scenes written again if it changed); FX leaves it as it was stored.
 * Meanwhile the keys and MIDI play; the other buttons and the encoders wait */
static const char NM_LETTERS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ", NM_DIGITS[] = "0123456789-.";
static int text_is(const char *a, const char *b);   /* (below) */
static struct {
    uint8_t on, slot, cur, drawn;
    uint8_t flow;                        /* the SAVE flow's: SAVE stores what WHAT and INTO say, FX back (else STORE's, the
                                          * scene stored: SEL keeps the name, FX leaves it) */
    uint8_t len;                         /* its places: SC_LONG a scene's, SC_NAME a preset's (the engine's "FRp") */
    uint32_t btn;                        /* the buttons down last frame: a press acts */
    char name[SC_LONG + 1];              /* len characters, a space for none */
} nm;

static void name_fill(const char *t, uint32_t len)   /* the places from t (NUL-ended), spaces after it */
{
    uint32_t i, end = 0;
    for (i = 0; i < len; i++) {
        end = end || !t[i];
        nm.name[i] = end ? ' ' : t[i];
    }
    nm.name[len] = 0;
    nm.len = (uint8_t)len;
}

static void name_open(uint32_t s)
{
    char t[SC_LONG + 1];
    name_fill(scene_name(s, t), SC_LONG);
    nm.slot = (uint8_t)s;
    nm.cur = 0;
    nm.on = 1;
    nm.flow = 0;
    nm.drawn = 0;
    nm.btn = plat_buttons();             /* (SAVE and REC, still down from STORE: no press) */
    fm.hold_done = ~0u;
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
    char was[SC_LONG + 1], t[4 + SC_LONG];
    int32_t i, n = (int32_t)nm.len;
    nm.on = 0;
    while (n > 0 && nm.name[n - 1] == ' ')
        n--;
    if (!keep || !n)                     /* (all spaces: the name as stored) */
        return;
    for (i = 0; i < n; i++)
        t[3 + i] = nm.name[i];
    t[3 + n] = 0;
    if (text_is(t + 3, scene_name(nm.slot, was)))   /* unchanged */
        return;
    if (store_held())
        return;
    name_put(nm.slot, t + 3);
    if (scenes_save() == 0) {
        t[0] = (char)('0' + (nm.slot + 1u) / 10u);
        t[1] = (char)('0' + (nm.slot + 1u) % 10u);
        t[2] = ' ';
        app_say("NAMED", t);
    } else {
        popup(POP_WARN, "SCENES", "NOT NAMED", "THE MEMORY DID NOT TAKE IT.", "THE NAME STAYS AS IT WAS.", "");
    }
}

/* ---------------------------------------------------- the SAVE flow, presets --- */
/* SAVE: LOAD | SAVE (K1, PG_SAVE); LOAD: SCENES. SAVE: WHAT and INTO (PG_SAVEAS); then the name (nm.flow); SEL goes on, FX
 * goes back a step. WHAT: 0 SCENE, 1 DRONE PRESET, 2 GLINT PRESET, 3 BASS PRESET (the SMK-37: 4 LEAD PRESET, admin #628).
 * INTO: a slot, or for a preset 0 NEW, n the player's nth (replaced). The player's presets come after the factory ones
 * in PRESET's lists */
static struct {
    uint8_t load;                        /* K1: 1 LOAD (SAVE to begin with; kept) */
    uint8_t what, into;
} sv;

static uint8_t *up_at(uint32_t t, uint32_t i) { return t == 0u ? up.dr[i] : t == 1u ? up.gl[i] : up.bs[i]; }
static uint32_t up_bytes(uint32_t t) { return t == 0u ? sizeof up.dr[0] : t == 1u ? sizeof up.gl[0] : sizeof up.bs[0]; }
static uint32_t up_n(uint32_t t)         /* how many the track has */
{
    uint32_t n = 0;
#if FRS_LEAD
    if (t == 3u)                         /* (the LEAD's: OBJ_LEAD's own part) */
        return ldu_n();
#endif
    while (n < NUP && up_at(t, n)[0] == 'F')
        n++;
    return n;
}
static int up_in_use(uint32_t t)         /* the track's preset of the player's, as it sounds now, -1 none */
{
    int c = up_cur[t];
#if FRS_LEAD
    if (t == 3u)
        return ldu_in_use();
#endif
    return c >= 0 && eng_preset_is(up_at(t, (uint32_t)c), up_bytes(t)) == 1 ? c : -1;
}
static void up_name(uint32_t t, uint32_t i, char *o)   /* its name (SC_NAME + 1 at most) */
{
#if FRS_LEAD
    if (t == 3u) {
        ldu_name(i, o);
        return;
    }
#endif
    const uint8_t *b = up_at(t, i) + 6u;
    uint32_t k;
    for (k = 0; k < SC_NAME && b[k]; k++)
        o[k] = (char)b[k];
    o[k] = 0;
}

/* where INTO starts: a scene, the current slot if the player's, else the first empty one, none empty slot 01; a preset,
 * the one in use if the player's (taken last on the track, turned since or not), else NEW (all sixteen taken: the first) */
static uint32_t into_min(void) { return sv.what && up_n(sv.what - 1u) >= NUP ? 1u : 0u; }
static void into_start(void)
{
    uint32_t s = 0;
    if (!sv.what) {
        if (fm.scene < NSCENES && (scn.used >> fm.scene & 1u)) {
            s = fm.scene;
        } else {
            for (s = NFACTORY; s < NSCENES && (scn.used >> s & 1u); s++)
                ;
            s = s < NSCENES ? s : 0u;
        }
    } else {
        int u = up_cur[sv.what - 1u];
        s = u >= 0 ? (uint32_t)u + 1u : into_min();
    }
    sv.into = (uint8_t)s;
}

/* page p's place on track t: EDIT's first; ENV, ARP, SEQ the same place (hidden, its first); a page all tracks have: itself */
static uint32_t track_page(uint32_t p, uint32_t t)
{
    uint32_t i, w;
    if (PAGES[p].track == T_ANY || PAGES[p].track == t)
        return p;
#if FRS_LEAD
    if (PAGES[p].button == B_EDIT && t == 3u)
        return PG_LEAD;
#endif
    if (PAGES[p].button == B_EDIT)
        return t == 1u ? PG_GLINT : t == 2u ? PG_BASS : PG_DRONE;
    for (i = 0; i < sizeof TRACK_PAGES / sizeof TRACK_PAGES[0]; i++)
        for (w = 0; w < 3u + FRS_LEAD; w++)
            if (TRACK_PAGES[i][w] == p)
                return TRACK_PAGES[i][t];
    return PG_SON;
}

static void flow_saveas(void)            /* WHAT and INTO */
{
    fm.page = PG_SAVEAS;
    sv.what = 0;
    into_start();
}

static void flow_back(void)              /* the flow left: back where it came from, on the track selected now (SELECT
                                          * turns on SAVE's pages too) */
{
    fm.page = fm.prev_page < NPAGES && fm.prev_page != PG_SCENES && fm.prev_page != PG_SAVE && fm.prev_page != PG_SAVEAS
                  ? (uint8_t)track_page(fm.prev_page, st.v[P_KB]) : PG_SON;
    page_back();
}

/* the name, its text as it starts: the scene's, or the preset's in use */
static void flow_name(void)
{
    char t[SC_LONG + 25];
    t[0] = 0;
    if (!sv.what) {
        scene_name(fm.scene, t);
        if (fm.scene >= NSCENES && st.v[P_DR_PRESET]) {
            t[0] = 0;
            text_cat(t, DRONE_NAME[st.v[P_DR_PRESET] % (NDPRESET + 1u)]);
        }
    } else {
        uint32_t k = sv.what - 1u;
        int u = up_cur[k];
        if (u >= 0)
            up_name(k, (uint32_t)u, t);
#if FRS_LEAD
        else if (k == 3u)                /* (the LEAD's: its PRESET's name) */
            value_text(P_LD_PRESET, st.v[P_LD_PRESET], t);
#endif
        else
            value_text(k == 0u ? P_DR_PRESET : k == 1u ? P_GL_PRESET : P_BS_MODEL,
                       st.v[k == 0u ? P_DR_PRESET : k == 1u ? P_GL_PRESET : P_BS_MODEL], t);
    }
    name_fill(t, sv.what ? SC_NAME : SC_LONG);   /* (a preset's: the engine keeps sixteen) */
    nm.cur = 0;
    nm.on = 1;
    nm.flow = 1;
    nm.drawn = 0;
    nm.btn = plat_buttons();
    fm.hold_done = ~0u;
}

/* SAVE on the name: what WHAT and INTO say, stored; the flow over. 0: no name yet */
static int flow_store(void)
{
    char t[SC_LONG + 1];
    int32_t n = (int32_t)nm.len;
    uint32_t k = sv.what - 1u, i;
    while (n > 0 && nm.name[n - 1] == ' ')
        n--;
    if (!n)
        return 0;
    for (i = 0; i < (uint32_t)n; i++)
        t[i] = nm.name[i];
    t[n] = 0;
    nm.on = 0;
    if (store_held()) {
        flow_back();
        return 1;
    }
    if (!sv.what) {
        scene_write(sv.into, t);
#if FRS_LEAD
    } else if (k == 3u) {                /* the LEAD's: into OBJ_LEAD's own part (ldu_save), nothing kept in RAM */
        uint32_t at = sv.into ? sv.into - 1u : up_n(k);
        if (ldu_save(at, t) == 0) {
            up_cur[k] = (int8_t)at;
            app_say("STORED", t);
        } else {
            popup(POP_WARN, "PRESETS", "NOT STORED", "THE MEMORY DID NOT TAKE IT.", "TRY AGAIN.", "");
        }
#endif
    } else {
        uint32_t at = sv.into ? sv.into - 1u : up_n(k), magic = up.magic;
        uint8_t was[PRESET_BYTES];
        memcpy(was, up_at(k, at), up_bytes(k));
        eng_preset_capture(k, t, up_at(k, at));
        up.magic = UPR_MAGIC;
        if (plat_store_save(OBJ_UPRE, &up, sizeof up) == 0) {
            up_cur[k] = (int8_t)at;
            app_say("STORED", t);
        } else {                         /* (not in the flash: not in the lists either) */
            memcpy(up_at(k, at), was, up_bytes(k));
            up.magic = magic;
            popup(POP_WARN, "PRESETS", "NOT STORED", "THE MEMORY DID NOT TAKE IT.", "TRY AGAIN.", "");
        }
    }
    flow_back();
    return 1;
}

/* the name up, a frame's input: 1 still up (the buttons, for the rest of the frame, none) */
static int name_input(uint32_t btn)
{
    uint32_t down = btn & ~nm.btn, e;
    nm.btn = btn;
    for (e = 0; e < NE; e++) {
        int32_t d = enc_read((int)e);
        if (d && e == EN_K4)
            name_turn(NM_LETTERS, (int32_t)sizeof NM_LETTERS - 1, d);
        else if (d && e == EN_K3)
            name_turn(NM_DIGITS, (int32_t)sizeof NM_DIGITS - 1, d);
        else if (d && e == EN_K1) {      /* (K1 the cursor too, as OCT; K2 a space at it) */
            nm.cur = (uint8_t)clamp((int32_t)nm.cur + d, 0, (int32_t)nm.len - 1);
            nm.drawn = 0;
        } else if (d && e == EN_K2) {
            nm.name[nm.cur] = ' ';
            nm.drawn = 0;
        }
    }
    if ((down >> B_OCTDN & 1u) && nm.cur > 0) {
        nm.cur--;
        nm.drawn = 0;
    }
    if ((down >> B_OCTUP & 1u) && nm.cur + 1u < nm.len) {
        nm.cur++;
        nm.drawn = 0;
    }
    if (nm.flow && (down & (1u << B_SAVE | 1u << B_SEL))) {   /* (SAVE stores; SEL too, the rule everywhere else) */
        if (!flow_store())
            return 1;                    /* (no name: nothing, the name stays up) */
    } else if (nm.flow && (down >> B_FX & 1u)) {
        nm.on = 0;
    } else if (!nm.flow && (down & (1u << B_SAVE | 1u << B_SEL)))
        name_close(1);
    else if (!nm.flow && (down >> B_FX & 1u))
        name_close(0);
    else
        return 1;
    btn_prev = btn;                      /* (what is down now: used, no tap on its release) */
    fm.hold_done = ~0u;
    return 0;
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

/* a scene of a layout before FRSv, after its bass's sequence: PLAYS as this one has it (SEQUENCE on was 1, SEQ's 2 now),
 * no arpeggio on the bass */
static void bass_ways(scene_t *sc)
{
    sc->bq_on = sc->bq_on ? SQT_SEQ : 0u;
    barp_none(sc);
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

static int set_keys_ok;                  /* settings_old read a record that has KEYS (FRSP's, FRSQ's) */
static int settings_old(void)             /* the settings in an earlier layout, the same (the bass's sequence none),
                                           * its bass's sequence as it kept it */
{
    int n = plat_store_load(OBJ_SETTINGS, raw, sizeof raw);
    uint32_t k, j;
    set.bq_steps = PD[P_BQ_STEPS].def;
    set.bq_trans = PD[P_BQ_TRANS].def;
    set_keys_ok = 0;
    if (n == (int)sizeof set && rd32(raw) == SET_FRSR) {   /* FRSR, 3.0.2-b5's: this layout, the AMOUNTs in percent */
        memcpy(&set, raw, sizeof set);
        amt_pct_over(set.lfo_amt);
        set.magic = SET_MAGIC;
        set_keys_ok = 1;
        return 6;
    }
    if (n == (int)FRSQ_BYTES && rd32(raw) == SET_FRSQ) {   /* FRSQ, 3.0.2's: this layout to tp_dropout */
        memcpy(&set, raw, FRSQ_BYTES);
        set.magic = SET_MAGIC;
        set_keys_ok = 1;
        return 5;
    }
    if (n == (int)FRSP_BYTES && rd32(raw) == SET_FRSP) {   /* FRSP, 3.0's before LOFI TAPE II: this layout to saver_off */
        memcpy(&set, raw, FRSP_BYTES);
        set.magic = SET_MAGIC;
        set_keys_ok = 1;                 /* (its KEYS, METRONOME, COUNT-IN, ARP SNAP are there) */
        return 4;
    }
    if (n == (int)FRSO_BYTES && rd32(raw) == SET_FRSO) {   /* FRSO, 3.0's before KEYS: this layout to gl_vib */
        memcpy(&set, raw, FRSO_BYTES);
        set.magic = SET_MAGIC;
        return 3;
    }
    if (n == (int)FRSN_BYTES && rd32(raw) == SET_FRSN) {   /* FRSN, 2.7's first test builds': this layout to bq_len */
        memcpy(&set, raw, FRSN_BYTES);
        set.magic = SET_MAGIC;
        return 2;
    }
    if (n == (int)FRSN_BYTES && rd32(raw) == SET_FRSM) {   /* FRSM, 2.6's: FRSN's, STEP LENGTH not yet */
        memcpy(&set, raw, FRSN_BYTES);
        set.magic = SET_MAGIC;
        return 1;
    }
    if (n == (int)FRSL_BYTES && rd32(raw) == SET_FRSL) {
        /* FRSL, 2.5's: this layout, its sound FRSQ_NV long (SOFT not yet: 0, the wave as it was). A CC learned on a
         * setting after the sound: on the same setting, its number moved on by the sound's one more */
        sound_over(set.v, raw + 4, FRSQ_NV, 0);
        memcpy((uint8_t *)&set + offsetof(settings_t, octave), raw + 4u + FRSQ_NV, FRSL_BYTES - 4u - FRSQ_NV);
        for (j = 0; j < NLEARN; j++)
            if (set.learn_p[j] >= FRSQ_NV && set.learn_p[j] < P_AFTER_AMT - (NSOUND - FRSQ_NV))
                set.learn_p[j] = (uint8_t)(set.learn_p[j] + (NSOUND - FRSQ_NV));
        set.magic = SET_MAGIC;
        return 1;
    }
    if (n == (int)FRSK_BYTES && rd32(raw) == SET_FRSK) {
        /* FRSK, 2.5's betas': this layout, its sound FRSO_NV long, ANIMATIONS not yet (its padding byte there: NORMAL).
         * A CC learned on a setting after the sound: on the same setting, its number moved on by the sounds added */
        sound_over(set.v, raw + 4, FRSO_NV, 0);
        memcpy((uint8_t *)&set + offsetof(settings_t, octave), raw + 4u + FRSO_NV, FRSK_BYTES - 4u - FRSO_NV);
        set.anim = 0;
        set.clock = 0;
        for (j = 0; j < NLEARN; j++)
            if (set.learn_p[j] >= FRSO_NV && set.learn_p[j] < P_AFTER_AMT - (NSOUND - FRSO_NV))
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
            if (set.learn_p[j] >= FRSL_NV && set.learn_p[j] < P_AFTER_AMT - (NSOUND - FRSL_NV))
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
            if (set.learn_p[j] >= FRSH_NV && set.learn_p[j] < P_AFTER_AMT - (NSOUND - FRSH_NV))
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

static int settings_over(void)           /* (settings_old's, the bass's sequence made over: bseq_over, but FRSN's; PLAYS
                                          * as this layout has it, the arpeggio's settings and GATE their defaults) */
{
    int r = settings_old();
    uint32_t k;
    if (r == 1)
        bseq_over(set.bnote, &set.bq_steps, &set.bq_len);
    if (r && r != 6) {                   /* (every one before FRSR: the LFOs' AMOUNTs x1.00, SWELL 0) */
        for (k = 0; k < 12u; k++)
            set.lfo_amt[k] = (uint8_t)PD[P_L1_AMT1 + k].def;
        set.swell = (uint8_t)PD[P_SWELL].def;
    }
    if (r && r < 5)                      /* (every one before FRSQ: LOFI TAPE II's four as the tape was, TAPE_NONE:
                                          * TAPE SPEED x1.00, TONE 0; not their defaults) */
        memcpy(&set.tp_flutter, TAPE_NONE, 4u);
    if (r && r < 3) {
        set.bq_on = set.bq_on ? SQT_SEQ : 0u;
        set.ba_mode = PD[P_BA_MODE].def;
        set.ba_range = PD[P_BA_RANGE].def;
        set.ba_chance = PD[P_BA_CHANCE].def;
        set.bq_gate = PD[P_BQ_GATE].def;
        set.bs_weight = set.bs_punch = set.bs_bite = set.bs_space = set.gl_vib = 0;
    }
    return r != 0;
}

/* the state as at power-on (nothing latched, held or to undo, nothing playing; the keys the drones'), from the store:
 * the scenes and the settings (an earlier layout's made over), the sound and the rest as the settings have them, else
 * the first scene. At boot, and after a restore from USB. The sequences that are on wait for PLAY (tapped: from their
 * first steps, the beat beginning there; held: the scene's notes too): begun here, they started off the beat (a
 * sixteenth into a grid no top had set: the bass's NOTEs off PULSE's strikes), and alone */
static void state_load(void)
{
    uint32_t i;
    int keys_kept;
    eng_power_on();
    fm.metro_on = 1;
    fm.octave = OCT_DEF;
    fm.help = 0;
    fm.prefs_due = 0;
    store_old = 0;
    int cur = plat_store_load(OBJ_SCENES, raw, sizeof raw) == (int)SCN_BYTES && rd32(raw) == SCN_MAGIC, over = 0;
    uint32_t sum[NSCENES];               /* (each slot's record summed: names_load) */
    if (cur) {
        memcpy(&scn, raw, 8u);
        for (i = 0; i < NSCENES; i++) {
            memcpy(&scn.s[i], raw + 8u + i * SC_REC, SC_REC);
            sum[i] = rec_sum(raw + 8u + i * SC_REC, SC_REC);
        }
    } else {
        scn.used = 0;
        over = scenes_over();
        store_old |= (uint8_t)(over != 0);
        scn.magic = SCN_MAGIC;
    }
    {                                    /* their sequences, basses and TEMPOs: FRBX's, GLINT's sequence first; FRBW's
                                          * the same but the bass's SPACE; FRBV's
                                          * the same to RATE, FRBU's to TEMPO, the bass's sequence made over (bseq_over);
                                          * FRBT's, the bass from its notes on (GLINT's sequence in an earlier scenes'
                                          * record); FRBS's without TEMPO (the settings', below); none: each empty */
        int n = plat_store_load(OBJ_BSEQ, raw, sizeof raw), ok = n == (int)BSQ_BYTES && rd32(raw) == BSQ_MAGIC;
        int frbw = n == (int)(4u + NSCENES * FRBW_SCENE) && rd32(raw) == BSQ_FRBW;
        int frbv = n == (int)(4u + NSCENES * FRBV_SCENE) && rd32(raw) == BSQ_FRBV;
        int frbu = n == (int)(4u + NSCENES * FRBU_SCENE) && rd32(raw) == BSQ_FRBU;
        int frbt = n == (int)(4u + NSCENES * FRBT_SCENE) && rd32(raw) == BSQ_FRBT;
        int frbs = n == (int)(4u + NSCENES * FRBS_SCENE) && rd32(raw) == BSQ_FRBS;
        uint32_t sz = ok ? BSQ_SCENE : frbw ? FRBW_SCENE : frbv ? FRBV_SCENE : FRBU_SCENE;
        for (i = 0; i < NSCENES; i++) {
            scene_t *d = &scn.s[i];
            if ((ok || frbw || frbv || frbu) && (cur || over == 2))   /* (FRSo's scenes: their sequences there too) */
                memcpy(d->seq, raw + 4u + i * sz, sz);
            else if (ok || frbw || frbv || frbu)   /* (an earlier scenes' record, restored alone: its own sequence kept) */
                memcpy(d->bnote, raw + 4u + i * sz + SC_SEQ, sz - SC_SEQ);
            else if (frbt || frbs)
                memcpy(d->bnote, raw + 4u + i * (frbt ? FRBT_SCENE : FRBS_SCENE), frbt ? FRBT_SCENE : FRBS_SCENE);
            else
                bseq_empty(d);
            if (frbu || frbt || frbs)
                bseq_over(d->bnote, &d->bq_steps, &d->bq_len);
            if (frbv || frbu || frbt || frbs)
                bass_ways(d);
            if (!ok)
                d->bs_space = d->gl_vib = 0;
            if (!ok && !frbw && !frbv && !frbu && (cur || over == 2))   /* (this scenes' record without its sequences: none) */
                memset(d->seq, SQ_REST, SC_SEQ);
            if (!ok && !frbw && !frbv && !frbu && !frbt)
                d->tempo = 0xFFu;
        }
        if ((frbw || frbv || frbu || frbt || frbs) && scn.used)   /* (the store's still in an earlier layout: a backup saves
                                                           * them first) */
            store_old |= 1u;
    }
    {                                    /* their drone sequences: FRDQ's; none (an earlier FRASIL's store, or its
                                          * backup restored): each off, empty */
        int n = plat_store_load(OBJ_DSEQ, raw, sizeof raw), ok = n == (int)DSQ_BYTES && rd32(raw) == DSQ_MAGIC;
        int old = n == (int)DSQ_BYTES_FRDQ && rd32(raw) == DSQ_FRDQ;   /* (3.0's: without LOFI TAPE II's four) */
        uint32_t sz = ok ? DSQ_SCENE : DSQ_SCENE_FRDQ;
        for (i = 0; i < NSCENES; i++) {
            scene_t *d = &scn.s[i];
            if (ok || old) {
                memcpy(&d->dq_on, raw + 4u + i * sz, DSQ_SET);
                dsq_unpack(raw + 4u + i * sz + DSQ_SET, &d->dnote[0][0]);
            } else {
                dseq_none(d);
            }
            if (ok)
                memcpy(&d->tp_flutter, raw + 4u + i * sz + DSQ_SCENE_FRDQ, 4u);
            else
                tape_none(d);
        }
        if (old && scn.used)
            store_old |= 1u;             /* (the store's still in 3.0's layout: a backup saves it first) */
    }
    names_load(cur ? sum : 0);           /* the rest of their names, their AMOUNTs, SWELL (OBJ_NAMES) */
    if (plat_store_load(OBJ_UPRE, &up, sizeof up) != (int)sizeof up || up.magic != UPR_MAGIC)
        memset(&up, 0, sizeof up);       /* (none: an earlier FRASIL's store, or its backup restored) */
    up_cur[0] = up_cur[1] = up_cur[2] = -1;
#if FRS_LEAD
    up_cur[3] = -1;
#endif
    keys_kept = plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC;
    if (!keys_kept) {
        memset(&set, 0, sizeof set);     /* (nothing learned, the defaults, whatever was read) */
        store_old |= (uint8_t)(settings_over() << 1);
        keys_kept = set_keys_ok;         /* (FRSP's has KEYS; the layouts before it, no) */
    }
    for (i = 0; i < NSCENES; i++)        /* scenes kept before their TEMPO: the one they were played at (the settings') */
        if (scn.s[i].tempo > PD[P_TEMPO].max)
            scn.s[i].tempo = set.magic == SET_MAGIC && set.tempo <= PD[P_TEMPO].max ? set.tempo : PD[P_TEMPO].def;
    hdr.dark = set.magic == SET_MAGIC && set.head == 1u;   /* HEADER (an earlier record, none: INVERT) */
    a4_take(set.magic == SET_MAGIC ? (uint32_t)(440 + set.a4) : 440u);   /* TUNE A4 (an earlier record, none: 440) */
    if (set.magic == SET_MAGIC) {
        for (i = 0; i < NSOUND; i++)
            st.v[i] = set.v[i] > PD[i].max ? PD[i].max : set.v[i];
        drone_take();
        fm.octave = (int8_t)clamp(set.octave, -3, 2);
        st.metro = !set.metro_off;
        if (keys_kept) {                 /* KEYS, METRONOME, COUNT-IN as they were left */
            for (i = 0; i < sizeof set.ky; i++)
                param_set((int)(P_KY_DR_HARM + i), set.ky[i]);
            param_set(P_METRO, set.metro);
            param_set(P_COUNT_IN, set.count_in);
            fm.metro_on = set.metro_on == 2u ? 2u : 1u;
        } else {                         /* (an earlier record: its KEY MODE as the drone's and the bass's LATCH, its
                                          * metronome OFF or REC) */
            param_set(P_KEYS, set.keys ? 1 : 0);
            param_set(P_METRO, !set.metro_off);
        }
        param_set(P_ARP_SNAP, set.arp_snap ? set.arp_snap - 1 : PD[P_ARP_SNAP].def);   /* (0: never set, its default) */
        st.v[P_MORPH] = 0;               /* (CUT at every start, whatever it was left on) */
        st.v[P_LOAD] = set.load ? 1 : 0;
        st.v[P_SPEAKER] = !set.speaker_off;
        st.v[P_ANIM] = set.anim ? 1 : 0;
        saver_unbyte(set.saver_off);     /* SCREENSAVER, IDLE TIME (an earlier record, or one before it: ON, ten) */
        st.v[P_CLOCK] = set.clock == 1u;
        st.v[P_CANAL] = set.channel > 17u ? PD[P_CANAL].def : set.channel;
        st.v[P_CH_GLINT] = set.ch_glint > 17u ? PD[P_CH_GLINT].def : set.ch_glint;
        st.v[P_CH_BASS] = set.ch_bass > 17u ? PD[P_CH_BASS].def : set.ch_bass;
        fm.scene = set.scene == SC_NEW ? SC_NEW : set.scene % NSCENES;
        st.v[P_TEMPO] = set.tempo > PD[P_TEMPO].max ? PD[P_TEMPO].def : set.tempo;
        for (i = 0; i < SC_SEQ; i++)
            seq[i] = set.seq[i];
        seq_view();
        memcpy(bnote, set.bnote, sizeof bnote);
        st.v[P_BQ_ON] = set.bq_on > PD[P_BQ_ON].max ? PD[P_BQ_ON].def : set.bq_on;
        st.v[P_BQ_STEPS] = set.bq_steps > PD[P_BQ_STEPS].max ? PD[P_BQ_STEPS].def : set.bq_steps;
        st.v[P_BQ_TRANS] = set.bq_trans > PD[P_BQ_TRANS].max ? PD[P_BQ_TRANS].def : set.bq_trans;
        st.v[P_BQ_LEN] = set.bq_len > PD[P_BQ_LEN].max ? PD[P_BQ_LEN].def : set.bq_len;
        st.v[P_BA_MODE] = set.ba_mode > PD[P_BA_MODE].max ? PD[P_BA_MODE].def : set.ba_mode;
        st.v[P_BA_RANGE] = set.ba_range > PD[P_BA_RANGE].max ? PD[P_BA_RANGE].def : set.ba_range;
        st.v[P_BA_CHANCE] = set.ba_chance > PD[P_BA_CHANCE].max ? PD[P_BA_CHANCE].def : set.ba_chance;
        st.v[P_BQ_GATE] = set.bq_gate > PD[P_BQ_GATE].max ? PD[P_BQ_GATE].def : set.bq_gate;
        st.v[P_BS_WEIGHT] = set.bs_weight > PD[P_BS_WEIGHT].max ? 0u : set.bs_weight;
        st.v[P_BS_PUNCH] = set.bs_punch > PD[P_BS_PUNCH].max ? 0u : set.bs_punch;
        st.v[P_BS_BITE] = set.bs_bite > PD[P_BS_BITE].max ? 0u : set.bs_bite;
        st.v[P_BS_SPACE] = set.bs_space > PD[P_BS_SPACE].max ? 0u : set.bs_space;
        st.v[P_GL_VIB_AUTO] = set.gl_vib > PD[P_GL_VIB_AUTO].max ? 0u : set.gl_vib;
        st.v[P_TP_FLUTTER] = set.tp_flutter > PD[P_TP_FLUTTER].max ? PD[P_TP_FLUTTER].def : set.tp_flutter;
        st.v[P_TP_TONE] = set.tp_tone > PD[P_TP_TONE].max ? PD[P_TP_TONE].def : set.tp_tone;
        st.v[P_TP_NOISE] = set.tp_noise > PD[P_TP_NOISE].max ? PD[P_TP_NOISE].def : set.tp_noise;
        st.v[P_TP_DROPOUT] = set.tp_dropout > PD[P_TP_DROPOUT].max ? PD[P_TP_DROPOUT].def : set.tp_dropout;
        for (i = 0; i < 12u; i++)        /* the LFOs' AMOUNTs, SWELL: the sound's as it was saved */
            st.v[P_L1_AMT1 + i] = set.lfo_amt[i] > PD[P_L1_AMT1 + i].max ? PD[P_L1_AMT1 + i].def : set.lfo_amt[i];
        st.v[P_SWELL] = set.swell > PD[P_SWELL].max ? PD[P_SWELL].def : set.swell;
        bseq_view();
        if (fm.scene < NSCENES && (scn.used >> fm.scene & 1u))
            dseq_take(&scn.s[fm.scene]);   /* the drone's sequence: the slot's, as it was stored with this sound (the
                                            * settings keep none of their own); a factory scene, NEW SHARD: none */
        else
            memset(dnote, 0, sizeof dnote);   /* (its settings their defaults: eng_power_on's) */
    } else {                             /* (never saved: NEW SHARD, to begin from) */
        scene_load(SC_NEW);
    }
    plat_speaker(st.v[P_SPEAKER] && !spk_page);
    fm.pick = fm.scene;
    fm.page = PG_SON;
#if FRS_LEAD
    ldp_load(fm.scene);                  /* (LEAD's: the scene's, as last stored; NEW SHARD, none: MOSS) */
#endif
    sound_mark();                        /* the sound saved last: nothing changed yet */
    engine_push();
}

void app_init(void)
{
    eng_init(44100.0f);                  /* (the FM-1: 44.1 kHz) */
    eng_keys_touch(1);                   /* (3.0: the keys honour KEYS) */
    step_stays = step_hand = !test_step_moves;   /* (RECORD STEP: K1 moves STEP, the keys never do) */
    frasil_profile(plat_ticks);          /* (where the render's time goes: the log's word) */
#if !defined(PLAT_HOST) && !defined(PLAT_WEB)
    frasil_budget((float)PLAT_TICKS_US * 1e6f / 44100.0f);   /* room made before a voice, not a render run over (the
                                                              * host's and the web's renders stay as they were) */
#endif
    state_load();
    fm.booting = 1;
}

/* GLO held and a CC on the drones' channel: that CC turns the setting turned last from now on (NLEARN of them, the
 * oldest replaced; kept with the settings) */
static void cc_learn(uint32_t cc)
{
    uint32_t i, at = NLEARN;
    char t[12];
    if (!fm.touched_at || fm.touched >= NPARAM) {
        popup(POP_WARN, "MIDI LEARN", "TURN ONE FIRST", "TURN A SETTING ON THE FM-1,", "THEN HOLD GLO AND MOVE",
              "A KNOB ON YOUR CONTROLLER.");
        return;
    }
#if FRS_LEAD
    if (fm.touched >= P_LD_WAVE) {       /* LEAD's: not learned (the learned CCs keep a byte's setting; card #144) */
        app_say("MIDI LEARN", "NOT ON LEAD");
        return;
    }
#endif
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
    set.learn_p[at] = fm.touched;
    prefs_later();
    t[0] = 0;
    text_cat(t, "CC ");
    int_text((int32_t)cc, t + 3);
    app_say(t, PD[fm.touched].name);
}

static void midi_in(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        lv_cc_took = 0;
        eng_midi(pkt);
        lv_midi(pkt);
    }
}

/* the engine's hooks (engine/core.c): the FM-1's clock, its header's words, GLINT SEQ's pages, MIDI learn, its slots */
static uint32_t eng_ms(void) { return plat_ms(); }
static void eng_say(const char *a, const char *b) { app_say(a, b); }
static int eng_trying(uint32_t track)    /* (the track's SEQ pages: RECORD off its keys play, whatever the sequence does) */
{
    return track == 1u ? fm.page >= PG_SEQ && fm.page <= PG_SEQ3 : fm.page == PG_BSEQ || fm.page == PG_BSEQ2 || fm.page == PG_BSEQ3;
}
static void eng_type_changed(void) { page_back(); }   /* (a layer's PLAYS: a page now hidden left for its first) */
static int eng_cc(uint32_t cc, uint32_t v)
{
    uint32_t i;
    int learned = 0;
    if (b_held(B_GLO) && cc) {           /* GLO held: MIDI learn */
        b_used(B_GLO);
        cc_learn(cc);
        lv_cc_took = 1;
        return 1;
    }
    for (i = 0; i < NLEARN; i++)         /* a learned one first */
        if (cc && set.learn_cc[i] == cc && set.learn_p[i] < NPARAM) {
            param_set(set.learn_p[i], ((int32_t)v * PD[set.learn_p[i]].max + 63) / 127);
            learned = 1;
        }
    lv_cc_took = (uint8_t)learned;
    return learned;
}
static void eng_program(uint32_t n)      /* Program Change: a slot */
{
    if (n < NSCENES)
        scene_go(n);
}
static void eng_setting(int p, int32_t x)   /* SPK/JACK: the output at once; it, ANIMATIONS, SCREENSAVER and CLOCK kept
                                             * (SPK/JACK now, whatever plays: the room's quiet comes first; the others
                                             * once nothing sounds, as the metronome is) */
{
    if (p == P_SPEAKER)
        plat_speaker(x && !spk_page);
    if (p == P_SPEAKER)
        fm.prefs_due = 2;
    if ((p == P_ANIM || p == P_CLOCK || p == P_SAVER) && !fm.prefs_due)
        fm.prefs_due = 1;
}

/* ---------------------------------------------------------------- input --- */
static uint32_t page_at(uint32_t p)      /* a page's place in PAGE_ORDER */
{
    uint32_t i;
    for (i = 0; i < NPAGES && PAGE_ORDER[i] != p; i++)
        ;
    return i;
}

/* a button tapped: its first page shown of the selected track; on one of them, the next, round */
static void page_button(uint32_t b)       /* (a page: the logo goes; PLAY and OCT leave it up) */
{
    uint32_t i, k, from = PAGES[fm.page].button == b && !page_hidden(fm.page) ? page_at(fm.page) : NPAGES - 1u;
    if (b != B_HOME)
        fm.logo = 0;
    fm.help = 0;
    for (k = 1; k <= NPAGES; k++) {
        i = PAGE_ORDER[(from + k) % NPAGES];
        if (PAGES[i].button == b && !page_hidden(i)) {
            fm.page = (uint8_t)i;
            if (i == PG_SCENES)
                fm.pick = fm.scene;
            return;
        }
    }
}

/* a page button held and SELECT turned: that button's pages, a detent each, as its taps go (the hidden ones skipped),
 * round; from another button's page, its first (turned back, its last); the button does nothing more when let go, and
 * its hold, if it has acted already, is undone (hold_undo). SAVE's pages apart */
static uint32_t page_held(void)          /* the page button held (the last pressed), NB: none */
{
    uint32_t p, b, got = NB;
    for (p = 0; p < NPAGES; p++) {
        b = PAGES[p].button;
        if (b != B_SAVE && b_held(b) && (got == NB || fm.hold_at[b] - fm.hold_at[got] < 0x80000000u))
            got = b;
    }
    return got;
}

static void page_turn(uint32_t b, int32_t d)
{
    uint32_t i = PAGES[fm.page].button == b ? page_at(fm.page) : d > 0 ? NPAGES - 1u : 0u, k;
    for (; d; d += d > 0 ? -1 : 1)
        for (k = 0; k < NPAGES; k++) {
            i = (i + (d > 0 ? 1u : NPAGES - 1u)) % NPAGES;
            if (PAGES[PAGE_ORDER[i]].button == b && !page_hidden(PAGE_ORDER[i]))
                break;
        }
    fm.page = PAGE_ORDER[i];
}

/* what a page button's hold did, undone once SELECT takes the button: FREEZE and the arpeggio's LATCH back as they
 * were (the notes LATCH off let go, again: arp_was; LATCH on, the layer's PLAYS as it was, its sequence playing or not,
 * recording or not, and the page), COMPARE over, SWELL down (swell_update); its words gone. HOME's help stays: SELECT
 * turns its pages */
static struct {
    arp_t *a;                            /* the layer's: ARP held acts on the one the keys play */
    int8_t held[16];
    float prs[16];
    uint8_t n;
    uint8_t typed;                       /* LATCH on turned PLAYS to ARP: as it was, its sequence, RECORD, the page */
    uint8_t type, run, rec, page;
} arp_was;
static void say_clear(void);             /* (the screen, below) */
static void say_red(void);
static void say_both(void);
static void say_amt(uint32_t slot);

static void hold_undo(uint32_t b)
{
    if (!(fm.hold_acted >> b & 1u))
        return;
    fm.hold_acted &= ~(1u << b);
    if (b == B_FX) {
        param_set(P_FREEZE, !st.v[P_FREEZE]);
    } else if (b == B_ARP) {
        arp_t *a = arp_was.a;
        int bs = a == &barp;
        a->latch ^= 1u;
        if (a->latch) {
            memcpy(a->held, arp_was.held, sizeof a->held);
            memcpy(a->prs, arp_was.prs, sizeof a->prs);
            a->n = arp_was.n;
            a->dirty = 1;
        } else {
            arp_keep_held(a);
        }
        lv_set(bs ? P_BA_LATCH : P_GA_LATCH, a->latch);   /* (ARP II's LATCH as it is: its notes kept back, not told) */
        if (arp_was.typed) {
            arp_was.typed = 0;
            param_set(bs ? P_BQ_ON : P_SQ_TYPE, arp_was.type);
            if (arp_was.type == SQT_SEQ && !arp_was.run) {   /* (it was stopped: stopped again) */
                if (bs) {
                    bseq_stop();
                } else {
                    seq_note_off();
                    sq.run = 0;
                }
            }
            param_set(bs ? P_BQ_REC : P_SQ_REC, arp_was.rec);
            fm.page = arp_was.page;
        }
    } else if (b == B_EDIT) {
        st.compare = 0;
        st.engine_dirty = 1;
        fm.edit_map = 0;
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
    param_set((int)(P_L1_AMT1 + 4u * (nx - 1u) + at), PD[P_L1_AMT1].def);   /* (its AMOUNT x1.00: adjusted after, SEL + K) */
    if (!st.v[LFO_BASE[nx - 1u] + 2u])
        param_set((int)(LFO_BASE[nx - 1u] + 2u), 32);   /* (DEPTH: a quarter) */
    m[0] = 0;
    text_cat(m, "> LFO ");
    int_text((int32_t)nx, m + 6);
    app_say(LFO_TGT_NAME[t], m);
}

#if PLAT_SMK37
/* an LFO's pad held (the SMK-37's, pad_lfo below: 1 + its LFO, 0 none) and an encoder turned: as LFO held, but onto that
 * LFO n alone: on, into its first free TARGET (off the others), DEPTH at 0 up to a quarter, its AMOUNT x1.00; back,
 * off it */
static uint8_t lfo_pad_held;
static uint8_t pad_amt_held;             /* the top row's last pad (base's 7) held: the knobs LFO AMOUNTs, as SEL held */
static void lfo_pad_route(uint32_t p, uint32_t n, int32_t d)
{
    uint32_t t, i, k, at = 4, on = 0, b = LFO_BASE[n - 1u];
    char m[12];
    for (t = 1; p < NPARAM && t < NLFO_TGT && LFO_TGT[t] != p; t++)
        ;
    if (p >= NPARAM || t >= NLFO_TGT) {
        app_say(p < NPARAM ? PD[p].name : "LFO", "NOT AN LFO TARGET");
        return;
    }
    m[0] = 0;
    text_cat(m, "> LFO ");
    int_text((int32_t)n, m + 6);
    for (k = 0; k < 4u; k++)
        on |= st.v[b + 4u + k] == t;
    if (d < 0) {                         /* back: off LFO n (another's as it is) */
        if (!on)
            return;
        for (k = 0; k < 4u; k++)
            if (st.v[b + 4u + k] == t)
                param_set((int)(b + 4u + k), 0);
        app_say(LFO_TGT_NAME[t], "> NO LFO");
        say_red();
        return;
    }
    if (on) {
        app_say(LFO_TGT_NAME[t], m);
        return;
    }
    for (k = 0; k < 4u && at == 4u; k++)
        if (st.v[b + 4u + k] == 0)
            at = k;
    if (at == 4u) {
        m[0] = 0;
        text_cat(m, "LFO ");
        int_text((int32_t)n, m + 4);
        text_cat(m, " FULL");
        app_say(LFO_TGT_NAME[t], m);
        return;
    }
    for (i = 0; i < NLFO; i++)           /* off the others */
        for (k = 0; k < 4u; k++)
            if (st.v[LFO_BASE[i] + 4u + k] == t)
                param_set((int)(LFO_BASE[i] + 4u + k), 0);
    param_set((int)(b + 4u + at), (int32_t)t);
    param_set((int)(P_L1_AMT1 + 4u * (n - 1u) + at), PD[P_L1_AMT1].def);
    if (!st.v[b + 2u])
        param_set((int)(b + 2u), 32);    /* (DEPTH: a quarter) */
    app_say(LFO_TGT_NAME[t], m);
}
#endif

static int32_t accel(uint32_t e, int32_t d)
{
    uint32_t now = plat_ms(), dt = now - fm.enc_at[e];
    fm.enc_at[e] = now;
    return dt < 25u ? d * 4 : dt < 60u ? d * 2 : d;
}

/* a factor, x1.00 at its default (the engine writes it "×1.00": TAPE SPEED, the LFOs' AMOUNTs) */
static int times_p(int p)
{
    char t[24];
    value_text(p, PD[p].def, t);
    return t[0] == '\xC3' && t[1] == '\x97';
}

/* a factor's knob (a page's: TAPE SPEED) turned from was to v: across its x1.00 (quickly, the detents counting double or
 * more), it stops there; the next detent goes on. x1.00 is easy to land on */
static int32_t times_catch(int p, int32_t was, int32_t v)
{
    int32_t c = (int32_t)PD[p].def;
    return times_p(p) && ((was < c && v > c) || (was > c && v < c)) ? c : v;
}

/* SEL held and an encoder turned: the LFO AMOUNT of the setting under it (on an LFO n TARGETS page: that TARGET slot's),
 * x0.00 .. x1.00 .. x5.00; the header says it large, a bar from its middle (x1.00) under it (ui.c amt_head, say.amt). A setting on no LFO: NO LFO;
 * an empty TARGET: NO TARGET */
static uint32_t tgt_slot(uint32_t p)    /* a TARGET setting's slot, LFO 1's TARGET 1 .. LFO 3's TARGET 4: 0 .. 11; 12 none */
{
    uint32_t i;
    for (i = 0; i < NLFO; i++)
        if (p >= LFO_BASE[i] + 4u && p < LFO_BASE[i] + 8u)
            return 4u * i + p - LFO_BASE[i] - 4u;
    return 12u;
}

static void lfo_amount(uint32_t e, uint32_t p, int32_t d)
{
    uint32_t t, i, k, slot = tgt_slot(p);
    char m[16], v[24];
    if (slot < 12u && !st.v[p]) {
        app_say(PD[p].name, "NO TARGET");
        return;
    }
    for (t = 1; slot == 12u && p < NPARAM && t < NLFO_TGT && LFO_TGT[t] != p; t++)
        ;
    for (i = 0; slot == 12u && p < NPARAM && t < NLFO_TGT && i < NLFO; i++)   /* the slot it is on */
        for (k = 0; k < 4u && slot == 12u; k++)
            if (st.v[LFO_BASE[i] + 4u + k] == t)
                slot = 4u * i + k;
    if (slot == 12u) {
        app_say(p < NPARAM ? PD[p].name : "LFO", "NO LFO");
        return;
    }
    p = P_L1_AMT1 + slot;
    param_set((int)p, (int32_t)st.v[p] + accel(e, d));
    m[0] = 0;
    text_cat(m, "LFO ");
    int_text((int32_t)(slot / 4u + 1u), m + 4);
    text_cat(m, " AMOUNT");
    value_text((int)p, st.v[p], v);
    app_say(m, v + 2);                   /* (the number: the engine's × is UTF-8, the header draws its own, amt_head) */
    say_amt(slot);
}

/* a long list of names turned (a preset, a SCALE, a CHORD, an LFO's TARGET, MASTER EQ's PRESET): the grid of names over
 * the screen (ui.c grid_screen), a detent a name, no acceleration; the choice sounds once the knob has rested 250 ms, the
 * grid goes 1.2 s after the last detent, or at once on a button (it then does its own job), SELECT or another knob; OCT+
 * and OCT- turn its pages, the octave as it is. OCT- held and a track's preset turned: picked unheard, what plays going
 * on as it is, then taken when OCT- is let go; its press's octave step undone (OCT- was a hand then) */
#define NG_ROWS 10u
#define NG_PER (2u * NG_ROWS)
static struct {
    int8_t oct;                          /* the octave before OCT- went down */
} pv;
static struct {
    uint8_t on;
    prm_t p;
    uint8_t v, pick, due, enc, drawn;    /* v: its place in the list; due: not yet heard */
    uint32_t at;                         /* the last detent */
} ng;
#ifdef PLAT_HOST
static uint8_t test_no_grid;             /* (basic.txt's older parts: these settings as any knob: nogrid) */
#else
#define test_no_grid 0
#endif
/* a sequence's STEP, and its SEQ LENGTH: STEP goes no further than the last step */
static int step_len_p(int p) { return p == P_SQ_STEP ? P_SQ_LEN : p == P_BQ_STEP ? P_BQ_STEPS : p == P_DQ_STEP ? P_DQ_LEN : -1; }
static int len_step_p(int p) { return p == P_SQ_LEN ? P_SQ_STEP : p == P_BQ_STEPS ? P_BQ_STEP : p == P_DQ_LEN ? P_DQ_STEP : -1; }
/* an LFO's TARGET in the grid: five pages, in sections, what each moves together; a scene keeps LFO_TGT's numbers, this is
 * the order they are shown in. A slot: a number of LFO_TGT, TH_NONE, or a section's name (TH(n), TGT_HEAD) */
#define NG_TPAGES (5u + FRS_LEAD)       /* (LEAD's on a sixth, the SMK-37's: card #144) */
#define TH_NONE (-1)
#define TH(n) (-2 - (n))
#if FRS_LEAD
static const char *const TGT_HEAD[12] = {"DRONE", "CHOIR", "PARTIALS", "BREATH", "GLINT", "GLINT II", "BASS", "GLOBAL",
                                         "LOFI TAPE", "LEAD", "LEAD ENV", "LEAD DELAY"};
#else
static const char *const TGT_HEAD[9] = {"DRONE", "CHOIR", "PARTIALS", "BREATH", "GLINT", "GLINT II", "BASS", "GLOBAL",
                                        "LOFI TAPE"};
#endif
static const int8_t TGT_SLOTS[NG_TPAGES * NG_PER] = {
    0, TH(0), 1, 2, 3, 35, 4, 29, 43, TH_NONE,                                   /* OFF; TIMBRE .. WARMTH, level, pan */
    TH_NONE, TH(1), 5, 8, 9, 10, 11, 47, TH_NONE, TH_NONE,
    TH(2), 6, 12, 13, 14, 15, 16, 17, 18, TH_NONE,
    TH(3), 7, 19, 20, 21, 22, 23, 24, TH_NONE, TH_NONE,
    TH(4), 30, 45, 25, 26, 27, 28, TH_NONE, TH_NONE, TH_NONE,                    /* level, pan, its sound */
    TH(5), 48, 49, 50, 51, 36, 37, 38, TH_NONE, TH_NONE,                         /* ADSR, CHANCE, SEQ LEN, SPARKLE */
    TH(6), 34, 44, 31, 32, 33, 52, 53, 54, 55,
    TH(7), 41, 42, 39, 40, 46, TH_NONE, TH_NONE, TH_NONE, TH_NONE,
    TH(8), 56, 57, 58, 59, 60, 61, TH_NONE, TH_NONE, TH_NONE,                    /* LOFI TAPE's two, LOFI TAPE II's four */
#if FRS_LEAD
    TH(9), 62, 63, 64, 65, 66, 67, 68, 70, 79,                                   /* LEAD: its sound, its level */
    TH(10), 69, 71, 72, 73, 74, 80, 81, 82, 83,                                  /* FILTER ENV, the ADSR, the
                                                                                  * filter's (admin #619) */
    TH(11), 75, 76, 77, 78, TH_NONE, TH_NONE, TH_NONE, TH_NONE, TH_NONE,         /* its delay, SPACE */
#else
    TH_NONE, TH_NONE, TH_NONE, TH_NONE, TH_NONE, TH_NONE, TH_NONE, TH_NONE, TH_NONE, TH_NONE,
#endif
};
static uint32_t tgt_at(uint32_t i)       /* the number at place i in the list */
{
    uint32_t k;
    for (k = 0; k < NG_TPAGES * NG_PER; k++)
        if (TGT_SLOTS[k] >= 0 && !i--)
            return (uint32_t)TGT_SLOTS[k];
    return 0;
}
static uint32_t tgt_pos(uint32_t t)      /* number t's place in the list */
{
    uint32_t k, i = 0;
    for (k = 0; k < NG_TPAGES * NG_PER; k++)
        if (TGT_SLOTS[k] >= 0) {
            if ((uint32_t)TGT_SLOTS[k] == t)
                return i;
            i++;
        }
    return 0;
}
static uint32_t tgt_page(uint32_t i)     /* the page place i is on */
{
    uint32_t k;
    for (k = 0; k < NG_TPAGES * NG_PER; k++)
        if (TGT_SLOTS[k] >= 0 && !i--)
            return k / NG_PER;
    return 0;
}
/* setting p as the screen shows it: during ANCHOR's MORPH back as it is heard (its value already the anchor's, the
 * glide still under way: morph_off), so that the page moves with the sound; else its value */
static float vh(uint32_t p)
{
    return (float)st.v[p] + (mo.on && mo.anc && p < NMORPH ? morph_off[p] : 0.0f);
}
static uint32_t vh_n(uint32_t p)          /* ... as a value of its own (gliding: rounded, in its range) */
{
    float v;
    if (!mo.on || !mo.anc || p >= NMORPH)
        return st.v[p];
    v = vh(p) + 0.5f;
    return v < 0.0f ? 0u : v > (float)PD[p].max ? PD[p].max : (uint32_t)v;
}
/* GLOBAL's TEMPO while a MIDI clock leads the grid (the engine's own grid_clocked: CLOCK on AUTO, a clock in this half
 * second; INTERNAL lets it go at once): TEMPO (EXT), the clock's tempo greyed, K1 not turning it (card #137). TEMPO
 * stays as it was (a STORE keeps it; the clock gone, it is back), this only the screen's. The clock's tempo in whole
 * BPM, the one shown moved only when the clock's is over HYST off it: the USB's jitter and the clocks read together
 * (clock_in's filter: within some 0.2 %) never make it blink 79/80/81. 0: no clock */
#define EXT_BPM_HYST 0.7f
static uint32_t ext_bpm(void)
{
    static uint32_t shown;
    float b;
    if (!grid_clocked(eng_ms()))
        return shown = 0u;
    b = 2500.0f / grid.clk_ms;
    if (!shown || b > (float)shown + EXT_BPM_HYST || b < (float)shown - EXT_BPM_HYST)
        shown = (uint32_t)(b + 0.5f);
    return shown;
}
static uint32_t pv_shown(int p)
{
    if (p == P_TEMPO && ext_bpm())       /* (a MIDI clock's: its gauge in TEMPO's range) */
        return ext_bpm() < 30u ? 0u : ext_bpm() - 30u > PD[P_TEMPO].max ? PD[P_TEMPO].max : ext_bpm() - 30u;
    if (!ng.on || ng.p != (prm_t)p || ng.v > PD[p].max)
        return vh_n((uint32_t)p);
    return PD[p].fmt == F_LTGT ? tgt_at(ng.v) : ng.v;
}

static int list_track(uint32_t p) { return p == P_DR_PRESET ? 0 : p == P_GL_PRESET ? 1 : p == P_BS_MODEL ? 2 : -1; }
#if FRS_LEAD                             /* LEAD's PRESET (card #144): the others' browser, its own, then the player's (admin
                                          * #628: OBJ_LEAD's, ldu_*) */
static int list_lead(uint32_t p) { return p == P_LD_PRESET; }
static int list_up(uint32_t p) { return list_lead(p) ? 3 : list_track(p); }   /* whose of the player's it lists, -1 */
static int list_param(uint32_t p)
{
    return list_track(p) >= 0 || list_lead(p) || p == P_SCALE || p == P_CHORD || p == P_EQ_PRESET || PD[p].fmt == F_LTGT;
}
#else
#define list_lead(p) 0
#define list_up(p) list_track(p)
static int list_param(uint32_t p)
{
    return list_track(p) >= 0 || p == P_SCALE || p == P_CHORD || p == P_EQ_PRESET || PD[p].fmt == F_LTGT;
}
#endif
static uint32_t list_n(uint32_t p)
{
    int t = list_up(p);
    return PD[p].max + 1u + (t >= 0 ? up_n((uint32_t)t) : 0u);
}
static uint32_t list_cur(uint32_t p)     /* where the setting is in its list now */
{
    int t = list_up(p), u = t >= 0 ? up_in_use((uint32_t)t) : -1;
    if (PD[p].fmt == F_LTGT)
        return tgt_pos(st.v[p]);
    return u >= 0 ? PD[p].max + 1u + (uint32_t)u : st.v[p];
}
static void list_name(uint32_t p, uint32_t i, char *o)
{
    if (i <= PD[p].max)
        value_text((int)p, (uint8_t)(PD[p].fmt == F_LTGT ? tgt_at(i) : i), o);
    else
        up_name((uint32_t)list_up(p), i - PD[p].max - 1u, o);
}
static void list_take(uint32_t p, uint32_t i)
{
    int t = list_track(p);
    if (PD[p].fmt == F_LTGT)
        i = tgt_at(i);
#if FRS_LEAD
    if (list_lead(p)) {                  /* LEAD's: the player's (ldu_take); a factory one, the same again after one of
                                          * theirs its sound back */
        int again = up_cur[3] >= 0 && st.v[p] == i;
        if (i > PD[p].max) {
            ldu_take(i - PD[p].max - 1u);
            return;
        }
        if (again)
            st.v[p] = (uint8_t)(i ? i - 1u : i + 1u);
        up_cur[3] = -1;
        param_set((int)p, (int32_t)i);
        if (again)                       /* (its sound back: the LEAD as it is, the other side's PRESET that one
                                          * already, card #152) */
            lv_lead(0u);
        return;
    }
#endif
    if (i > PD[p].max) {
        uint32_t u = i - PD[p].max - 1u;
        eng_preset_apply(up_at((uint32_t)t, u), up_bytes((uint32_t)t));
        lv_preset(up_at((uint32_t)t, u), up_bytes((uint32_t)t));
        up_cur[t] = (int8_t)u;
        return;
    }
    if (t >= 0) {
        if (up_cur[t] >= 0 && st.v[p] == i) {   /* (the same factory one again: its sound back) */
            st.v[p] = (uint8_t)(i ? i - 1u : i + 1u);
            lv_set(p, st.v[p]);          /* (told as a turn: the other side's sound comes back the same) */
        }
        up_cur[t] = -1;
    }
    param_set((int)p, (int32_t)i);
}

/* an LFO's TARGET, a track's preset: heard as it is turned (the choice sounds once the knob has rested), SEL keeps it, FX
 * brings back the sound as it was before the grid (gk), only those two lit, the rest wait, no closing by itself (OCT- held,
 * a preset picked unheard: OCT-'s, as ever) */
#if FRS_LEAD
static int grid_asks(void) { return ng.on && !ng.pick && (PD[ng.p].fmt == F_LTGT || list_track(ng.p) >= 0 || list_lead(ng.p)); }
#else
static int grid_asks(void) { return ng.on && !ng.pick && (PD[ng.p].fmt == F_LTGT || list_track(ng.p) >= 0); }
#endif
static struct {
    prm_t p;                             /* the setting, */
    uint8_t old;                         /* its value before */
    int8_t up;                           /* the player's preset in use before (-1 none) */
    uint32_t n;                          /* the track's sound before, as a preset (0: an LFO's TARGET, its value alone) */
    uint8_t b[PRESET_BYTES];
#if FRS_LEAD
    uint8_t ld[P_LD_FRELEASE - P_LD_WAVE + 1];   /* LEAD's settings before (its PRESET's browser) */
#endif
} gk;
static void grid_keep(void)              /* the grid opened: the sound as it is, for FX */
{
    int t = list_track(ng.p);
    gk.p = ng.p;
    gk.old = st.v[ng.p];
    gk.up = t >= 0 ? up_cur[t] : -1;
    gk.n = t >= 0 ? eng_preset_capture((uint32_t)t, 0, gk.b) : 0u;
#if FRS_LEAD
    if (list_lead(ng.p)) {
        memcpy(gk.ld, &st.v[P_LD_WAVE], sizeof gk.ld);
        gk.up = up_cur[3];
    }
#endif
}
static void grid_back(void)              /* FX: the sound as it was */
{
    int t = list_track(gk.p);
#if FRS_LEAD
    if (list_lead(gk.p)) {               /* LEAD's: its settings as they were, tweaks and all (MODE turned: let go) */
        uint32_t i;
        for (i = 0; i < sizeof gk.ld; i++)
            if (P_LD_WAVE + i != P_LD_PRESET)
                param_set((int)(P_LD_WAVE + i), gk.ld[i]);
        st.v[P_LD_PRESET] = gk.ld[P_LD_PRESET - P_LD_WAVE];
        st.dirty = (uint8_t)sound_changed();   /* (with its PRESET back: as it was, card #152) */
        lv_lead(0u);                     /* (that PRESET in place: the LEAD as it is) */
        up_cur[3] = gk.up;
        return;
    }
#endif
    if (gk.n) {
        eng_preset_apply(gk.b, gk.n);
        lv_preset(gk.b, gk.n);
    }
    if (t >= 0) {
        st.v[gk.p] = gk.old;             /* (its name back, the sound the preset's bytes) */
        up_cur[t] = gk.up;
    } else {
        param_set(gk.p, gk.old);
    }
}

static void grid_close(void)
{
    if (ng.due)
        list_take(ng.p, ng.v);
    ng.due = ng.on = 0;
    fm.touched = ng.p;
    fm.touched_at = plat_ms();
}

static void grid_tick(void)              /* each pass: the sound after the rest, the grid gone */
{
    uint32_t t;
    if (!ng.on)
        return;
    t = plat_ms() - ng.at;
    if (ng.due && t >= 250u) {
        ng.due = 0;
        list_take(ng.p, ng.v);
    }
    if (!ng.pick && !grid_asks() && t >= 1200u)
        grid_close();
}

static void grid_turn(uint32_t p, uint32_t e, int32_t d)
{
    int pick = b_held(B_OCTDN) && !b_held(B_OCTUP) && (list_track(p) >= 0 || list_lead(p));
    if (!ng.on || ng.p != (prm_t)p) {
        if (ng.on)
            grid_close();
        ng.on = 1;
        ng.p = (prm_t)p;
        ng.v = (uint8_t)list_cur(p);
        ng.pick = (uint8_t)pick;
        if (grid_asks())
            grid_keep();
        if (pick) {                      /* the first detent: OCT- a hand, its step undone; picking from what plays */
            fm.octave = pv.oct;
            b_used(B_OCTDN);
            app_say("PICKING", "LET GO OCT-");
        }
    }
    ng.v = (uint8_t)clamp((int32_t)ng.v + d, 0, (int32_t)list_n(p) - 1);
    ng.enc = (uint8_t)e;
    ng.due = !ng.pick;
    ng.at = plat_ms();
    ng.drawn = 0;
}

static void grid_page(int32_t d)         /* OCT+, OCT-: a page on or back */
{
    int32_t n = (int32_t)list_n(ng.p), v = (int32_t)ng.v + d * (int32_t)NG_PER;
    if (PD[ng.p].fmt == F_LTGT) {        /* (the next section's first) */
        uint32_t to = (uint32_t)clamp((int32_t)tgt_page(ng.v) + d, 0, (int32_t)NG_TPAGES - 1);
        for (v = 0; v < n - 1 && tgt_page((uint32_t)v) != to; v++)
            ;
    }
    ng.v = (uint8_t)clamp(v, 0, n - 1);
    ng.due = !ng.pick;
    ng.at = plat_ms();
    ng.drawn = 0;
}

/* PLAY's light: the transport's (a sequence on: lit while one runs, or the arpeggio plays), else lit while anything is
 * held (the drones, the bass, the arpeggio's notes, a key, a MIDI note); never for a tail alone: a stop puts it out at
 * once, the sound fading on as it does */
static int play_lit(void)
{
    uint32_t k, midi = mb_n;
    if (st.v[P_SQ_TYPE] == SQT_SEQ || st.v[P_BQ_ON] == SQT_SEQ || st.v[P_DQ_ON])
        return sq.run || bq.run || dq.run || (st.v[P_SQ_TYPE] == SQT_ARP && !arp.paused && arp.n) ||
               (st.v[P_BQ_ON] == SQT_ARP && !barp.paused && barp.n);
    for (k = 0; k < 4u; k++)
        midi |= mid_on[k] | mgl[k];
    return holding_any() || keys_prev || midi;
}

/* the three left encoders, the performance's hands: SELECT the track, PRESETS its preset, ALGORITHM its filter (the
 * drone's TIMBRE, GLINT's BRIGHT, the bass's FILTER); OCT- held, ALGORITHM is MASTER FILTER's CUTOFF */
#if FRS_LEAD                             /* (LEAD: its PRESET; its FILTER) */
static uint32_t track_preset(void) { return st.lead ? P_LD_PRESET : st.glint ? P_GL_PRESET : st.bass ? P_BS_MODEL : P_DR_PRESET; }
static uint32_t track_algo(void) { return st.lead ? P_LD_CUT : st.glint ? P_GL_BRIGHT : st.bass ? P_BS_FILTER : P_TIMBRE; }
#else
static uint32_t track_preset(void) { return st.glint ? P_GL_PRESET : st.bass ? P_BS_MODEL : P_DR_PRESET; }
static uint32_t track_algo(void) { return st.glint ? P_GL_BRIGHT : st.bass ? P_BS_FILTER : P_TIMBRE; }
#endif
static uint32_t macro_of(uint32_t e)
{
    return e == EN_SELECT ? P_KB : e == EN_PRESET ? track_preset()
         : e == EN_ALGO ? (b_held(B_GLO) ? P_FL_CUT : track_algo()) : NPARAM;
}

/* SELECT turned: the track, its page followed on EDIT, ENV, ARP and SEQ (the same place, the first if hidden; EDIT: the
 * first) at once, never the play page between (the track large on the play page alone: ui.c overlay_on, admin #866);
 * elsewhere the page stays. OCT- held: the keys change track, the page stays (OCT- a hand, its step undone). A
 * recording: not at all */
static void track_turn(int32_t d)
{
    uint32_t was = st.v[P_KB], t;
    int follow = !(b_held(B_OCTDN) && !b_held(B_OCTUP));
    if (rec_any()) {
        app_say("RECORDING", "REC: OFF");
        return;
    }
    if (!follow && !(fm.hold_done >> B_OCTDN & 1u)) {
        fm.octave = pv.oct;
        b_used(B_OCTDN);
    }
    param_set(P_KB, (int32_t)was + d);
    fm.touched = P_KB;
    fm.touched_enc = EN_SELECT;
    fm.touched_at = plat_ms();
    t = st.v[P_KB];
    if (t == was || !follow || PAGES[fm.page].track == T_ANY)
        return;
    fm.page = (uint8_t)track_page(fm.page, t);
    page_back();
}

/* ARP's or SEQ's K1: ON sets the track's PLAYS, the other turned off with it (the header says so); OFF, KEYS */
static void ways_turn(uint32_t k, int32_t d)
{
    uint32_t bs = PAGES[fm.page].track == 2u, tp = bs ? P_BQ_ON : P_SQ_TYPE, way = k == K_ARP ? SQT_ARP : SQT_SEQ;
    uint32_t was = st.v[tp];
    if (d > 0 && was != way) {
        param_set((int)tp, (int32_t)way);
        if (was) {
            app_say(k == K_ARP ? "ARP ON" : "SEQ ON", k == K_ARP ? "SEQ OFF" : "ARP OFF");
            say_both();
        }
    } else if (d < 0 && was == way) {
        param_set((int)tp, 0);
    }
}

/* KEYS' K1 .. K4, before they are set. NOTES: no CHORD on the bass, or GLINT on its SEQUENCE (slate, not turned: the drone's
 * steps hold chords, its stays); to CHORD with HARMONY OFF, HARMONY on too. LATCH turned OFF lets the track's notes go: a second detent the same way,
 * within 2 s. 1: the detent stops here */
static int keys_turn(int p, int32_t d)
{
    uint32_t i = (uint32_t)(p - P_KY_DR_HARM) & 3u, t = (uint32_t)(p - P_KY_DR_HARM) / 4u;
    if (i == 1u) {
        if (d > 0 && !st.v[P_KY_DR_HARM + 4u * t])
            param_set((int)(P_KY_DR_HARM + 4u * t), 1);
    } else if (i == 2u && st.v[p] && d < 0) {
        if (fm.keys_arm != (int8_t)p || plat_ms() - fm.keys_arm_at > 2000u) {
            fm.keys_arm = (int8_t)p;
            fm.keys_arm_at = plat_ms();
            app_say("LATCH", "TURN AGAIN");
            return 1;
        }
        fm.keys_arm = 0;
    }
    return 0;
}

static void encoders(void)
{
    uint32_t e, b;
    for (e = 0; e < NE; e++) {
        int32_t d = enc_read((int)e), p;
        if (!d || welcome_knob())        /* (Learn's welcome up: closed, the turn nothing more) */
            continue;
        spk_turned |= 1u << e;           /* (the screen reader says what it turned: speak.c) */
        if (fm.help && e == EN_SELECT) { /* the help up: SELECT turns its pages */
            fm.help_page = (uint8_t)clamp((int32_t)fm.help_page + (d > 0 ? 1 : -1), 0, HELP_PAGES - 1);
            continue;
        }
        fm.help = 0;
        fm.logo = 0;
        if (grid_asks() && (e == EN_SELECT || (e < EN_K1 && macro_of(e) != ng.p)))
            continue;
        if (ng.on && e == EN_SELECT)
            grid_close();
        if (e == EN_SELECT && (b = page_held()) < NB) {   /* a page button held: its pages */
            b_used(b);
            hold_undo(b);
            page_turn(b, d);
            continue;
        }
#if PLAT_SMK37
        if (lfo_pad_held && e >= EN_K1) {    /* an LFO's pad held (the SMK-37's): the setting under it onto that LFO */
            lfo_pad_route(fm.page == PG_SCENES ? NPARAM : page_knob(fm.page, (uint32_t)(e - EN_K1)), lfo_pad_held, d);
            continue;
        }
        if (pad_amt_held && e >= EN_K1 && fm.page != PG_SAVE && fm.page != PG_SAVEAS) {   /* its AMOUNT pad held: as SEL
                                                                                          * held, the setting's LFO
                                                                                          * AMOUNT (on none: NO LFO) */
            lfo_amount(e, fm.page == PG_SCENES ? NPARAM : page_knob(fm.page, (uint32_t)(e - EN_K1)), d);
            continue;
        }
#endif
        if (b_held(B_LFO) && e >= EN_K1) {   /* LFO held: the setting under it onto an LFO, the next, or off */
            b_used(B_LFO);
            lfo_route(fm.page == PG_SCENES ? NPARAM : page_knob(fm.page, (uint32_t)(e - EN_K1)), d);
            continue;
        }
        if (b_held(B_SEL) && e >= EN_K1 && fm.page != PG_SAVE && fm.page != PG_SAVEAS) {   /* SEL held: its LFO AMOUNT
                                                                                          * (SEL a hand: no page on its
                                                                                          * release) */
            b_used(B_SEL);
            lfo_amount(e, fm.page == PG_SCENES ? NPARAM : page_knob(fm.page, (uint32_t)(e - EN_K1)), d);
            continue;
        }
        if (fm.page == PG_SCENES && (e == EN_K1 || e == EN_SELECT)) {   /* the slots, NEW SHARD above the first:
                                                                          * SELECT as K1 (KEYS stay as they are) */
            p = clamp((fm.pick >= NSCENES ? -1 : (int32_t)fm.pick) + d, -1, NSCENES - 1);
            fm.pick = (uint8_t)(p < 0 ? SC_NEW : p);
            continue;
        }
        if (fm.page == PG_SAVE && e == EN_K1) {   /* LOAD | SAVE */
#if !PLAT_SMK37                          /* (the SMK-37: its pads 1 and 2 are LOAD and SAVE, K1 nothing: pad_taps) */
            sv.load = d < 0;
#endif
            continue;
        }
        if (fm.page == PG_SAVEAS && e == EN_K1) {   /* WHAT */
            sv.what = (uint8_t)clamp((int32_t)sv.what + d, 0, 3 + FRS_LEAD);
            into_start();
            continue;
        }
        if (fm.page == PG_SAVEAS && e == EN_K2) {   /* INTO */
            sv.into = (uint8_t)clamp((int32_t)sv.into + d, (int32_t)into_min(),
                                     sv.what ? (int32_t)up_n(sv.what - 1u) : (int32_t)NSCENES - 1);
            continue;
        }
        if (e == EN_SELECT) {
            track_turn(d);
            continue;
        }
        if (e < EN_K1) {
            p = (int32_t)macro_of(e);
            if (p == P_FL_CUT) {         /* (GLO a hand: no page on its release; the filter OFF: on, or nothing would move) */
                b_used(B_GLO);
                if (!st.v[P_FL_TYPE])
                    param_set(P_FL_TYPE, 1);
            }
        } else {
            p = page_knob(fm.page, (uint32_t)(e - EN_K1));
            if (p == (int32_t)K_ARP || p == (int32_t)K_SEQ) {
                ways_turn((uint32_t)p, d);
                continue;
            }
            if (p == (int32_t)K_IDLE) {      /* IDLE TIME (turned OFF too: slate), kept as SCREENSAVER is */
                saver.min = (uint8_t)clamp((int32_t)saver.min + accel(e, d), 1, 60);
                saver.min_at = plat_ms() | 1u;
                if (!fm.prefs_due)
                    fm.prefs_due = 1;
                continue;
            }
            if (p == (int32_t)K_HEAD) {      /* HEADER: right DARK, left INVERT, kept as IDLE TIME is */
                hdr.dark = d > 0;
                hdr.at = plat_ms() | 1u;
                if (!fm.prefs_due)
                    fm.prefs_due = 1;
                continue;
            }
            if (p == (int32_t)K_TUNE) {      /* TUNE A4: a hertz a detent (quicker, more), stopping at 432 and 440 */
                a4_take((uint32_t)a4_catch(a4.hz, clamp((int32_t)a4.hz + accel(e, d), (int32_t)A4_LO, (int32_t)A4_HI)));
                a4.at = plat_ms() | 1u;
                if (!fm.prefs_due)
                    fm.prefs_due = 1;
                continue;
            }
            if (p >= NPARAM)                 /* an empty slot */
                continue;
            if (p == P_TEMPO && ext_bpm())   /* TEMPO (EXT): the MIDI clock's, nothing to turn */
                continue;
        }
#if FRS_LEAD
        if ((fm.page == PG_MIXER || (fm.page == PG_NUAGE3 && e == EN_K4)) && b_held(B_OCTDN) && !b_held(B_OCTUP) &&
            e >= EN_K1) {                /* MIXER, OCT- held: K1 .. K3 each track's PAN, K4 the LEAD's level still (it
                                          * has none); CLOUD III's K4, the cloud's (its LEVEL there: admin #744) (OCT- a
                                          * hand, its step undone) */
#else
        if (fm.page == PG_MIXER && b_held(B_OCTDN) && !b_held(B_OCTUP) && e >= EN_K1) {   /* MIXER, OCT- held: K1 .. K4
                                                                                        * each track's PAN (OCT- a
                                                                                        * hand, its step undone) */
#endif
            if (!(fm.hold_done >> B_OCTDN & 1u)) {
                fm.octave = pv.oct;
                b_used(B_OCTDN);
            }
#if FRS_LEAD
            p = fm.page == PG_NUAGE3 ? P_PN_CLOUD : e == EN_K4 ? P_MX_LEAD : P_PN_DRONE + (int)(e - EN_K1);
#else
            p = P_PN_DRONE + (int)(e - EN_K1);
#endif
            param_set(p, (int32_t)st.v[p] + accel(e, d));
            fm.touched = (prm_t)p;
            fm.touched_enc = (uint8_t)e;
            fm.touched_at = plat_ms();
            continue;
        }
        if (grid_asks() && p != (int32_t)ng.p)
            continue;
        if (ng.on && p != (int32_t)ng.p)
            grid_close();
        if (list_param((uint32_t)p) &&
            (!test_no_grid || (b_held(B_OCTDN) && !b_held(B_OCTUP) && list_track((uint32_t)p) >= 0))) {
            fm.touched = (prm_t)p;
            fm.touched_enc = (uint8_t)e;
            fm.touched_at = plat_ms();
            grid_turn((uint32_t)p, e, d);
            continue;
        }
        if (p >= P_KY_DR_HARM && p <= P_KY_BS_STACK && keys_turn(p, d))
            continue;
        {
            int32_t v = times_catch(p, st.v[p], (int32_t)st.v[p] + (PD[p].max > 16u ? accel(e, d) : d));
            int lp = step_len_p(p), sp = len_step_p(p);
            if (!st.v[p] && d > 0 && p == P_SQ_NOTE)   /* (up from a REST: a note of the line, not the range's bottom) */
                v = step_near(seq, st.v[P_SQ_STEP], st.v[P_SQ_LEN] + 1u);
            else if (!st.v[p] && d > 0 && p == P_BQ_NOTE)
                v = step_near(bnote, st.v[P_BQ_STEP], st.v[P_BQ_STEPS] + 1u);
            param_set(p, lp >= 0 ? clamp(v, 0, st.v[lp]) : v);
            if (sp >= 0 && st.v[sp] > st.v[p])   /* (a shorter sequence: STEP back on its last) */
                param_set(sp, st.v[p]);
        }
        fm.touched = (prm_t)p;
        fm.touched_enc = (uint8_t)e;
        fm.touched_at = plat_ms();
        if ((p >= P_KY_DR_HARM && p <= P_KY_BS_STACK) || p == P_METRO || p == P_COUNT_IN ||
            p == P_ARP_SNAP) {           /* (kept with the settings) */
            if (p == P_METRO && st.v[P_METRO])
                fm.metro_on = st.v[P_METRO];
            prefs_later();
        }
    }
}

/* OCT- held, REC, on a SEQ page: the selected track's sequence cleared, whatever it is doing (UNDO TAKE has it). Empty
 * already, nothing: a second press must not take what UNDO TAKE keeps */
static void track_clear(void)
{
    static const uint8_t P[3] = {P_DQ_CLEAR, P_SQ_CLEAR, P_BQ_CLEAR};
    static const char *const W[3] = {"DRONE SEQUENCE", "SEQUENCE", "BASS SEQUENCE"};
    uint32_t t = st.v[P_KB] % 3u, i, n = t == 0u ? sizeof dnote : t == 1u ? sizeof seq : sizeof bnote;
    const uint8_t *s = t == 0u ? &dnote[0][0] : t == 1u ? seq : bnote;
#if FRS_LEAD
    if (st.lead)                         /* (LEAD: no sequence) */
        return;
#endif
    for (i = 0; i < n && !s[i]; i++)
        ;
    if (i == n)
        app_say(W[t], "EMPTY");
    else
        param_set(P[t], 2);
}

/* REC tapped (eng_record_on: OFF, LIVE, STEP, OFF, on the selected track; from silence a bar counted in), a recording
 * begun, the track's steps shown: its SEQ II (else the page staying) */
static void rec_tap(void)
{
    uint32_t layer = st.v[P_KB];         /* (before: RECORD moves the keys, rec_keys) */
    int was = rec_any(), r;
#if FRS_LEAD
    if (st.lead && !was) {               /* (LEAD: played, never recorded: no sequence; a take under way: REC ends it) */
        app_say("REC", "NOT FOR LEAD");
        return;
    }
#endif
    lv_rec(layer);                       /* (first: the take's COUNT-IN and OVERDUB as they are, FST_RECORDING) */
    r = eng_record_on(layer, 1);
    if (r && !was) {
        fm.logo = 0;
        fm.help = 0;
        fm.page = r == 3 ? PG_DSEQ2 : r == 2 ? PG_BSEQ2 : PG_SEQ2;
    }
}

/* PLAY tapped: the transport (eng_play_tap: nothing playing at all, the scene's three tracks); on SCENES, the scene K1
 * points at (MORPH, LOAD), playing at once if nothing played. Tapped twice quick while anything plays (not on SCENES):
 * RELEASE ALL, as held */
static void play_tap(void)
{
    static uint32_t twice;               /* when the last tap came while anything played (0: it did not) */
    uint32_t now = plat_ms() | 1u;
    scene_t sc;
    if (fm.page != PG_SCENES && twice && now - twice < PLAY_TWICE_MS) {   /* tapped twice quick while anything plays
                                                                         * (not on SCENES): RELEASE ALL, as held (the
                                                                         * first tap's stop already made) */
        twice = 0;
        sound_off();
        grid_top(0);
        lv_op(LV_RELEASE, 0, 0);
        app_say("RELEASE ALL", rst_valid ? "HOLD PLAY: RESTORE" : "");
        return;
    }
    twice = fm.page != PG_SCENES && holding_any() ? now : 0u;
    if (fm.page == PG_SCENES) {          /* nothing playing: the scene loaded plays at once, as PLAY tapped plays a
                                          * scene from silence (power-on alone stays quiet); its own notes, not those
                                          * a RELEASE ALL kept for RESTORE */
        int idle = !holding_any();
        scene_go(fm.pick);
        if (idle) {
            rst_valid = 0;
            scene_get(fm.scene, &sc);
            if (eng_scene_again(&sc))
                lv_op(FST_PLAY, 0, 0);   /* (frs_play from silence: the scene given last, again) */
        }
    } else {                             /* nothing playing at all: its three tracks, as PLAY held; else the transport;
                                          * stopping a take ends it, RECORD off */
        int ran = sq.run || bq.run || dq.run || cnt.run;
        scene_get(fm.scene, &sc);
        eng_play_tap(&sc);
        lv_op(FST_PLAY, 0, 0);
        if (ran && rec_any() && !sq.run && !bq.run && !dq.run && !cnt.run) {
            lv_record_off();
            app_say("STOPPED", "RECORD OFF");
        }
    }
}

/* PLAY held: RELEASE ALL, or RESTORE (eng_release_all), the current slot's scene the one restored */
static void play_hold(void)
{
    scene_t sc;
    scene_get(fm.scene, &sc);
    eng_release_all(&sc);
    lv_op(LV_RELEASE_ALL, 0, 0);
    if (rec_any())
        lv_record_off();
}

/* PLAY being held, for the screen: 0 not yet (or not held), else how far to its hold, 0..1 (from 200 ms) */
static float play_holding(void)
{
    uint32_t t = plat_ms() - fm.hold_at[B_PLAY];
    if (!b_held(B_PLAY) || (fm.hold_done >> B_PLAY & 1u) || t < PLAY_SHOW_MS)
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
    case B_HOME:                         /* its page; the help closes; SOUND, KEYS, MIXER, CLOUD SEND, the logo, the river */
        if (fm.help)
            fm.help = 0;
        else if (fm.page == PG_SON && fm.logo) {
            fm.logo = 0;
            fm.logo_until = 0;
        } else if (fm.page == PG_SON) {
            fm.page = PG_KEYS;
        } else if (fm.page == PG_KEYS) {
            fm.page = PG_MIXER;
        } else if (fm.page == PG_MIXER) {   /* (the LEAD's level MIXER's K4: no MIXER II, admin #744) */
            fm.page = PG_CSEND;
        } else if (fm.page == PG_CSEND) {
            fm.page = PG_SON;
            fm.logo = 1;
            fm.logo_until = 0;
        } else {
            fm.logo = 0;
            page_button(B_HOME);
        }
        break;
    case B_SAVE:                         /* LOAD | SAVE; again, straight to WHAT and INTO, whichever K1 points at; SCENES:
                                          * back where it came from */
        if (fm.page == PG_SCENES) {
            flow_back();
        } else if (fm.page == PG_SAVE) {
            sv.load = 0;
            flow_saveas();
        } else if (fm.page != PG_SAVEAS) {
            fm.prev_page = fm.page;
            fm.page = PG_SAVE;
        }
        fm.logo = 0;
        fm.help = 0;
        break;
    case B_SEL:                          /* in the flow: OK, the next step (LOAD: SCENES); else its pages */
        fm.logo = 0;
        if (fm.page == PG_SAVE && sv.load) {
            fm.page = PG_SCENES;
            fm.pick = fm.scene;
        } else if (fm.page == PG_SAVE) {
            flow_saveas();
        } else if (fm.page == PG_SAVEAS) {
            flow_name();
        } else {
            page_button(k);
        }
        break;
    case B_FX:                           /* in the flow: back a step */
        if (fm.page == PG_SAVE) {
            flow_back();
        } else if (fm.page == PG_SAVEAS) {
            fm.page = PG_SAVE;
        } else {
            fm.logo = 0;
            page_button(k);
        }
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
#if PLAT_SMK37
    /* the SMK-37's < and > (above OCTAVE- and OCTAVE+): the page before, after, among its button's (GLINT I, GLINT
     * II...), round, as that button held and SELECT turned (Styves, 2026-11-09) */
    case B_LEFT:
    case B_RIGHT:
        fm.logo = 0;
        fm.help = 0;
        page_turn(PAGES[fm.page].button, k == B_RIGHT ? 1 : -1);
        break;
    case B_KBANK:                        /* the SMK-37's KNOB BANK: the next track (DRONE, GLINT, BASS, round), as SELECT
                                          * turned (Styves, 2026-11-09) */
        track_turn(st.v[P_KB] >= PD[P_KB].max ? -(int32_t)st.v[P_KB] : 1);
        break;
    case B_FBANK: {                      /* the SMK-37's FADER BANK: the selected track's KEYS NOTES, SINGLE NOTE or CHORD
                                          * (to CHORD with HARMONY off: HARMONY on, as KEYS' K2 does); held: its LATCH
                                          * (holds) (Styves, 2026-11-09) */
        uint32_t t = st.v[P_KB], np = P_KY_DR_NOTES + 4u * t;
#if FRS_LEAD
        if (st.lead) {                   /* (LEAD: one note at a time, always) */
            app_say("LEAD", "ONE NOTE");
            break;
        }
#endif
        if (!st.v[np] && !st.v[P_KY_DR_HARM + 4u * t])
            param_set((int)(P_KY_DR_HARM + 4u * t), 1);
        param_set((int)np, !st.v[np]);
        prefs_later();
        app_say("NOTES", st.v[np] ? "CHORD" : "SINGLE NOTE");
        break;
    }
#endif
    default:                             /* its pages */
        fm.logo = 0;
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
            fm.hold_at[k] = now;
            fm.hold_done &= ~(1u << k);
            fm.hold_acted &= ~(1u << k);
        }
}

/* ANCHOR (engine/core.c: what it keeps, how it comes back): OCT- and OCT+ together set it, the octave with it; again,
 * let go at once, MORPH there (ANCHOR_MS); held HOLD_MS, BACK there at once (3.1: the other way round before); during
 * the MORPH, it ends there at once. The screen's frame shows it (ui.c anchor_frame) */
static void anchor_back_to(uint32_t ms)
{
    anchor_back(ms);
    lv_op(FST_ANCHOR_BACK, ms & 0xFFu, ms >> 8 & 0xFFu);
    fm.octave = fm.anc_oct;
    app_say(ms ? "MORPH" : "BACK", "TO " ANCHOR_NAME);
}

static void anchor_press(void)           /* the two down */
{
    fm.anc_both = 1;
    if (anc.on) {                        /* (MORPH or BACK: as they are let go, or held) */
        fm.anc_arm = 1;
        fm.anc_at = plat_ms();
    } else if (mo.on && mo.anc) {
        anchor_back(0);
        lv_op(FST_ANCHOR_BACK, 0, 0);
        app_say("BACK", "TO " ANCHOR_NAME);
    } else {
        anchor_set();
        lv_long(FST_ANCHOR, LV_ANC);     /* (its bytes: frs_anchor_put) */
        fm.anc_oct = fm.octave;
        app_say(ANCHOR_NAME, "SET");
    }
}

static void anchor_keys(uint32_t btn)    /* each pass while the two were down together */
{
    uint32_t both = 1u << B_OCTDN | 1u << B_OCTUP;
    if (!fm.anc_both)
        return;
    if ((btn & both) != both) {          /* one let go: MORPH, if nothing was done held */
        fm.anc_both = 0;
        if (fm.anc_arm)
            anchor_back_to(ANCHOR_MS);
        fm.anc_arm = 0;
    } else if (fm.anc_arm && plat_ms() - fm.anc_at >= HOLD_MS) {
        fm.anc_arm = 0;
        anchor_back_to(0);
    }
}

#if PLAT_SMK37
/* the SMK-37's wheels (card #144). The mod wheel the selected track's (Styves, 2026-11-09, admin #607): on the LEAD its
 * vibrato (VIBRATO's depth, if more); on the others their filter, ALGORITHM's (track_algo: the drone's TIMBRE, GLINT's
 * BRIGHT, the bass's FILTER) set where the wheel is, as a fader sets its level, its large view as ALGORITHM turned shows
 * it. It acts only when it moves: a track selected takes nothing until then, the one left keeps what it had (the LEAD
 * its vibrato); after a track change or a second at rest, two steps wake it (a reading's flicker is no move); the first
 * second and a half only reads where it rests, as the faders. The pitch wheel the LEAD's, two semitones either way,
 * whatever the track; told when it moves. MIDI's CC 1 as ever */
static void smk_wheels(void)
{
    static int16_t was = -1;
    static uint8_t awake, trk;
    static uint32_t moved_at;
    static int mod, bend = 0x7FFF, sent = -1;
    int m = plat_wheel(PLAT_WHEEL_MOD), b = plat_wheel(PLAT_WHEEL_PITCH), d = m - was;
    if (b > -96 && b < 96)               /* (its rest: dead still) */
        b = 0;
    if (trk != st.v[P_KB] || plat_ms() - moved_at > 1000u) {
        trk = (uint8_t)st.v[P_KB];
        awake = 0;
    }
    if (was < 0 || plat_ms() < 1500u) {
        was = (int16_t)m;
    } else if (d && (awake || d > 1 || d < -1)) {
        was = (int16_t)m;
        awake = 1;
        moved_at = plat_ms();
#if FRS_LEAD
        if (st.lead) {
            mod = m;
        } else
#endif
        {
            uint32_t p = track_algo();
            param_set((int)p, ((int32_t)m * PD[p].max + 63) / 127);
            fm.touched = (prm_t)p;
            fm.touched_enc = EN_ALGO;
            fm.touched_at = plat_ms();
        }
    }
#if FRS_LEAD
    if (mod == sent && b == bend)
        return;
    sent = mod;
    bend = b;
    lv_wheels((uint32_t)mod, b);         /* (Perform LIVE's: card #152) */
    eng_lead_wheels((uint32_t)mod, b);
#else
    (void)mod;
    (void)sent;
    (void)bend;
#endif
}
#endif

#if PLAT_SMK37
/* the SMK-37's faders (Styves, 2026-11-09): 1, 2, 3, 4 MIXER's DRONE, BASS, GLINT and LEAD (K1 .. K4); a fader acts only
 * when it moves, so a scene, a knob or LIVE keep their level until then; the first second and a half after the start
 * only reads where they rest (no jump while the analog inputs settle) */
static void smk_faders(void)
{
    static const prm_t FP[4] = {P_MX_DRONE, P_MX_BASS, P_MX_GLINT, P_MX_LEAD};
    static int16_t was[4] = {-1, -1, -1, -1};
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int v = plat_fader((int)i);
        if (was[i] < 0 || plat_ms() < 1500u) {
            was[i] = (int16_t)v;
            continue;
        }
        if (v == was[i])
            continue;
        was[i] = (int16_t)v;
        param_set(FP[i], ((int32_t)v * PD[FP[i]].max + 63) / 127);
    }
}

/* MASTER on K8, the encoder no role has (Styves, 2026-11-09): a detent a step (base's K5 step, 96 of 4096), from where
 * it is; never saved (each start at base's half gain, 2896: a write at each turn, or after it, held the audio up); its
 * level a moment on the screen, 0 .. 100. The engine's way to it eased by base (smk_level), as ever */
static void k8_master(void)              /* (smk_master is base's, in the device's unity build) */
{
    static int32_t lv = -1;
    int32_t d = plat_enc_free();
    char t[8];
    if (!d)
        return;
    if (lv < 0)
        lv = (int32_t)plat_master();
    lv = clamp(lv + d * 96, 0, 4096);
    plat_master_set((uint32_t)lv);
    t[0] = 0;
    int_text((lv * 100 + 2048) / 4096, t);
    app_say("MASTER", t);
}

/* the SMK-37's pads 1 and 2 (base's 0 and 1, the top row's first) answer every choice (Styves, 2026-11-09): 1 red, CANCEL
 * (FX's), 2 green, OK (SEL's); lit while a choice waits, the answers it has alone (a warning: OK), dark otherwise, as
 * the FM-1 lights only its answering buttons; a hit, whatever its velocity, its button's tap (down this pass, up the
 * next, into what the buttons say: pop-ups, names, the grid, SAVE's flow as they read SEL and FX); the pads nothing
 * else meanwhile. SEL and FX themselves as ever. The screens say CANCEL and OK (ui.c pad_hint) */
#define PAD_CANCEL 0
#define PAD_OK 1
#define PAD_RED 0x800000u                 /* (half: full was too bright on stage, Styves) */
#define PAD_GREEN 0x008000u
#define PAD_BLUE 0x001080u               /* LOAD's (Styves, 2026-11-09): blue */
#define PAD_EMERALD 0x008050u            /* SAVE's: emerald, a green with blue in it (OK's green apart) */
/* the choice waiting, as the pads answer it (ui.c pad_hint_as says it): PA_OK a warning's OK alone; PA_OKCANCEL every
 * question's CANCEL and OK; SAVE's flow, its own words (Styves): its first page LOAD (pad 1, blue) and SAVE (pad 2,
 * emerald), K1 nothing; SCENES (its LOAD) CANCEL and LOAD; WHAT and INTO, the name, CANCEL and SAVE */
enum { PA_NONE, PA_OK, PA_OKCANCEL, PA_LOADSAVE, PA_LOAD, PA_SAVE };
static uint32_t pad_mode(void)
{
    if (pop.on)                          /* (a warning, a done: OK alone) */
        return PA_OK;
    if (nm.on)
        return nm.flow ? PA_SAVE : PA_OKCANCEL;
    if (grid_asks())
        return PA_OKCANCEL;
    return fm.page == PG_SAVE ? PA_LOADSAVE : fm.page == PG_SAVEAS ? PA_SAVE : fm.page == PG_SCENES ? PA_LOAD : PA_NONE;
}
static uint32_t pad_answers(void)        /* bit 0 pad 2 answers, bit 1 pad 1 does; 0 none (the LFO pads' rule) */
{
    uint32_t m = pad_mode();
    return m == PA_NONE ? 0u : m == PA_OK ? 1u : 3u;
}
/* the SMK-37's LFO pads (Styves, 2026-11-09): LFO 1, 2, 3 on the bottom row's last three (base's pads 13, 14, 15; 12
 * dark), each its hue, blue, cyan, pink (violet kept for a fourth); its light the LFO's SHAPE as it moves now, a floor
 * of it at the bottom (which is which, still), at the top half the LED's full as pads 1 and 2; dark while the LFO is off
 * (DEPTH 0 or no TARGET: FRAME's bit, lfo[].on). Some 30 times a second: each a WS2812 frame between two LCD transfers
 * (base's smk_pads_flush, the main loop's, ~0.5 ms; the render the audio's, apart). A tap, its LFO's first page; held
 * HOLD_MS, as LFO held: a knob turned puts the setting under it onto that LFO (lfo_pad_route), its release no page. The
 * top row's last (base's 7) held, at once, as SEL held: a knob turned, its setting's LFO AMOUNT (lfo_amount; on none,
 * NO LFO); a tap nothing; its light a dim white while an LFO is on, brighter held, dark with none on. While a choice
 * waits, a hit nothing: their light stays */
#define PAD_LFO0 13u
#define PAD_AMT 7u
#define PAD_WHITE 0x202020u              /* (a quarter of 0x80) */
#define PAD_WHITE_HELD 0x404040u
static const uint32_t PAD_HUE[4] = {0x0028FFu, 0x00C8FFu, 0xFF2896u, 0x9600FFu};   /* blue, cyan, pink, (violet) */
static const uint8_t LFO_PAGE[NLFO] = {PG_LFO1, PG_LFO2, PG_LFO3};
static const uint8_t LFO_TPAGE[NLFO] = {PG_LFO1T, PG_LFO2T, PG_LFO3T};   /* (a tap on its own page: its TARGETS, and back) */
static uint32_t pad_hue(uint32_t hue, uint32_t lv)   /* a hue at lv of 32, its 255 at 0x80 */
{
    uint32_t r = (hue >> 16 & 255u) * lv * 128u / 8160u, g = (hue >> 8 & 255u) * lv * 128u / 8160u;
    return r << 16 | g << 8 | (hue & 255u) * lv * 128u / 8160u;
}
static void pad_lfo(uint32_t hit, int choice)
{
    static uint32_t lit_at;
    static struct {
        uint8_t down, used;
        uint32_t at;
    } lp[NLFO];
    uint32_t i, now = plat_ms(), held = plat_pads(), any = 0;
    pad_amt_held = (uint8_t)((held >> PAD_AMT & 1u) && !choice);
    for (i = 0; i < NLFO; i++)
        any |= lfo[i].on;
    plat_pad_rgb((int)PAD_AMT, !any ? 0u : pad_amt_held ? PAD_WHITE_HELD : PAD_WHITE);
    if (now - lit_at >= 33u) {
        lit_at = now;
        for (i = 0; i < NLFO; i++) {     /* (4 .. 32: the floor, then SHAPE's value) */
            float o = lfo[i].out < -1.0f ? -1.0f : lfo[i].out > 1.0f ? 1.0f : lfo[i].out;
            uint32_t lv = 4u + (uint32_t)((o + 1.0f) * 14.0f + 0.5f);
            plat_pad_rgb((int)(PAD_LFO0 + i), lfo[i].on ? pad_hue(PAD_HUE[i], lv) : 0u);
        }
    }
    for (i = 0; i < NLFO; i++) {
        uint32_t bit = 1u << (PAD_LFO0 + i);
        if ((hit & bit) && !choice && !lp[i].down) {
            lp[i].down = 1;
            lp[i].used = 0;
            lp[i].at = now;
        }
        if (!lp[i].down)
            continue;
        if (!(held & bit)) {             /* let go: a tap, its page; a hold, its end */
            if (!lp[i].used && now - lp[i].at < HOLD_MS) {   /* (on its page: its TARGETS; elsewhere, its TARGETS
                                                              * too: its page) */
                fm.logo = 0;
                fm.help = 0;
                fm.page = fm.page == LFO_PAGE[i] ? LFO_TPAGE[i] : LFO_PAGE[i];
            }
            if (lfo_pad_held == i + 1u)
                lfo_pad_held = 0;
            lp[i].down = 0;
        } else if (!lp[i].used && now - lp[i].at >= HOLD_MS && !lfo_pad_held) {
            lp[i].used = 1;              /* (held: the knobs its, no page at its release) */
            lfo_pad_held = (uint8_t)(i + 1u);
        }
    }
}

/* the SMK-37's track pads (Styves, 2026-11-09): the bottom row's first four (base's 8 .. 11), DRONE, GLINT, BASS, LEAD as
 * KNOB BANK goes; each its track's colour as the screen has it, made for the LEDs (the screen's gamma undone, green a
 * little less: it reads brighter), the selected at half the LED's full as the others pads' tops, the rest a quarter of
 * it; a tap the track, as KNOB BANK or SELECT landing on it (its page followed, the header's colour); the selected's tap
 * its EDIT pages (admin #607), as EDIT tapped (PARA on the SMK-37): from elsewhere its first, on one of them the next,
 * round (LEAD I, II, III, DELAY), at its release; held HOLD_MS, its track's notes let go (admin #634: core.c
 * track_off), "DRONE RELEASED" a moment, no tap at its release; while a choice waits, nothing (their light stays) */
#define PAD_TRK0 8u
#define PAD_RGB(r, g, b) ((uint32_t)(r) << 16 | (uint32_t)(g) << 8 | (uint32_t)(b))   /* (not a hex word: base's check
                                                                                        * reads 0x01.... as an address) */
static const uint32_t PAD_TRK[3 + FRS_LEAD] = {PAD_RGB(112, 90, 128), PAD_RGB(1, 20, 128), PAD_RGB(128, 42, 15),
                                               PAD_RGB(128, 16, 30)};   /* fog, azure, lamp, rose */
static void pad_tracks(uint32_t hit, int choice)
{
    static const char *const NAME[4] = {"DRONE", "GLINT", "BASS", "LEAD"};
    static uint32_t at[4];
    static uint8_t down, used;           /* (a bit a pad: down since a hit; its hold done) */
    uint32_t t, cur = st.v[P_KB], held = plat_pads(), now = plat_ms();
    for (t = 0; t < 3u + FRS_LEAD; t++)
        plat_pad_rgb((int)(PAD_TRK0 + t), t == cur ? PAD_TRK[t] : PAD_TRK[t] >> 2 & 0x3F3F3Fu);
    for (t = 0; t < 3u + FRS_LEAD; t++) {
        uint32_t bit = 1u << (PAD_TRK0 + t);
        if ((hit & bit) && !choice) {
            down |= (uint8_t)(1u << t);
            used &= (uint8_t)~(1u << t);
            at[t] = now;
        }
        if (!(down >> t & 1u))
            continue;
        if (!(held & bit)) {             /* let go: before its hold, the tap (the track, or its EDIT pages) */
            down &= (uint8_t)~(1u << t);
            if (!(used >> t & 1u) && !choice) {
                if (t == cur)
                    tap(B_EDIT);
                else
                    track_turn((int32_t)t - (int32_t)cur);
            }
        } else if (!(used >> t & 1u) && now - at[t] >= HOLD_MS && !choice) {   /* held: its notes let go (admin
                                                                                * #634), no tap at its release */
            used |= (uint8_t)(1u << t);
            track_off(t);
            app_say(NAME[t], "RELEASED");
        }
    }
}

static uint32_t pad_taps(void)           /* each pass, the buttons read: the pads' as SEL's and FX's (SAVE's flow: its own) */
{
    static const uint32_t P1[6] = {0u, 0u, PAD_RED, PAD_BLUE, PAD_RED, PAD_RED};   /* pad 1's light, PA_*'s */
    static const uint32_t P2[6] = {0u, PAD_GREEN, PAD_GREEN, PAD_EMERALD, PAD_BLUE, PAD_EMERALD};
    static uint8_t up;
    uint32_t m = pad_mode(), hit = plat_pads_pressed(), b = 0;
    int one = (hit >> PAD_CANCEL & 1u) != 0u, two = (hit >> PAD_OK & 1u) != 0u;
    plat_pad_rgb(PAD_CANCEL, P1[m]);
    plat_pad_rgb(PAD_OK, P2[m]);
    pad_lfo(hit, m != PA_NONE);
    pad_tracks(hit, m != PA_NONE);
    if (up) {                            /* (the tap's second pass: let go) */
        up = 0;
        return 0u;
    }
    if (m == PA_LOADSAVE && (one || two)) {   /* SAVE's first page: LOAD (SCENES, as SEL on LOAD), SAVE (WHAT, INTO) */
        fm.logo = fm.help = 0;
        sv.load = (uint8_t)one;
        if (one) {
            fm.page = PG_SCENES;
            fm.pick = fm.scene;
        } else {
            flow_saveas();
        }
        return 0u;
    }
    if (m == PA_LOAD && (one || two)) {  /* SCENES: LOAD, the scene K1 points at (as PLAY does there); CANCEL, back */
        if (two)
            play_tap();
        else
            fm.page = PG_SAVE;
        return 0u;
    }
    if (m != PA_NONE && two)
        b = 1u << B_SEL;
    else if (m != PA_NONE && m != PA_OK && one)
        b = 1u << B_FX;
    up = b != 0u;
    return b;
}
#endif

static void buttons(uint32_t btn)
{
    uint32_t down = btn & ~btn_prev, up = btn_prev & ~btn, k, now = plat_ms();
    anchor_keys(btn);
    if (grid_asks()) {                   /* SEL takes, FX leaves, the rest wait; OCT+, OCT- its pages */
        for (k = 0; k < NB; k++)
            if ((down >> k & 1u) && k != B_OCTDN && k != B_OCTUP)
                b_used(k);
        if (down >> B_SEL & 1u) {        /* (what is heard kept; a choice not yet heard taken now) */
            grid_close();
        } else if (down >> B_FX & 1u) {
            ng.due = 0;
            grid_close();
            grid_back();
        }
        down &= 1u << B_OCTDN | 1u << B_OCTUP;
    }
    if (ng.on && (down & ~(1u << B_OCTDN | 1u << B_OCTUP)))   /* any other button: the grid goes, the button does its job */
        grid_close();
    for (k = 0; k < NB; k++)
        if ((up >> k & 1u) && !(fm.hold_done >> k & 1u) &&
            !(k == B_PLAY && now - fm.hold_at[B_PLAY] >= PLAY_SHOW_MS))   /* (PLAY let go while counting: nothing) */
            tap(k);
    if ((up >> B_OCTDN & 1u) && ng.on && ng.pick) {   /* OCT- let go, a preset picked under it: taken now */
        ng.due = 1;
        grid_close();
    }
    for (k = 0; k < NB; k++) {
        if (!(down >> k & 1u))
            continue;
        if (k == B_OCTDN)
            pv.oct = fm.octave;          /* (to undo, should it be a hand) */
        if (ng.on && (k == B_OCTDN || k == B_OCTUP)) {   /* the grid's pages, the octave as it is */
            grid_page(k == B_OCTUP ? 1 : -1);
            spk_turned |= 1u << EN_K1;   /* (the screen reader says the row it went to, as a turn's) */
        } else if (k == B_OCTDN || k == B_OCTUP) {   /* at once; both: ANCHOR (the first one's step undone) */
            uint32_t other = k == B_OCTDN ? B_OCTUP : B_OCTDN;
            if (btn >> other & 1u) {
                if (!fm.anc_both) {
                    if (btn_prev >> other & 1u)   /* (it came first: its step undone; both in one pass, none taken) */
                        fm.octave = fm.oct_was;
                    fm.oct_at = 0;
                    b_used(B_OCTDN);
                    b_used(B_OCTUP);
                    anchor_press();
                }
            } else {
                fm.oct_was = fm.octave;
                fm.octave = (int8_t)clamp(fm.octave + (k == B_OCTUP ? 1 : -1), -3, 2);
                fm.oct_at = plat_ms() | 1u;   /* (large a moment, ui.c; undone as a hand: not shown) */
                fm.oct_v = fm.octave;
            }
        } else if (k == B_REC && (btn >> B_SAVE & 1u)) {   /* SAVE held, REC: STORE (SCENES' slot, else the scene's) */
            b_used(B_SAVE);
            b_used(B_REC);
            scene_store();
        } else if (k == B_REC && (btn >> B_OCTDN & 1u) && !(btn >> B_OCTUP & 1u)) {   /* OCT- held, REC: on a SEQ page, the
                                                                                       * track's sequence cleared (elsewhere
                                                                                       * nothing); OCT- a hand, its step
                                                                                       * undone and not shown; REC's cycle
                                                                                       * and its hold as they were */
            fm.octave = pv.oct;
            fm.oct_at = 0;
            b_used(B_OCTDN);
            b_used(B_REC);
            if (PAGES[fm.page].button == B_SEQ)
                track_clear();
        } else if (k == B_PLAY && (btn >> B_OCTDN & 1u) && !(btn >> B_OCTUP & 1u)) {   /* OCT- held, PLAY: CUT,
                                                                                       * every sound gone now, the
                                                                                       * cloud's tail and the echoes
                                                                                       * too (RELEASE ALL, then the
                                                                                       * engine emptied: frasil_cut);
                                                                                       * OCT- a hand, its step undone;
                                                                                       * PLAY's tap and hold nothing */
            fm.octave = pv.oct;
            b_used(B_OCTDN);
            b_used(B_PLAY);
            eng_cut();
            lv_op(FST_CUT, 0, 0);
            app_say("CUT", "ALL SOUND");
        } else if (k == B_EDIT && (btn >> B_OCTDN & 1u) && !(btn >> B_OCTUP & 1u)) {   /* OCT- held, EDIT: COMPARE,
                                                                                       * the scene as saved, while EDIT
                                                                                       * stays down (OCT- a hand, its
                                                                                       * step undone) */
            fm.octave = pv.oct;
            b_used(B_OCTDN);
            b_used(B_EDIT);
            fm.hold_acted |= 1u << B_EDIT;   /* (as EDIT's hold was: SELECT turned with it undoes it, hold_undo) */
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
        if (!b_held(b) || (fm.hold_done >> b & 1u) || now - fm.hold_at[b] < (b == B_PLAY ? PLAY_HOLD_MS : b == B_REC ? REC_HOLD_MS : HOLD_MS))
            continue;
        switch (b) {
        case B_HOME:                     /* the help, on its page with this page's button, until HOME again */
            b_used(b);
            fm.help = 1;
            fm.help_page = (uint8_t)help_for(PAGES[fm.page].button);
            break;
        case B_ARP: {                    /* the latch of the selected track's arpeggio (nothing on DRONE); on, its PLAYS
                                          * on ARP too (off, PLAYS stays) */
            arp_t *a = st.bass ? &barp : &arp;
            int bs = a == &barp, tp = bs ? P_BQ_ON : P_SQ_TYPE;
            b_used(b);
            if (!st.bass && !st.glint)
                continue;                /* (nothing done: nothing for SELECT to undo, hold_acted stays off) */
            a->latch ^= 1u;
            lv_set(bs ? P_BA_LATCH : P_GA_LATCH, a->latch);   /* (as ARP II's LATCH sets it) */
            arp_was.a = a;
            arp_was.typed = 0;
            if (!a->latch) {             /* off: only the keys still down go on (the rest kept for hold_undo) */
                memcpy(arp_was.held, a->held, sizeof arp_was.held);
                memcpy(arp_was.prs, a->prs, sizeof arp_was.prs);
                arp_was.n = a->n;
                arp_keep_held(a);
            } else if (st.v[tp] != SQT_ARP) {   /* (what it was, for hold_undo) */
                arp_was.typed = 1;
                arp_was.type = st.v[tp];
                arp_was.run = bs ? bq.run : sq.run;
                arp_was.rec = st.v[bs ? P_BQ_REC : P_SQ_REC];
                arp_was.page = fm.page;
                param_set(tp, (int32_t)SQT_ARP);
            }
            app_say(bs ? "BASS ARP LATCH" : "ARP LATCH", !a->latch ? "OFF" : arp_was.typed ? "ON, ARP ON" : "ON");
            break;
        }
        case B_SEQ: {                    /* the selected track's STACK, from any page (the SEQ pages show it on) */
            uint32_t p = P_KY_DR_STACK + 4u * st.v[P_KB];
            b_used(b);
#if FRS_LEAD
            if (st.lead)                 /* (LEAD: no KEYS of its own) */
                continue;
#endif
            param_set((int)p, !st.v[p]);
            prefs_later();
            app_say("STACK", st.v[p] ? "ON" : "OFF");
            continue;                    /* (nothing for SELECT to undo) */
        }
#if PLAT_SMK37
        case B_FBANK: {                  /* the SMK-37's FADER BANK held: the selected track's KEYS LATCH (off: its notes
                                          * let go, as KEYS' K3 does) (Styves, 2026-11-09) */
            uint32_t p = P_KY_DR_LATCH + 4u * st.v[P_KB];
            b_used(b);
#if FRS_LEAD
            if (st.lead)                 /* (LEAD: nothing latches) */
                continue;
#endif
            param_set((int)p, !st.v[p]);
            prefs_later();
            app_say("LATCH", st.v[p] ? "ON" : "OFF");
            continue;                    /* (nothing for SELECT to undo) */
        }
#endif
        case B_FX:                       /* FREEZE, the page staying */
            b_used(b);
            param_set(P_FREEZE, !st.v[P_FREEZE]);
            app_say("FREEZE", st.v[P_FREEZE] ? "ON" : "OFF");
            break;
        case B_REC:                      /* the metronome: OFF, and back to its last on value */
            b_used(b);
            if (st.v[P_METRO])
                fm.metro_on = st.v[P_METRO];
            param_set(P_METRO, st.v[P_METRO] ? 0 : fm.metro_on);
            prefs_later();               /* (while anything sounds, once it no longer does: the flash written stops the
                                          * render a moment, a click) */
            app_say("METRONOME", !st.v[P_METRO] ? "OFF" : st.v[P_METRO] == 1u ? "REC" : "ALWAYS");
            break;
        case B_EDIT:                     /* its three keys, while it stays down (no COMPARE: OCT- first) */
            b_used(b);
            if (!st.compare)
                fm.edit_map = 1;
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
        if (fm.hold_done >> b & 1u)
            fm.hold_acted |= 1u << b;
    }
    if (st.compare && !b_held(B_EDIT)) {
        st.compare = 0;
        st.engine_dirty = 1;
    }
}

static int swell_held(uint32_t btn)     /* ENV held, as the engine hears it (eng_swell; STATE's running state too) */
{
    return (btn >> B_ENV & 1u) && plat_ms() - fm.hold_at[B_ENV] >= HOLD_MS &&
           (fm.hold_acted >> B_ENV & 1u);   /* (ENV's hold: none once SELECT took it) */
}

static void swell_update(uint32_t btn)
{
    int held = swell_held(btn);
    eng_swell(held);
    lv_watch(held);                      /* (ENV held, COMPARE: told as they change) */
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

/* EDIT held and a key: the track's group g, its first page, the same key again its next, round */
static const uint8_t EDIT_GROUPS[3 + FRS_LEAD][3][3] = {
    {{PG_CHOEUR, PG_CHOEUR2, PG_CHOEUR3}, {PG_PARTIELS, PG_PARTIELS2, PG_PARTIELS2}, {PG_SOUFFLE, PG_SOUFFLE2, PG_SOUFFLE2}},
    {{PG_GLINT, PG_GLINT, PG_GLINT}, {PG_GLINT2, PG_GLINT2, PG_GLINT2}, {PG_GDELAY, PG_GDELAY, PG_GDELAY}},
    {{PG_BASS, PG_BASS, PG_BASS}, {PG_BASS2, PG_BASS2, PG_BASS2}, {PG_BDRIVE, PG_BDRIVE, PG_BDRIVE}},
#if FRS_LEAD
    {{PG_LEAD, PG_LEAD, PG_LEAD}, {PG_LEAD2, PG_LEAD3, PG_LEAD3}, {PG_LDELAY, PG_LDELAY, PG_LDELAY}},
#endif
};
static int32_t edit_group(void)          /* the group of the page on the screen, -1 none */
{
    const uint8_t (*g)[3] = EDIT_GROUPS[st.v[P_KB] % (3u + FRS_LEAD)];
    uint32_t i, j;
    for (i = 0; i < 3u; i++)
        for (j = 0; j < 3u; j++)
            if (g[i][j] == fm.page)
                return (int32_t)i;
    return -1;
}
static void edit_jump(uint32_t i)
{
    const uint8_t *g = EDIT_GROUPS[st.v[P_KB] % (3u + FRS_LEAD)][i];
    uint32_t j, n = g[1] == g[0] ? 1u : g[2] == g[1] ? 2u : 3u;
    fm.logo = 0;
    fm.help = 0;
    for (j = 0; j < n; j++)
        if (g[j] == fm.page) {
            fm.page = g[(j + 1u) % n];
            return;
        }
    fm.page = g[0];
}

static void key_down(uint32_t k, uint32_t vel)
{
    if (fm.logo_until)                   /* playing ends the power-on logo */
        fm.logo = 0;
    if (b_held(B_SAVE)) {                /* SAVE held: the white keys are the scenes, nothing sounds */
        int32_t w = white_of(k);
        b_used(B_SAVE);
        key_begin(k);
        key[k].kind = K_NONE;
        if (w >= 0 && w < (int32_t)NSCENES)
            scene_go((uint32_t)w);
        return;
    }
    if (b_held(B_EDIT) && !st.compare) { /* EDIT held: the three leftmost black keys, the track's three groups of pages;
                                          * nothing sounds, the rest do nothing */
        b_used(B_EDIT);
        fm.edit_map = 1;
        key_begin(k);
        key[k].kind = K_NONE;
        if (k == 1u || k == 3u || k == 5u)
            edit_jump((k - 1u) / 2u);
        return;
    }
    if (b_held(B_SEL))                   /* (SEL with a key: no HARMONY on its release, whatever the keys play) */
        b_used(B_SEL);
    eng_key_down(k, 53 + (int)k + 12 * fm.octave, vel, b_held(B_SEL));   /* (SEL held: a single note) */
    lv_key(k, 53 + (int)k + 12 * fm.octave, vel, b_held(B_SEL));
}

#if PLAT_SMK37
/* the SMK-37's ten keys around the FM-1's 27 (C3..E3 below F3, G#5..C6 above G5): notes only, never a page's keys
 * (Styves, 2026-11-09: they played nothing), on the engine's key numbers 27..31 (FRS_NKEYS 32, LIVE's): five of them
 * down at once at most, a sixth silent; SAVE or EDIT held: nothing */
static void keys_outer(void)
{
    static uint64_t prev;
    static int8_t id_of[NKEYS37];
    static uint8_t ready;
    const uint64_t outer = 0x1Full | 0x1Full << 32;   /* keys 0..4 and 32..36 */
    uint64_t now = plat_keys37() & outer, chg = now ^ prev;
    uint32_t k;
    if (!ready) {
        for (k = 0; k < NKEYS37; k++)
            id_of[k] = -1;
        ready = 1;
    }
    for (k = 0; chg && k < NKEYS37; k++) {
        uint64_t bit = 1ull << k;
        if (!(chg & bit))
            continue;
        chg &= ~bit;
        if (now & bit) {
            uint32_t id, used = 0, j, vel;
            int note;
            if (b_held(B_SAVE) || b_held(B_EDIT))
                continue;
            for (j = 0; j < NKEYS37; j++)
                if (id_of[j] >= 0)
                    used |= 1u << (uint32_t)id_of[j];
            for (id = NKEYS; id < FRS_NKEYS && (used >> id & 1u); id++)
                ;
            if (id >= FRS_NKEYS)
                continue;                /* (five down already) */
            id_of[k] = (int8_t)id;
            if (fm.logo_until)
                fm.logo = 0;
            if (b_held(B_SEL))
                b_used(B_SEL);
            note = 48 + (int)k + 12 * fm.octave;   /* (key 0 is C3, 48; key 5 F3, 53: the FM-1's first) */
            vel = (uint32_t)plat_key_vel((int)k);
            eng_key_down(id, note, vel, b_held(B_SEL));
            lv_key(id, note, vel, b_held(B_SEL));
        } else if (id_of[k] >= 0) {
            key_up((uint32_t)id_of[k]);
            lv_key_up((uint32_t)id_of[k]);
            id_of[k] = -1;
        }
    }
    prev = now;
}
#endif

static void keys_scan(void)
{
    uint32_t keys, k;
    touch_poll();
    keys = welcome_keys(saver_keys(touch_keys()));   /* (a key that woke the screen, or closed Learn's welcome: no
                                                      * note) */
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(keys_prev & bit)) {
#if PLAT_SMK37
            key_down(k, (uint8_t)plat_key_vel((int)k + 5));   /* (the SMK-37: its two contacts' velocity, the middle 27
                                                              * keys from its sixth, F3; no pressure on its keys) */
#else
            key_down(k, touch_vel(k));
#endif
        } else if (!(keys & bit) && (keys_prev & bit)) {
            key_up(k);
            lv_key_up(k);
        } else if (keys & bit) {
            uint8_t p = touch_pressure(k);
            lv_pressure(k, p);           /* (before: it reads what the key had) */
            key_pressure(k, p);
        }
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
        int played = 53 + (int)k + 12 * fm.octave, snapped = scale_snap(played, st.v);
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
            if (w == (int32_t)fm.scene)
                *bright |= 1u << k;
            else if ((scn.used >> w & 1u) || w < (int32_t)NFACTORY)
                *dim |= 1u << k;
        }
        return;
    }
    if (st.glint && st.v[P_SQ_TYPE] == SQT_SEQ) {   /* the sequencer: the note sounding bright, the scale's dim */
        int32_t kk;
        for (k = 0; k < NKEYS; k++)
            if (bits != 0xFFFu && in_scale(53 + (int)k + 12 * fm.octave, st.v[P_ROOT] % 12, bits))
                *dim |= 1u << k;
        if (sq.sounding >= 0 && (kk = note_key(sq.sounding, 0)) >= 0)
            *bright |= 1u << kk;
        *bright |= keys_prev;
        return;
    }
    if ((st.glint && st.v[P_SQ_TYPE] == SQT_ARP) || (st.bass && st.v[P_BQ_ON] == SQT_ARP)) {   /* the layer's arpeggio: its
                                                     * notes dim, the one playing bright */
        const arp_t *a = st.bass ? &barp : &arp;
        int32_t kk, sh = 0;              /* (its notes are kept without OCTAVE) */
        for (k = 0; k < a->pn; k++)
            if ((kk = note_key(a->play[k], sh)) >= 0)
                *dim |= 1u << kk;
        if (a->sounding >= 0 && (kk = note_key(a->src, sh)) >= 0)
            *bright |= 1u << kk;
        *bright |= keys_prev;
        return;
    }
    if (st.bass) {                       /* BASS: its note bright, the scale's dim */
        int32_t kk;
        for (k = 0; k < NKEYS; k++)
            if (bits != 0xFFFu && in_scale(53 + (int)k + 12 * fm.octave, st.v[P_ROOT] % 12, bits))
                *dim |= 1u << k;
        if (bass_note >= 0 && (kk = note_key(bass_note, 0)) >= 0)
            *bright |= 1u << kk;
        *bright |= keys_prev;
        return;
    }
    for (k = 0; k < NKEYS; k++) {
        int n = 53 + (int)k + 12 * fm.octave;
        if (bit_get(lit, n))
            *bright |= 1u << k;
        if (bits != 0xFFFu && in_scale(n, st.v[P_ROOT] % 12, bits))
            *dim |= 1u << k;
    }
    *bright |= keys_prev;
}

#ifdef PLAT_HOST
static void enc_late_in(void);          /* (the tests' detents, come in during a pass: app_host_cmd) */
static uint8_t test_loop_uneven;
#endif

static void input(void)
{
    uint32_t btn = saver_buttons(plat_buttons()), lb, mb, gb, kb, kd;   /* (a button that woke the screen: nothing) */
#if PLAT_SMK37
    btn |= pad_taps();                   /* (the pads answer a choice as SEL and FX do) */
#endif
    btn = welcome_buttons(btn);          /* (one that closed Learn's welcome: nothing, till let go) */
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
#if PLAT_SMK37
    keys_outer();
    smk_wheels();
#endif
    eng_steps();                         /* (the engine's millisecond, around the MIDI) */
    midi_in();
    eng_moves();
    grid_tick();
    if (rec_any() && !test_rec_anywhere && PAGES[fm.page].button != B_SEQ && PAGES[fm.page].button != B_ARP) {   /* a take lives on the SEQ and ARP
                                                                                            * pages: leaving them ends it */
        lv_record_off();
        app_say("RECORD OFF", "");
    }
    {                                    /* SCENES left: its LOAD kept, if it changed (MORPH: CUT at every start) */
        static uint8_t was;
        if (was == PG_SCENES && fm.page != PG_SCENES &&
            (set.load != st.v[P_LOAD] || set.magic != SET_MAGIC))
            prefs_later();
        was = fm.page;
    }
    if (fm.prefs_due && !fm.store_hold && (fm.prefs_due == 2 || prefs_quiet()))   /* (kept: when nothing sounds) */
        prefs_save();
    if (frasil_pump_gain() < pump_low)
        pump_low = frasil_pump_gain();
    if (!pop.on && !nm.on)               /* (up, the encoders are theirs alone: a detent come in since they read them
                                          * waits for their next read, never the page under them: SCENES' LOAD) */
        encoders();
    buttons(btn);
#if PLAT_SMK37
    smk_faders();
    k8_master();
#endif
    holds();
    swell_update(btn);
    if (st.engine_dirty)
        engine_push();
    lv_pump();                           /* (what this pass gave the engine, to a LIVE page) */
    if (pop.on) {                        /* a pop-up: every light out but SEL's, its one way out (back after) */
        oct_soft = 0;
        lights_set(1u << B_SEL, 0, 0, 0, 0, 0);
        return;
    }
    if (grid_asks()) {                   /* an LFO's TARGET: its answers alone */
        oct_soft = 0;
        lights_set(1u << B_SEL | 1u << B_FX, 0, 0, 0, 0, 0);
        return;
    }
    if (nm.on) {                         /* a name: its answers lit, SAVE (store) and FX (cancel); OCT at the mid level */
        oct_soft = 0;
        lights_set(1u << B_SAVE | 1u << B_FX, 1u << B_OCTDN | 1u << B_OCTUP, 0, 0, 0, 0);
        return;
    }
    if (fm.page == PG_SAVE || fm.page == PG_SAVEAS) {   /* the flow: its answers lit, the keys as they are */
        oct_soft = 0;
        keys_lights(&kb, &kd);
        lights_set(1u << B_SEL | 1u << B_FX, fm.page == PG_SAVE ? 1u << B_SAVE : 0u, 0, kb, kd, 0);
        return;
    }
    /* the buttons' lights, four levels (lights_set): the function buttons at the mid level, their names readable in the
     * dark; lit, what is chosen or on: the page's button, FX while the cloud is held, SEQ swelling. PLAY in the glow,
     * lit while anything sounds. REC in the glow (brighter, it would read as recording: at power-on, say); while a
     * sequencer records on STEP, lit; LIVE, flashing on the sequence's beats (as the metronome counts them), twice as
     * quick as OCT. OCT- and OCT+ in the glow, a little
     * brighter on the beat (MIDI clock, else TEMPO; app_tick). SAVE lit while the sound differs from the scene's. LFO
     * in the glow while no LFO runs. ARP, off its pages, a flash on each step of the selected track's arpeggio, in the
     * glow on DRONE; SEQ, off its pages, on each of its sequence's */
    lb = 1u << PAGES[fm.page].button;
    mb = ((1u << B_REC) - 1u) & ~(1u << B_PLAY);
    gb = 1u << B_PLAY | 1u << B_OCTDN | 1u << B_OCTUP;
    if (st.dirty)
        lb |= 1u << B_SAVE;
    if (!lfo_any) {
        mb &= ~(1u << B_LFO);
        gb |= 1u << B_LFO;
    }
    {
        static uint32_t steps_seen, flash_until, was_kb;
        uint32_t n = st.bass ? barp.steps : st.glint ? arp.steps : 0u;
        if (was_kb != st.v[P_KB]) {      /* (another track's count: no step) */
            was_kb = st.v[P_KB];
            steps_seen = n;
        }
        if (n != steps_seen) {
            steps_seen = n;
            flash_until = plat_ms() + 60u;
            flashes[0]++;
        }
        if (PAGES[fm.page].button != B_ARP && (int32_t)(flash_until - plat_ms()) > 0)
            lb |= 1u << B_ARP;
    }
    {
        static uint32_t steps_seen, flash_until, was_kb;
        uint32_t n = st.bass ? bq.steps : st.glint ? sq.steps : dq.steps;
        if (was_kb != st.v[P_KB]) {
            was_kb = st.v[P_KB];
            steps_seen = n;
        }
        if (n != steps_seen) {
            steps_seen = n;
            flash_until = plat_ms() + 60u;
            flashes[1]++;
        }
        if (PAGES[fm.page].button != B_SEQ && (int32_t)(flash_until - plat_ms()) > 0)
            lb |= 1u << B_SEQ;
    }
    if (!st.bass && !st.glint) {
        mb &= ~(1u << B_ARP);
        gb |= 1u << B_ARP;
    }
    if (!rec_any())
        gb |= 1u << B_REC;
    else if (cnt.run) {                  /* COUNT-IN: the first quarter of each of its four beats */
        if (cnt.t - (float)(uint32_t)cnt.t < 0.25f)
            lb |= 1u << B_REC;
    } else if (st.v[P_SQ_REC] != 2u && st.v[P_BQ_REC] != 2u && st.v[P_DQ_REC] != 2u)   /* STEP: lit, steady */
        lb |= 1u << B_REC;
    else if ((uint32_t)(mtr.at >= 0.0f ? mtr.at : grid.tick) % 12u < 3u)   /* LIVE: the first quarter of each
                                                                          * half beat, the sequence's (stopped: the
                                                                          * grid's) */
        lb |= 1u << B_REC;
    if (st.v[P_FREEZE])
        lb |= 1u << B_FX;
    oct_soft = (uint32_t)grid.tick % 24u < 6u;   /* the first quarter of each beat */
    if (st.swell_btn > 0.0f)
        lb |= 1u << B_ENV;
    if (play_lit())
        lb |= 1u << B_PLAY;
    keys_lights(&kb, &kd);
    if (!b_held(B_EDIT))
        fm.edit_map = 0;
    if (fm.edit_map) {                   /* EDIT held: its three keys at the mid level, the group on the screen lit, every
                                          * other key off */
        int32_t g = edit_group();
        kb = g >= 0 ? 1u << (1u + 2u * (uint32_t)g) : 0u;
        lights_set(lb, mb & ~lb, gb & ~(lb | mb), kb, 0, (1u << 1 | 1u << 3 | 1u << 5) & ~kb);
        return;
    }
    lights_set(lb, mb & ~lb, gb & ~(lb | mb), kb, kd, 0);
}

void app_input_only(void)
{
#ifdef PLAT_HOST
    if (test_loop_uneven) {              /* (loopuneven 1: a pass of the device's loop is no millisecond: several in one,
                                          * none in the next, as its drawing and its load make them) */
        static const uint8_t PASSES[8] = {3, 0, 1, 7, 0, 0, 2, 5};
        static uint32_t at;
        uint32_t n = PASSES[at++ & 7u];
        if (test_loop_uneven == 2u && !n)   /* (2: at least one a millisecond, as the device's while it does not draw) */
            n = 1;
        while (n--)
            input();
        return;
    }
#endif
    input();
}

/* ------------------------------------------------------------- the screen --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint8_t red;                         /* its second line bold and bright red (say_red) */
    uint8_t amt;                         /* an LFO's AMOUNT being turned (SEL + K: lfo_amount): its slot + 1, its gauge
                                          * under the words; 0 none */
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
    if (text_is(a, "STOP FIRST")) {      /* (the River lets it all go first, CC 120: what is left, a key held down;
                                          * from a terminal, anything) */
        popup(POP_WARN, "RESTORE FROM USB", "STOP FIRST", "SOMETHING IS PLAYING: LET GO",
              "OF THE KEYS, OR HOLD PLAY TO", "RELEASE ALL, THEN TRY AGAIN.");
        return;
    }
    if (text_is(a, "RESTORE FROM USB")) {
        if (text_is(b, "RESTORED"))
            popup(POP_DONE, "RESTORE FROM USB", "RESTORED", "THE SCENES, THE SETTINGS", "AND ITS PRESETS ARE BACK.", "");
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
    say.amt = 0;
    spk_msg(a, b, ANN_NOW);                  /* and to a screen reader, whole (speak.c) */
}

static void say_clear(void) { say.msg_until = 0; }   /* the words gone (a hold undone: hold_undo) */
static void say_red(void) { say.red = 1; }
static void say_both(void) { say.red = 2; }          /* its two words alike, large, a dot between: ARP ON . SEQ OFF */
static void say_amt(uint32_t slot) { say.amt = (uint8_t)(slot + 1u); }   /* its AMOUNT's gauge under it (lfo_amount) */

#include "ui.c"
#include "speak.c"                       /* the screen to a screen reader (fm1-reader) */

/* the buttons down now taken as used: their release no tap (a restore's question answered with SEL or HOME, outside
 * the app's loop: SEL let go would open HARMONY) */
static void buttons_spent(void)
{
    btn_prev = btn_now = plat_buttons();
    fm.hold_done = ~0u;
}

void app_ui_init(void)
{
    buttons_spent();
    saver.on = saver.party = 0;              /* (awake: at power-on, after a restore or an update tried) */
    saver_touch();
    touch_on(1);                             /* velocity and pressure: the keys' readings on */
    if (fm.booting) {                        /* at power-on, the logo a moment, alive, then the river; again (after a
                                              * restore from USB was tried), the screen as it was */
        fm.booting = 0;
        fm.logo = 1;
        fm.logo_until = plat_ms() + 4000u;
    }
    ui_init();
}

/* ------------------------------------------------------- PERFORM over USB --- */
/* Perform (perform.frasilsynth.com): a page on the computer shows the play as it goes, very large (a projector on a
 * stage, a film). The page asks and the FM-1 tells; nothing is built or sent while no page listens (a DAW on the port
 * never sees a byte), and never from the render: the main loop, once a UI frame, from what the screen knows already.
 * SysEx over USB-MIDI, F0 7D 46 50 cmd .. F7 ("FP"; design/perform/README.md):
 *   page -> FM-1  1 HELLO ver, flags   listen (again every ~2 s: a page not heard for 5 s is gone; each one has SCENE
 *                                 and PAGE told again, a frame the page may have missed); flags 1 COMPUTER AUDIO,
 *                                 2 LIVE (the engine's inputs as OPs, below: PERFORM's LIVE)
 *                 2 BYE           stop
 *                 9 STATE?        LIVE's start (below)
 *   FM-1 -> page  1 HELLO ver, NPARAM (2), the version (ASCII)            the answer to each HELLO
 *                 2 SCENE slot (16: NEW SHARD), dirty, name (16 ASCII)     at HELLO, and when one of them changes
 *                 3 VALUES first (2), n, then n values (2 each)            every setting: at HELLO, after a load
 *                 4 VALUE p (2), v (2), enc (EN_*, 127: no knob's)          a setting that moved
 *                 5 PAGE page, octave + 3, keys (0 DRONE, 1 GLINT, 2 BASS, 3 LEAD)  when one of them changes
 *                 6 FRAME 20 a second, what sounds: the drones' 8 (note, level), GLINT's 4 (note, level as heard,
 *                   1/20 s since struck), the bass (note, level), the LFOs on (bits), then each LFO where in its turn
 *                   (0..127), its value ((-1..1) to 0..127) and its turns (3 bytes: RANDOM's and DRIFT's values, the
 *                   page's lfo_rnd as app.c's), then the tracks' meters (drones, bass, glints, the cloud), the
 *                   transport (bits), LOFI TAPE's drag under way (how much slower: 127 is 8 %); then (card #144: an FM-1
 *                   without the LEAD ends there) the LEAD's 6 voices (note, level), its meter; a note 127: none;
 *                   levels 0..127
 *                 7 BUTTON b (B_*), down
 *                 8 KEYS the keys held (bit k: key k), four 7-bit bytes
 *                 9 STATE, 10 OP, 11 OP in parts: PERFORM's LIVE (below)
 *                12 BATTERY raw (2), mV (2), level        after each HELLO's answer: the divider's last reading
 *                   (16383: none), the millivolts smoothed (APP_BATT_RAW_4V2's scale), the level drawn (127: none);
 *                   the calibration's (design/perform/scripts/fm1_listen.py)
 *                13 PRESSURE p        the hand pressing into THE KEYS (card #139): the held key pressed hardest, how
 *                   much (0..127, 0 none held), as the engine has it; after each HELLO's answer, then as it moves (by
 *                   4, or to 0 or from it), 20 a second at most. An FM-1 before it never sends one
 *   both ways    14 START         Learn's known state (card #153: Learn's START, below)
 *   ARP II's LATCH (P_GA_LATCH, P_BA_LATCH) is the arpeggio's latch as it is, ARP held turning it: its VALUE says so
 *   (eng_steps keeps the setting on the latch)
 * Two bytes: 7-bit high, low. Each frame goes whole or not at all, never waiting (plat_sysex_try): one that does not
 * fit now (the computer not reading, a backup or the screen reader on the wire) is tried again at the next UI frame.
 * A backup streaming (app_backup_hold): quiet. */
#ifndef PLAT_WEB
#define PF_VER 1u                        /* (LIVE too: a page that doesn't ask never sees it) */
#define PF_DUMP 48u                      /* settings in one VALUES (a SysEx ring's third) */
#define LV_PART 120u                     /* a LIVE part's packed bytes at most (fifteen sevens: 105 of its own) */
#define LV_LIVE 2048u                    /* STATE's running state at most (live_save: some 1.5 KB a busy scene; more
                                          * would not fit: none told) */
#define LV_LEAD 1024u                    /* its LEAD's part at most (card #152, engine/live.c LIVE_LEAD_BYTES: some 150
                                          * bytes a voice sounding, 600 its six), made on the main loop's stack */
#define LV_LEAD_ROOM 320u                /* STATE's room for it past the first's most (it takes what the first leaves:
                                          * LV_LIVE + this in all; the SMK-37's pool keeps its 8 KiB spare) */
#define LV_STATE (2u + FRS_SCENE_BYTES + 1u + STATE_BYTES + 1u + 2u * FST_MOVES + 1u + 2u + LV_LIVE + 2u + LV_LEAD_ROOM)   /*
                                          * STATE's bytes, at most */
static struct {
    uint32_t heard, frame_at, btn, kd, prs_at;   /* (kd: the keys held, as the page knows them; prs_at: PRESSURE's
                                          * last) */
    uint8_t on, page, oct, keys, scene, dirty, dump;   /* dump: the next VALUES' first setting + 1 (0: none due) */
    uint8_t prs;                         /* PRESSURE as the page knows it (0xFF: to tell) */
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

#ifdef PLAT_HOST
static struct {                          /* the tests' (pfhello, pfop, pfstate ...): LIVE's frames as sent */
    uint8_t b[16384];                    /* each: its length (2), its bytes */
    uint32_t n, rd, jam, fails;          /* jam: the next ones refused, a ring full (pfjam) */
    uint8_t want[LV_STATE];              /* the STATE pfstate asked for, as the tests make it then (pfc_want) */
    uint32_t want_n, want_ms;            /* (want_ms: the millisecond its running state says, plat_ms() + 1 then) */
    int still, still_fd, still_got;      /* pfstill (pfc_still): 1 the run without the asks, 2 the one with them; the
                                          * first's pipe, its renderhash come */
    uint32_t asks;                       /* (the STATEs asked since pfstill) */
    uint64_t still_hash;
    uint32_t watch;                      /* pfwatch: PERFORM's other messages kept beside LIVE's, bit cmd (pfmsg) */
} pfc;
#endif

static int pf_send(const uint8_t *b, uint32_t n)   /* F0 7D 46 50 b.. F7, whole or not at all: 0 = sent */
{
    uint8_t f[5u + 3u + LV_PART];        /* (the longest: a LIVE part, cmd, part, parts, its bytes) */
    uint32_t i;
    _Static_assert(4u + 2u * PF_DUMP <= 3u + LV_PART, "a VALUES fits pf_send's frame");
    if (n + 5u > sizeof f)
        return -1;
    f[0] = 0xF0u, f[1] = 0x7Du, f[2] = 0x46u, f[3] = 0x50u;
    for (i = 0; i < n; i++)
        f[4u + i] = b[i] & 0x7Fu;
    f[4u + n] = 0xF7u;
#ifdef PLAT_HOST
    if (n && ((b[0] >= 9u && b[0] <= 11u) || (b[0] < 32u && (pfc.watch >> b[0] & 1u)))) {   /* (LIVE's, and those
                                          * pfwatch names, for the checks) */
        if (pfc.jam && b[0] >= 9u && b[0] <= 11u) {
            pfc.jam--;
            return -1;
        }
        if (pfc.n + 2u + n + 5u <= sizeof pfc.b) {
            pfc.b[pfc.n] = (uint8_t)(n + 5u);
            pfc.b[pfc.n + 1u] = (uint8_t)((n + 5u) >> 8);
            memcpy(pfc.b + pfc.n + 2u, f, n + 5u);
            pfc.n += 2u + n + 5u;
        }
    }
#endif
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

#ifdef PLAT_HOST
static uint8_t pf_batt_last[6];          /* (the tests': the last BATTERY sent, pfbatt; how many, pfbattn) */
static uint32_t pf_batt_n;
#endif
static void pf_battery(void)             /* 12 BATTERY: the reading (16383 none), the millivolts, the level (127 none) */
{
    uint8_t b[6];
    b[0] = 12u;
    pf_two(b + 1, batt.raw < 0 ? 16383u : (uint32_t)batt.raw);
    pf_two(b + 3, (uint32_t)(batt.mv16 / 16));
    b[5] = batt.pct < 0 ? 127u : (uint8_t)batt.pct;
    if (pf_send(b, 6u))
        return;
#ifdef PLAT_HOST
    memcpy(pf_batt_last, b, sizeof b);
    pf_batt_n++;
#endif
}

static int pf_scene(void)
{
    char t[SC_LONG + 1];                 /* (the name whole: Perform reads it to the message's end, any length) */
    uint8_t b[3u + SC_LONG];
    uint32_t i;
    scene_name(fm.scene, t);
    b[0] = 2u, b[1] = fm.scene, b[2] = st.dirty != 0;
    for (i = 0; i < SC_LONG && t[i]; i++)
        b[3u + i] = (uint8_t)(t[i] < 0x7F ? t[i] : ' ');
    if (pf_send(b, 3u + i))
        return -1;
    pf.scene = fm.scene, pf.dirty = st.dirty != 0;
    return 0;
}

/* the transport, each track playing (bits): 1 the drones (a chord latched or held, from the keys or MIDI), 2 GLINT
 * (its arpeggio going, not waiting; its sequence running), 4 the bass (its note held or latched; its arpeggio, its
 * sequence, as GLINT's), 8 the LEAD (a voice held, card #144) */
static uint8_t pf_playing(void)
{
    uint32_t k, dr = 0;
    int gl = (st.v[P_SQ_TYPE] == SQT_ARP && arp.n && !arp.paused) || (st.v[P_SQ_TYPE] == SQT_SEQ && sq.run);
    int bs = bass_note >= 0 || mb_n || (st.v[P_BQ_ON] == SQT_ARP && barp.n && !barp.paused) ||
             (st.v[P_BQ_ON] == SQT_SEQ && bq.run);
    for (k = 0; k < 4u; k++)
        dr |= lit[k] | mid_on[k] | sus[k];
    dr |= dq.nsnd;                       /* (the drone's sequence's notes) */
#if FRS_LEAD
    {
        lead_view_t lv;
        frasil_lead_view(&lv);
        return (uint8_t)((dr != 0) | (gl != 0) << 1 | (bs != 0) << 2 | (lv.held != 0) << 3);
    }
#else
    return (uint8_t)((dr != 0) | (gl != 0) << 1 | (bs != 0) << 2);
#endif
}

#ifdef PLAT_HOST
static uint8_t pf_frame_last[96];        /* (the tests': the last FRAME sent, its cmd first; pfframeN, pfframelen) */
static uint32_t pf_frame_n;
#endif
static void pf_frame(void)
{
    frasil_view_t v[FR_NV];
    glint_view_t g[GLINT_NV];
    bass_view_t bv;
    uint8_t b[1u + 2u * FR_NV + 3u * GLINT_NV + 3u + 5u * NLFO + 4u + 1u + 1u + FRS_LEAD * (2u * LEAD_NV + 1u)], *p = b;
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
    *p++ = pf_lvl(frasil_tape_drag() * 12.5f);   /* (LOFI TAPE's drag: 0.08 slower, 127) */
#if FRS_LEAD
    {                                    /* the LEAD's voices, its meter (card #144) */
        lead_view_t lv;
        frasil_lead_view(&lv);
        for (i = 0; i < LEAD_NV; i++) {
            *p++ = pf_note(lv.venv[i] > 0.0f ? lv.vnote[i] : -1);
            *p++ = lv.vnote[i] < 0 ? 0u : pf_lvl(lv.venv[i]);
        }
        *p++ = pf_lvl(frasil_lead_meter());
    }
#endif
#ifdef PLAT_HOST
    if (!pf_send(b, (uint32_t)(p - b))) {   /* (the tests': the last one sent, pfframeN) */
        memcpy(pf_frame_last, b, (uint32_t)(p - b));
        pf_frame_n = (uint32_t)(p - b);
    }
#else
    pf_send(b, (uint32_t)(p - b));
#endif
}

/* ------------------------------------------------------------ PERFORM's LIVE --- */
/* A page with LIVE's bit (HELLO's flags, 2) hears what the hands give the engine, as SHARD STREAM's ops, so that its
 * listeners' engines, a fresh one each fed them in order, play what this one plays (design/perform/LIVE.md, 1):
 *   page -> FM-1  9 STATE?        answered once, as the queue reaches it (a STATE asked while one waits: that one)
 *   FM-1 -> page  9 STATE part, parts, packed: joined and unpacked (little-endian), u16 scene_n, the scene as it sounds
 *                   now (STORE's: its sound, its notes, unsaved), u8 state_n, frs_state_save's bytes, u8 moves_n,
 *                   (setting, value) for each FRS_P_MOMENT one, u8 tracks (FRAME's transport), then (card #131) u16
 *                   live_n, the engine's running state at the same instant (engine/live.c's live_save: "FRlv" and its
 *                   head, the brain, the sound's; LV_LIVE at most, 0 none: it did not fit), then (card #152) u16
 *                   lead_n, the LEAD's part of it ("FRld": its settings, its keys, its voices; in the room the first
 *                   leaves, LV_LIVE + LV_LEAD_ROOM in all, 0 none),
 *                   apart so that a listener without the LEAD reads the first as it always did. A page of #114 stops at
 *                   tracks, one of #131 after the running state; 31 parts at most (LV_STATE)
 *                10 OP packed: op, its args (frasil_stream.h's, the gestures it has none for LV_*'s); KEY_DOWN's fourth
 *                   the velocity. The LEAD's (card #152): its keys FST_KEY_LEAD (an engine without the LEAD skips them:
 *                   KEY_DOWN's track 3 would play its bass), its settings as they turn (FST_PARAM; FST_PARAM16 past
 *                   255, its byte too short: P_LD_DFB 256 .. P_LD_FRELEASE 267), a scene's LEAD (its
 *                   own object's) FST_LEAD how 1 after the scene's op, the LEAD as it is FST_LEAD how 0 (a preset of
 *                   the player's, FX back: its PRESET set alone), the SMK-37's wheels FST_LEAD_WHEELS. An op whose file
 *                   has an index into SCNS (FST_SCENE, FST_SCENE_NOW, FST_STORE, FST_ANCHOR) carries the scene itself,
 *                   FRS_SCENE_BYTES (scene_t: the LEAD apart): too long for one frame, it goes as
 *                11 OP part, parts, packed: joined and unpacked, op, the scene
 * "packed": MIDI's 8-to-7, each seven bytes a byte of their top bits (bit i: byte i's), then their low seven; a part
 * LV_PART packed bytes at most. Each op is queued as the engine gets it (lv_q: their order, the STATE among them where
 * it was asked), the queue sent from the main loop as the wire takes it (lv_pump: each pass, a part a pass), never from
 * the render; without the page's bit, nothing. The queue full: the op dropped (lv.lost; the page's next STATE puts its
 * listeners right), a key's pressure leaving room for the rest */
#define LV_RAW (LV_PART / 8u * 7u)       /* a part's own bytes */
#define LV_Q 512u                        /* the queue's bytes: an op n (1 + its args), op, args; a long one 0x80, op,
                                          * src (lv_long); STATE 0x81 (512: a STATE with its running state holds the wire
                                          * some 60 ms, card #131; 192 before) */
#define LV_KEEP 48u                      /* the room a pressure leaves */
static struct {
    uint16_t r, n;                       /* the queue: its first byte, its bytes */
    uint8_t part, state;                 /* the head's next part (a long op's, STATE's); a STATE queued (lv_big) */
    uint8_t swell, compare;              /* as last told */
    uint32_t kd, lost;                   /* the keys whose down was told; the ops dropped */
} lv;
static uint8_t lv_q[LV_Q] PLAT_POOL;
static uint8_t lv_big[LV_STATE] PLAT_POOL;   /* STATE's */
static uint16_t lv_big_n;
_Static_assert(sizeof(scene_t) == FRS_SCENE_BYTES && FRS_NKEYS <= 32u, "a scene told is FRS_SCENE_BYTES; lv.kd's keys");
_Static_assert(LV_STATE <= 127u * LV_RAW, "STATE's parts counted in a 7-bit byte");

/* MIDI's 8-to-7: s's n bytes into o, each seven a byte of their top bits (bit i: byte i's), then their low seven bits;
 * the packed length (n and a byte a seven) */
static uint32_t pf_pack(uint8_t *o, const uint8_t *s, uint32_t n)
{
    uint32_t i, j, k = 0;
    for (i = 0; i < n; i += 7u) {
        uint32_t h = k++;
        o[h] = 0;
        for (j = 0; j < 7u && i + j < n; j++) {
            o[h] |= (uint8_t)((s[i + j] >> 7) << j);
            o[k++] = s[i + j] & 0x7Fu;
        }
    }
    return k;
}

static uint8_t lv_at(uint32_t i) { return lv_q[(lv.r + i) % LV_Q]; }
static void lv_drop(uint32_t n)
{
    lv.r = (uint16_t)((lv.r + n) % LV_Q);
    lv.n = (uint16_t)(lv.n - n);
}
static void lv_put(const uint8_t *b, uint32_t n, uint32_t keep)   /* a record whole, or none (keep: room left after) */
{
    uint32_t i;
    if (!lv_on && b[0] != 0x81u)
        return;
    if (LV_Q - lv.n < n + keep) {
        lv.lost++;
        return;
    }
    for (i = 0; i < n; i++)
        lv_q[(lv.r + lv.n + i) % LV_Q] = b[i];
    lv.n = (uint16_t)(lv.n + n);
}

/* an op and its args (as many as its top bits say: 0, 2 or 4) */
static void lv_ev(uint32_t op, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t keep)
{
    uint32_t n = op >> 6 == 1u ? 2u : op >> 6 == 2u ? 4u : 0u;
    uint8_t b[6] = {(uint8_t)(1u + n), (uint8_t)op, (uint8_t)a0, (uint8_t)a1, (uint8_t)a2, (uint8_t)a3};
    lv_put(b, 2u + n, keep);
}
static void lv_op(uint32_t op, uint32_t a0, uint32_t a1) { lv_ev(op, a0, a1, 0, 0, 0); }
static void lv_set(uint32_t p, uint32_t v)   /* (past 255, FST_PARAM's byte too short: FST_PARAM16, the LEAD's from
                                              * FEEDBACK on: card #152) */
{
    if (p > 255u)
        lv_ev(FST_PARAM16, p & 0xFFu, p >> 8, v, 0, 0);
    else
        lv_ev(FST_PARAM, p, v, 0, 0, 0);
}
static void lv_long(uint32_t op, uint32_t src)   /* src: a slot (scene_get's), LV_ANC the ANCHOR's: read as sent */
{
    uint8_t b[3] = {0x80u, (uint8_t)op, (uint8_t)src};
    lv_put(b, 3u, 0);
}

/* app.c's param_set (the macro, top): a setting that moves, told first (as param_set clamps it), but the FM-1's alone
 * (SPK/JACK, the screen's, the metronome: its ticks the player's; COUNT-IN: with REC, FST_RECORDING); the LEAD's too,
 * a scene's (card #152: a listener with the LEAD turns them as this one, a preset's sound, MODE's voices let go; one
 * without it leaves a setting past its NPARAM) */
static void lv_param_set(int p, int32_t x)
{
    if (lv_on && p >= 0 && p < (int)NPARAM && st.v[p] != (uint8_t)clamp(x, 0, PD[p].max) &&
        param_kind((uint32_t)p) != PK_NONE)
        lv_set((uint32_t)p, (uint32_t)clamp(x, 0, PD[p].max));
    (param_set)(p, x);
}

/* the LEAD's settings whole (card #152, FST_LEAD: how 1 a scene's LEAD taken, its own object's, ldp_load; 0 the LEAD as
 * it is, where a setting was put in place without param_set: a preset of the player's, FX back, their PRESET alone) */
static void lv_lead(uint32_t how)
{
#if FRS_LEAD
    uint8_t b[3u + NLDS];
    if (!lv_on)
        return;
    b[0] = (uint8_t)(2u + NLDS);
    b[1] = FST_LEAD;
    b[2] = (uint8_t)how;
    memcpy(b + 3, &st.v[P_LD_WAVE], NLDS);
    lv_put(b, sizeof b, 0);
#else
    (void)how;
#endif
}
/* the SMK-37's wheels on the LEAD, as eng_lead_wheels takes them (FST_LEAD_WHEELS: the pitch wheel's u16 plus 8192),
 * leaving room as a pressure does (they move as one) */
static void lv_wheels(uint32_t mod, int32_t bend)
{
    uint32_t b = (uint32_t)(clamp(bend, -8192, 8191) + 8192);
    if (lv_on)
        lv_ev(FST_LEAD_WHEELS, mod > 127u ? 127u : mod, b & 0xFFu, b >> 8, 0, LV_KEEP);
}

/* REC tapped: the take's COUNT-IN and OVERDUB as they are (FST_RECORDING: frs_count_in, frs_overdub), then the record
 * itself on the layer (LV_RECORD: frs_record_layer, eng_record_on's) */
static void lv_rec(uint32_t layer)
{
    lv_ev(FST_RECORDING, st.v[P_COUNT_IN] != 0, overdub != 0, 0, 0, 0);
    lv_op(LV_RECORD, layer, 0);
}

static void lv_key(uint32_t k, int note, uint32_t vel, int single)
{
    if (!lv_on || k >= 32u || st.v[P_KB] > 3u)
        return;
    lv.kd |= 1u << k;
    lv_ev(st.v[P_KB] == 3u ? FST_KEY_LEAD : single ? LV_KEY_SINGLE : FST_KEY_DOWN, k, (uint32_t)clamp(note, 0, 127),
          st.v[P_KB], vel < 1u ? 1u : vel > 127u ? 127u : vel, 0);   /* (the LEAD's: FST_KEY_LEAD, which a listener
                                          * without the LEAD skips; KEY_DOWN's track 3 would play its bass, card #152) */
}
static void lv_key_up(uint32_t k)        /* (a key whose down was told: SAVE's, EDIT's keys never were) */
{
    if (k >= 32u || !(lv.kd >> k & 1u))
        return;
    lv.kd &= ~(1u << k);
    lv_ev(FST_KEY_UP, k, 0, 0, 0, 0);
}
static void lv_pressure(uint32_t k, uint8_t p)   /* before key_pressure: told when it acts (past its 2 either way, to 0,
                                                  * its first press in) */
{
    uint32_t was = key[k].prs;
    if (k < 32u && (lv.kd >> k & 1u) &&
        (p > was + 2u || p + 2u < was || (p == 0 && was) || (p > 12u && !key[k].pressed_in)))
        lv_ev(LV_KEY_PRESSURE, k, p, 0, 0, LV_KEEP);
}

/* MIDI in, as eng_midi took it (the clock and Start, the channels' messages on FRASIL's channels, All Sound Off on
 * any); MIDI learn's CC not (its setting told instead: lv_cc_took) */
static void lv_midi(uint32_t pkt)
{
    uint32_t s = pkt >> 8 & 0xFFu, ch = (s & 15u) + 1u, d1 = pkt >> 16 & 0x7Fu, d2 = pkt >> 24 & 0x7Fu;
    if (!lv_on || lv_cc_took)
        return;
    if (s == 0xF8u || s == 0xFAu) {
        if (st.v[P_CLOCK])               /* (CLOCK INTERNAL: not taken) */
            return;
    } else if (s < 0x80u || s >= 0xF0u ||
               (!((s & 0xF0u) == 0xB0u && d1 == 120u) && !on_ch(P_CANAL, ch) && !on_ch(P_CH_GLINT, ch) &&
                !on_ch(P_CH_BASS, ch))) {
        return;
    }
    lv_ev(FST_MIDI, s, d1, d2, 0, 0);    /* (taken at once: 0 ms ahead) */
}

/* a preset taken (eng_preset_apply): its settings, in its order, as it sets them */
static void lv_preset(const uint8_t *in, uint32_t n)
{
    int l = preset_layer(in, n);
    uint32_t i;
    if (!lv_on || l < 0)
        return;
    for (i = 0; i < in[5]; i++) {
        uint32_t p = in[PRESET_HEAD + 2u * i];
        if (p < NPARAM && preset_has((uint32_t)l, p))
            lv_set(p, (uint32_t)clamp(in[PRESET_HEAD + 2u * i + 1u], 0, PD[p].max));
    }
}

/* RECORD OFF (eng_record_off), told as the settings it turns: the three off, the track's PLAYS back */
static void lv_record_off(void)
{
    static const uint8_t RP[3] = {P_SQ_REC, P_BQ_REC, P_DQ_REC}, TP[3] = {P_DQ_ON, P_SQ_TYPE, P_BQ_ON};
    uint8_t rec[3], plays[3];
    uint32_t i;
    if (!lv_on) {
        eng_record_off();
        return;
    }
    for (i = 0; i < 3u; i++) {
        rec[i] = st.v[RP[i]];
        plays[i] = st.v[TP[i]];
    }
    eng_record_off();
    for (i = 0; i < 3u; i++)
        if (rec[i])
            lv_set(RP[i], 0);
    for (i = 0; i < 3u; i++)
        if (plays[i] != st.v[TP[i]])
            lv_set(TP[i], st.v[TP[i]]);
}

static void lv_watch(int swell)          /* each pass: ENV held, COMPARE, told as they change */
{
    if (!lv_on)
        return;
    if ((uint8_t)(swell != 0) != lv.swell) {
        lv.swell = (uint8_t)(swell != 0);
        lv_op(LV_SWELL, lv.swell, 0);
    }
    if (st.compare != lv.compare) {
        lv.compare = st.compare;
        lv_op(LV_COMPARE, lv.compare, 0);
    }
}

/* a FRS_P_MOMENT setting as it is (ARP II's LATCH: the arpeggio's own, ARP held turns it alone) */
static uint8_t lv_moment(uint32_t p)
{
    return p == P_GA_LATCH ? arp.latch : p == P_BA_LATCH ? barp.latch : st.v[p];
}

/* STATE asked: the start, as it is now, into lv_big, then the engine's running state at the same instant (live_save:
 * the brain whole, the sound's read between two blocks; its scenes XORed with this start's), queued where it was asked
 * (each op before it, made before it). Each only read: taking it changes nothing of the sound or the brain */
static void lv_state(void)
{
    static const uint8_t H = 0x81u;
    scene_t sc;
    uint8_t *o = lv_big, ld[LV_LEAD];
    uint32_t i, n, p, m = 0, at, ln, lm = 0;
    if (lv.state)
        return;
    scene_get(fm.scene, &sc);            /* (as STORE makes it, the scene's: its name, what it plays now) */
    for (i = 0; i < NSOUND; i++)
        sc.v[i] = kept_v(i);
    scene_notes_take(&sc);
    o[0] = (uint8_t)(FRS_SCENE_BYTES & 0xFFu);
    o[1] = (uint8_t)(FRS_SCENE_BYTES >> 8);
    memcpy(o + 2, &sc, FRS_SCENE_BYTES);
    n = 2u + FRS_SCENE_BYTES;
    o[n] = (uint8_t)state_save(o + n + 1u);
    n += 1u + o[n];
    at = n++;
    for (p = 0; p < NPARAM && p < 256u && m < FST_MOVES; p++)   /* (as fst_capture takes them) */
        if (param_kind(p) == PK_MOMENT) {
            o[n++] = (uint8_t)p;
            o[n++] = lv_moment(p);
            m++;
        }
    o[at] = (uint8_t)m;
    o[n++] = pf_playing();
    ln = live_save(o + n + 2u, LV_LIVE, o + 2, swell_held(btn_now), 0, 1, ld, sizeof ld, &lm);   /* (ENV held as the
                                          * last pass gave it; no engine alone's part; 0: it did not fit, none told; the
                                          * LEAD's part read with it, on the stack) */
    o[n] = (uint8_t)(ln & 0xFFu);
    o[n + 1u] = (uint8_t)(ln >> 8);
    n += 2u + ln;
    if (!ln || n + 2u + lm > LV_STATE)   /* (after the first, in the room it leaves: none, none told) */
        lm = 0;
    memcpy(o + n + 2u, ld, lm);
    o[n] = (uint8_t)(lm & 0xFFu);
    o[n + 1u] = (uint8_t)(lm >> 8);
    n += 2u + lm;
    lv_big_n = (uint16_t)n;
    i = lv.lost;
    lv_put(&H, 1u, 0);
    lv.state = lv.lost == i;
}

/* the head's next part (a long op's, STATE's): cmd 11 or 9, part, parts, LV_RAW bytes packed. A long op's scene read
 * where it is, each part (a slot's, NEW SHARD's, the ANCHOR's) */
static void lv_part(uint32_t h, uint8_t *b, uint8_t *raw)
{
    scene_t sc;
    const uint8_t *s = lv_big;
    uint32_t n = lv_big_n, op = 0, at = (uint32_t)lv.part * LV_RAW, i, k, parts;
    if (h == 0x80u) {
        uint32_t src = lv_at(2);
        op = lv_at(1);
        if (src == LV_ANC) {
            s = (const uint8_t *)&anc.sc;
        } else {
            scene_get(src, &sc);
            s = (const uint8_t *)&sc;
        }
        n = 1u + FRS_SCENE_BYTES;
    }
    parts = (n + LV_RAW - 1u) / LV_RAW;
    for (i = 0; i < LV_RAW && at + i < n; i++)
        raw[i] = h == 0x81u ? s[at + i] : at + i == 0u ? (uint8_t)op : s[at + i - 1u];
    b[0] = h == 0x81u ? 9u : 11u;
    b[1] = lv.part;
    b[2] = (uint8_t)parts;
    k = pf_pack(b + 3, raw, i);
    if (pf_send(b, 3u + k))
        return;
    if (++lv.part < parts)
        return;
    lv.part = 0;
    lv_drop(h == 0x81u ? 1u : 3u);
    if (h == 0x81u)
        lv.state = 0;
}

/* each pass: the queue to the page as the wire takes it, an op a frame, a long one's part a pass */
static void lv_pump(void)
{
    uint8_t b[3u + LV_PART], raw[LV_RAW];
    while (lv.n) {
        uint32_t h = lv_at(0), i;
        if (h >= 0x80u) {
            lv_part(h, b, raw);
            return;
        }
        for (i = 0; i < h; i++)
            raw[i] = lv_at(1u + i);
        b[0] = 10u;
        if (pf_send(b, 1u + pf_pack(b + 1, raw, h)))
            return;
        lv_drop(1u + h);
    }
}

static void lv_live(int on)              /* HELLO's bit: from its first, what STATE doesn't hold told as it changes */
{
    if (on && !lv_on) {
        lv.kd = 0;
        lv.swell = lv.compare = 0;
    }
    lv_on = (uint8_t)(on != 0);
}
static void lv_end(void)                 /* the page gone: nothing more, nothing waiting */
{
    lv_on = 0;
    lv.r = lv.n = 0;
    lv.part = lv.state = 0;
    lv.kd = 0;
}

/* a UI frame: what changed since the page last heard, then what sounds (20 a second) */
static void pf_tick(void)
{
    uint32_t now = plat_ms(), i, n, sent = 0, btn, k;
#if FRS_LEAD
    uint8_t b[4u + 2u * PF_DUMP], keys = st.lead ? 3u : st.glint ? 1u : st.bass ? 2u : 0u;
#else
    uint8_t b[4u + 2u * PF_DUMP], keys = st.glint ? 1u : st.bass ? 2u : 0u;
#endif
    if (!pf.on)
        return;
    if (now - pf.heard > 5000u) {
        pf.on = 0;
        pf_quiet(0);
        lv_end();
        return;
    }
    if (fm.store_hold)                   /* a backup streaming: its frames alone on the wire */
        return;
    if ((fm.scene != pf.scene || (st.dirty != 0) != pf.dirty) && pf_scene())
        return;                          /* (no room now: all of it at the next frame) */
    if (fm.page != pf.page || fm.octave + 3 != pf.oct || keys != pf.keys) {
        b[0] = 5u, b[1] = fm.page, b[2] = (uint8_t)(fm.octave + 3), b[3] = keys;
        if (pf_send(b, 4u))
            return;
        pf.page = fm.page, pf.oct = (uint8_t)(fm.octave + 3), pf.keys = keys;
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
            b[5] = i == fm.touched && touched_now() ? fm.touched_enc : 127u;
            if (pf_send(b, 6u))
                return;
            pf.v[i] = st.v[i];
            sent++;
        }
    }
    btn = plat_buttons() & ~saver.btn_spent;   /* (one that did nothing, woke the screen or closed Learn's welcome: not
                                                * told, Learn's step not taken by it: admin #881) */
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
    for (k = n = 0; k < NKEYS && k < FRS_NKEYS; k++)   /* the hand pressing in: the held key pressed hardest */
        if ((keys_prev >> k & 1u) && key[k].prs > n)
            n = key[k].prs;
    if (n != pf.prs && (pf.prs == 0xFFu || (((n == 0) != (pf.prs == 0) || n >= pf.prs + 4u || n + 4u <= pf.prs) &&
                                            now - pf.prs_at >= 50u))) {
        b[0] = 13u, b[1] = (uint8_t)n;
        if (pf_send(b, 2u))
            return;
        pf.prs = (uint8_t)n, pf.prs_at = now;
    }
    if (now - pf.frame_at >= 50u) {
        pf.frame_at = now;
        pf_frame();
    }
}

/* ------------------------------------------------------------ Learn's START --- */
/* Learn (learn.frasilsynth.com) begins from a known state (card #153; Styves, admin #795, #800): as soon as the FM-1
 * answers its HELLO, the page sends
 *   page -> FM-1  14 START n, then n (setting (2), value (2))   n 127 at most: 513 bytes between F0 and F7, of the 640
 *                   fm1-base takes whole (APP_SYSEX), one message, no parts
 *   FM-1 -> page  14 START status   0 done: the state in, then VALUES gone out (every setting, as after a load); 1
 *                   refused: a message the wrong length, or a START under way already. A page that said no HELLO hears
 *                   it too (without VALUES); an FM-1 before it (3.2) never answers: the page goes on
 * Nothing is asked: WELCOME TO LEARN MODE on the screen (ui.c welcome_screen) as it begins, up until a button, a key
 * or a knob, which closes it and does nothing else (welcome_buttons). A backup streaming: after it. What plays let go
 * first, as PLAY tapped twice lets it go (RELEASE ALL: each note fading as its RELEASE has it, never a cut), its fading
 * waited for (the hand off the keys, the tracks' meters quiet: LS_FADE_MS at most); then NEW SHARD as SAVE › LOAD
 * loads it (KEYS to their defaults), CUT and SND+NOTES whatever MORPH and LOAD say (theirs kept after), the play page;
 * then each pair in order as a knob sets it (KEYS' as their page does, kept with the settings). A setting past NPARAM,
 * a value past its max: skipped (NEW SHARD's stays). Unsaved, as after any edit; the stored scenes never touched. One
 * a visit, the page's */
#define LS_MAX 127u                      /* a START's pairs at most (its count, a 7-bit byte) */
#define LS_FADE_MS 3000u                 /* what was let go: its fading waited for, at most */
#define LS_QUIET 0.01f                   /* a track's meter this low (frasil_meters): faded */
static struct {
    uint8_t due, let, n, nos;            /* due: 1 a START waiting (a backup, the fading), 2 in, VALUES going out; let:
                                          * what played let go; its pairs kept; the refusals owed */
    uint32_t at;                         /* let go at */
} ls;
static uint16_t ls_p[LS_MAX];            /* the pairs, each checked: the setting, its value (381 bytes: in .bss, the
                                          * pool's spare thin) */
static uint8_t ls_v[LS_MAX];
_Static_assert(5u + 4u * LS_MAX <= 640u && NPARAM < 16384u, "a START whole in one frame; its settings in two bytes");

static void learn_start(const uint8_t *b, uint32_t n)   /* START's bytes after its cmd, n of them */
{
    uint32_t i, k = 0;
    if (!n || n != 1u + 4u * b[0] || ls.due) {
        if (ls.nos < 255u)
            ls.nos++;
        return;
    }
    for (i = 0; i < b[0]; i++) {
        const uint8_t *e = b + 1u + 4u * i;
        uint32_t p = (uint32_t)e[0] << 7 | e[1], v = (uint32_t)e[2] << 7 | e[3];
        if (p < NPARAM && v <= PD[p].max) {
            ls_p[k] = (uint16_t)p;
            ls_v[k++] = (uint8_t)v;
        }
    }
    ls.n = (uint8_t)k;
    ls.due = 1;
    ls.let = 0;
}

static int learn_heard(void)             /* something still heard: a track's meter, the LEAD's */
{
    float m[4];
    uint32_t i;
    frasil_meters(m);
    for (i = 0; i < 4u; i++)
        if (m[i] > LS_QUIET)
            return 1;
#if FRS_LEAD
    return frasil_lead_meter() > LS_QUIET;
#else
    return 0;
#endif
}

static void learn_set(uint32_t p, uint32_t v)   /* a pair, as a knob sets it (encoders: keys_turn, the settings') */
{
    if (p >= P_KY_DR_HARM && p <= P_KY_BS_STACK && ((p - P_KY_DR_HARM) & 3u) == 1u && v && !st.v[p - 1u])
        param_set((int)(p - 1u), 1);     /* (NOTES to CHORD with HARMONY OFF: HARMONY on too) */
    param_set((int)p, (int32_t)v);
    if ((p >= P_KY_DR_HARM && p <= P_KY_BS_STACK) || p == P_METRO || p == P_COUNT_IN || p == P_ARP_SNAP) {
        if (p == P_METRO && st.v[P_METRO])
            fm.metro_on = st.v[P_METRO];
        prefs_later();
    }
}

static void learn_apply(void)            /* NEW SHARD, then the pairs */
{
    uint8_t morph = st.v[P_MORPH], load = st.v[P_LOAD];
    uint32_t i;
    nm.on = 0;                           /* (a name, the grid, the help, the logo: gone; a pop-up stays, SEL's) */
    ng.on = ng.due = 0;
    fm.edit_map = fm.help = fm.logo = 0;
    fm.page = PG_SON;
    param_set(P_MORPH, 0);               /* (CUT, SND+NOTES: the scene whole, at once; told to LIVE as set) */
    param_set(P_LOAD, 0);
    scene_go(SC_NEW);
    param_set(P_MORPH, morph);
    param_set(P_LOAD, load);
    for (i = 0; i < ls.n; i++)
        learn_set(ls_p[i], ls_v[i]);
    if (pf.on)
        pf.dump = 1u;                    /* (every setting told, as after a load) */
}

/* each UI frame, before the screen: the refusals owed, the welcome, a START's steps, its answer once VALUES went out */
static void learn_tick(void)
{
    uint32_t now = plat_ms();
    uint8_t b[2] = {14u, 1u};
    while (ls.nos && !fm.store_hold && !pf_send(b, 2u))   /* (a backup streaming: its frames alone on the wire) */
        ls.nos--;
    if (ls.due == 1u) {
        if (fm.store_hold)               /* a backup streaming: after it */
            return;
        if (!ls.let) {                   /* what plays let go (as PLAY tapped twice: frs_release), the welcome up */
            ls.let = 1;
            ls.at = now;
            if (holding_any() || app_backup_playing()) {
                sound_off();
                grid_top(0);
                lv_op(LV_RELEASE, 0, 0);
            }
            welcome.on = 1;
            welcome.drawn = 0;
            if (saver.on) {              /* (the screen wakes) */
                saver.on = saver.party = 0;
                saver_touch();
            }
            if (ann_on())
                ann_say("Welcome to Learn mode.", 0, ANN_NOW);
        }
        if (now - ls.at < LS_FADE_MS && (keys_prev || learn_heard()))
            return;                      /* (fading: the hand off the keys, every track quiet) */
        learn_apply();
        ls.due = 2;
    }
    if (ls.due == 2u && (!pf.on || !pf.dump)) {   /* VALUES gone out (pf_tick): done */
        b[1] = 0;
        if (!pf_send(b, 2u))
            ls.due = 0;
    }
}

/* the welcome up (no pop-up, no name over it): the first button, key or knob closes it, and that alone (Styves, admin
 * #881: up until then, never gone by itself; the lesson's first step never taken by the hand that closed it), as one
 * wakes the screensaver: the button, the key dropped till let go (no tap, no hold, no note, nothing told to the page:
 * saver's btn_spent, keys_spent), the knob's detents while it turns on (SAVER_GRACE_MS of rest). Those down as it came:
 * theirs */
static int welcome_waits(void) { return welcome.on && !pop.on && !nm.on; }

static uint32_t welcome_buttons(uint32_t btn)   /* (input: the buttons read) */
{
    uint32_t down = btn & ~btn_prev;
    if (!down || !welcome_waits())
        return btn;
    welcome.on = 0;
    saver.btn_spent |= down;
    return btn & ~down;
}

static uint32_t welcome_keys(uint32_t keys)   /* (keys_scan: the keys read) */
{
    uint32_t down = keys & ~keys_prev;
    if (!down || !welcome_waits())
        return keys;
    welcome.on = 0;
    saver.keys_spent |= down;
    return keys & ~down;
}

static int welcome_knob(void)            /* (encoders: a detent read) 1: it closed the welcome */
{
    if (!welcome_waits())
        return 0;
    welcome.on = 0;
    saver.woke_at = plat_ms();
    saver.enc_until = saver.woke_at + SAVER_GRACE_MS;
    return 1;
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
        lv_end();
        return 1;
    }
    if (p[3] == 9u) {                    /* STATE? (a page listening: LIVE's start) */
        if (pf.on)
            lv_state();
        return 1;
    }
    if (p[3] == 14u) {                   /* START (Learn's, any page: HELLO not needed) */
        learn_start(p + 4, n - 4u);
        return 1;
    }
    if (p[3] != 1u)
        return 1;
    pf_quiet(n > 5u && p[5] < 0x80u && (p[5] & 1u));   /* (HELLO: version, then flags: 1 COMPUTER AUDIO, 2 LIVE) */
    lv_live(n > 5u && p[5] < 0x80u && (p[5] & 2u));
    if (!pf.on) {                        /* a page arrives: everything, from the start */
        pf.on = 1;
        pf.scene = 0xFFu, pf.page = 0xFFu, pf.dump = 1u;
        pf.btn = plat_buttons() & ~saver.btn_spent;
        pf.kd = keys_prev;
        pf.frame_at = plat_ms();
    }
    pf.scene = 0xFFu, pf.page = 0xFFu, pf.prs = 0xFFu;   /* (told again: one the page missed comes back within 2 s;
                                          * PRESSURE after the answer: a page knows this FM-1 tells it) */
    pf.heard = plat_ms();
    if (fm.store_hold)                   /* a backup streaming: heard, not answered */
        return 1;
    b[0] = 1u, b[1] = PF_VER;
    pf_two(b + 2, NPARAM);
    for (i = 0; ver[i]; i++)
        b[4u + i] = (uint8_t)ver[i];
    if (!pf_send(b, 4u + i))
        pf_battery();
    return 1;
}
#else
static void pf_tick(void) {}
static void lv_key(uint32_t k, int note, uint32_t vel, int single) {}
static void lv_key_up(uint32_t k) {}
static void lv_pressure(uint32_t k, uint8_t p) {}
static void lv_midi(uint32_t pkt) {}
static void lv_op(uint32_t op, uint32_t a0, uint32_t a1) {}
static void lv_long(uint32_t op, uint32_t src) {}
static void lv_set(uint32_t p, uint32_t v) {}
static void lv_preset(const uint8_t *in, uint32_t n) {}
static void lv_record_off(void) { eng_record_off(); }
static void lv_rec(uint32_t layer) {}
static void lv_watch(int swell) {}
static void lv_pump(void) {}
static void lv_lead(uint32_t how) {}
static void lv_wheels(uint32_t mod, int32_t bend) {}
static uint32_t welcome_buttons(uint32_t btn) { return btn; }   /* (no Learn here: no welcome) */
static uint32_t welcome_keys(uint32_t keys) { return keys; }
static int welcome_knob(void) { return 0; }
#endif

void app_frame(void)
{
#if defined PLAT_HOST && !defined PLAT_WEB
    uint32_t t0, k, ms = plat_ms();
    static uint32_t last_ms;
#endif
    input();
    load_watch();
    batt_tick();
    saver_tick();
#ifndef PLAT_WEB
    learn_tick();                        /* (Learn's START: before the screen draws, PERFORM's VALUES after) */
#endif
#if defined PLAT_HOST && !defined PLAT_WEB
    k = saver.on ? 1u + saver.party : 0u;
    t0 = plat_ticks();
    ui_frame();
    t0 = plat_ticks() - t0;
    ui_cost.ticks[k] += t0;
    ui_cost.tmax[k] = t0 > ui_cost.tmax[k] ? t0 : ui_cost.tmax[k];
    ui_cost.n[k]++;
    ui_cost.ms[k] += ms - last_ms;
    last_ms = ms;
#else
    ui_frame();
#endif
    speak();                             /* what the screen shows, to a screen reader listening (speak.c) */
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
    return lit[0] | lit[1] | lit[2] | lit[3] || bass_note >= 0 || arp.n || barp.n || (sq.run && notes) ||
           (bq.run && bnotes) || (dq.run && dseq_has_notes()) || keys_prev || midi;
}

/* a backup starts, nothing playing: a record the store still has in an earlier layout saved in this one (as boot made
 * it over: the backup's scenes can go back one by one), SCENES' LOAD kept if it changed (else when SCENES is left). The sound as it is now is not stored: STORE is the player's */
void app_backup_flush(void)
{
    if ((store_old & 1u) && scenes_save() == 0)
        store_old &= ~1u;
    if (set.magic == SET_MAGIC &&
        (store_old & 2u || fm.prefs_due || set.load != st.v[P_LOAD]))
        prefs_save();
}

void app_backup_hold(int on) { fm.store_hold = (uint8_t)(on != 0); }

/* restored (SEL said yes; nothing was playing): every note let go, then everything as at power-on */
void app_backup_restored(void)
{
    nm.on = 0;                           /* (what was open went with the state it was about) */
    ng.on = ng.due = 0;
    fm.edit_map = 0;
    count_stop();
    /* a backup of an earlier FRASIL has no OBJ_UPRE, and base/device/backup.c makes it absent: the player's presets, which
     * it says nothing about, written back from RAM (a backup with them restores them: the store has it, this does nothing) */
    if (up.magic == UPR_MAGIC && plat_store_load(OBJ_UPRE, raw, sizeof raw) != (int)sizeof up)
        plat_store_save(OBJ_UPRE, &up, sizeof up);
    release_all();
    arp.n = arp.pn = 0;
    arp.dirty = arp.paused = 0;
    arp_stop(&arp);
    barp.n = barp.pn = 0;
    barp.dirty = barp.paused = 0;
    arp_stop(&barp);
    seq_note_off();
    sq.run = 0;
    bseq_stop();
    dseq_stop();
    state_load();
    buttons_spent();
}

#if defined PLAT_HOST && !defined PLAT_WEB
static int16_t saver_mark[2];
static struct {
    int32_t bad, min, max, ways, edge;   /* saveraim: off the minute, the soonest and latest corner (steps), the corners
                                          * and ways seen (bits), the nearest a start came to two edges */
} saver_aims;

/* saveraim N: N starts as the screensaver draws them (from N times plat_ms might give), each run as it drifts: its
 * first corner within 55 .. 65 s (2200 .. 2600 steps) and where it was aimed, never a start on an edge or near two */
static void saver_aim_check(uint32_t n)
{
    uint32_t i, ms = 600000u;
    memset(&saver_aims, 0, sizeof saver_aims);
    saver_aims.min = 1 << 30;
    saver_aims.edge = 1 << 30;
    for (i = 0; i < n; i++) {
        uint32_t r;
        int16_t x, y;
        int8_t dx, dy;
        int32_t aim, t, e;
        ms += 1u + (i * 2654435761u >> 22);  /* (any ms: 1 .. 1024 apart) */
        r = ms * 2654435761u;
        aim = saver_aim(r ^ (r >> 15), &x, &y, &dx, &dy);
        e = x < SAVER_MAXX - x ? x : SAVER_MAXX - x;
        t = y < SAVER_MAXY - y ? y : SAVER_MAXY - y;
        e = e > t ? e : t;               /* (how far from the nearer edge, on the axis farther from one) */
        saver_aims.edge = e < saver_aims.edge ? e : saver_aims.edge;
        if (x <= 0 || x >= SAVER_MAXX || y <= 0 || y >= SAVER_MAXY || e < SAVER_AIM_EDGE)
            saver_aims.bad++;
        saver_aims.ways |= 1 << ((dx > 0) + 2 * (dy > 0));
        for (t = 1; t <= (int32_t)(70000u / SAVER_STEP_MS); t++) {
            saver_move(&x, &y, &dx, &dy);
            if (saver_cornered(x, y))
                break;
        }
        saver_aims.ways |= 16 << ((x > 0) + 2 * (y > 0));
        if (t < (int32_t)(55000u / SAVER_STEP_MS) || t > (int32_t)(65000u / SAVER_STEP_MS) || t != aim)
            saver_aims.bad++;
        saver_aims.min = t < saver_aims.min ? t : saver_aims.min;
        saver_aims.max = t > saver_aims.max ? t : saver_aims.max;
    }
    printf("  saveraim %u starts: the first corner %d .. %d steps in (%.2f .. %.2f s), %d off, starts %d px at least "
           "from a corner, ways and corners 0x%x\n", (unsigned)n, saver_aims.min, saver_aims.max,
           saver_aims.min * SAVER_STEP_MS / 1000.0, saver_aims.max * SAVER_STEP_MS / 1000.0, saver_aims.bad, saver_aims.edge, (unsigned)saver_aims.ways);
}
/* the screensaver's checks: "" on, _x _y where, _steps _bounces since it came, _dist how far since savermark, _out
 * steps outside the screen, _lit the logo's box's pixels not black, _stray the others not black (fb: the screen) */
static int saver_expect(const char *w)
{
    uint32_t x, y, lit = 0, stray = 0;
    if (!*w)
        return saver.on;
    if (!strcmp(w, "_x"))
        return saver.x;
    if (!strcmp(w, "_y"))
        return saver.y;
    if (!strcmp(w, "_steps"))
        return (int)saver.steps;
    if (!strcmp(w, "_bounces"))
        return (int)saver.bounces;
    if (!strcmp(w, "_out"))
        return (int)saver_out;
    if (!strcmp(w, "_corners"))          /* corners since it came; the step of the first; the step aimed at */
        return (int)saver.corners;
    if (!strcmp(w, "_corner_step"))
        return (int)saver.corner_step;
    if (!strcmp(w, "_aim"))
        return saver.aim;
    if (!strcmp(w, "_min"))              /* IDLE TIME; the byte the settings' record has (saver_byte) */
        return saver.min;
    if (!strcmp(w, "_byte"))
        return set.saver_off;
    if (!strcmp(w, "_party"))            /* celebrating one */
        return saver.on && saver.party;
    if (!strcmp(w, "_aimbad"))           /* saveraim's */
        return saver_aims.bad;
    if (!strcmp(w, "_aimmin"))
        return saver_aims.min;
    if (!strcmp(w, "_aimmax"))
        return saver_aims.max;
    if (!strcmp(w, "_aimways"))
        return saver_aims.ways;
    if (!strcmp(w, "_aimedge"))
        return saver_aims.edge;
    if (!strcmp(w, "_dist"))
        return abs(saver.x - saver_mark[0]) + abs(saver.y - saver_mark[1]);
    for (y = 0; y < 240u; y++)
        for (x = 0; x < 240u; x++) {
            int in = (int32_t)x >= saver.x && (int32_t)x < saver.x + (int32_t)SAVER_W && (int32_t)y >= saver.y &&
                     (int32_t)y < saver.y + (int32_t)SAVER_H;
            if (fb[y * 240u + x]) {
                if (in)
                    lit++;
                else
                    stray++;
            }
        }
    return !strcmp(w, "_lit") ? (int)lit : !strcmp(w, "_stray") ? (int)stray : -1;
}
#endif
#ifdef PLAT_HOST
static uint8_t bq_kept[SC_BSEQ + 2u];   /* the bass's sequence as bseqkeep kept it: its steps, SEQ STEPS, STEP LENGTH */
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
    else if (!strcmp(what, "glintsdown")) {  /* those with their key down */
        *got = 0;
        for (i = 0; i < GLINT_NV; i++)
            *got += s[i].note >= 0 && s[i].gate;
    }
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
#if FRS_LEAD
    else if (!strcmp(what, "lead"))          /* the keys play LEAD (card #144) */
        *got = st.lead;
    else if (!strcmp(what, "gridon"))        /* a list's browser up (a preset's, a TARGET's) */
        *got = ng.on;
#if PLAT_SMK37
    else if (!strcmp(what, "padanswers"))    /* the choice waiting, as the pads answer it: 1 OK, 3 OK and CANCEL, 0 none */
        *got = (int)pad_answers();
    else if (!strcmp(what, "padmode"))       /* their words: 0 none, 1 OK, 2 CANCEL OK, 3 LOAD SAVE, 4 CANCEL LOAD, 5 CANCEL
                                              * SAVE (pad_mode) */
        *got = (int)pad_mode();
    else if (!strcmp(what, "lfopad"))        /* 1 + the LFO whose pad is held (the knobs its), 0 none */
        *got = lfo_pad_held;
#endif
    else if (!strncmp(what, "lead", 4)) {    /* leadnote: LEAD sounding (OCTAVE on it; POLY: the newest), -1 silent; leadgate;
                                              * leadlvl: its envelope x 100; leadons, leadglides: the notes struck, glided
                                              * to; leadheld, leadvoices: its voices held, sounding; leadsend: its share of
                                              * the cloud at its loudest, x 1000; leadcutmod: its FILTER as it sounds, that
                                              * far from the knob's (an LFO's), x 1000; leadechol, leadechor: each side's
                                              * echo at its loudest, the last render, x 1000; leaddelayms: TIME as the
                                              * beat has it; leadmod: the mod wheel as it came, 0..127; leadfenv: FILTER
                                              * ENV's own envelope x 100 */
        lead_view_t l;
        frasil_lead_view(&l);
        *got = !strcmp(what, "leadcutmod") ? (int)(fm_fabsf(l.cut - (float)st.v[P_LD_CUT] / 127.0f) * 1000.0f + 0.5f)
             : !strcmp(what, "leadechol") ? (int)(l.echol * 1000.0f + 0.5f)
             : !strcmp(what, "leadechor") ? (int)(l.echor * 1000.0f + 0.5f)
             : !strcmp(what, "leaddelayms") ? (int)(ld_delay_s * 1000.0f + 0.5f)
             : !strcmp(what, "leadmod") ? (int)(l.mod * 127.0f + 0.5f)
             : !strcmp(what, "leadfenv") ? (int)(l.fenv * 100.0f + 0.5f)
             : !strcmp(what, "leadnote") ? l.note : !strcmp(what, "leadgate") ? l.gate
             : !strcmp(what, "leadlvl") ? (int)(l.level * 100.0f + 0.5f) : !strcmp(what, "leadons") ? (int)l.ons
             : !strcmp(what, "leadheld") ? (int)l.held : !strcmp(what, "leadvoices") ? (int)l.sounding
             : !strcmp(what, "leadsend") ? (int)(l.send * 1000.0f + 0.5f) : (int)l.glides;
    }
#endif
#if APP_CPU_LINE && !defined PLAT_WEB
    else if (!strcmp(what, "cpuline") || !strcmp(what, "cpucol")) {   /* the load's line as the screen has it (its row
                                              * read back after the label, CL_X on): cpuline its pixels filled from
                                              * there (of CL_W), the rest the track's (-1: the row not the line: the
                                              * saver, the logo); cpucol the fill's colour, 0 slate, 1 yellow, 2 red
                                              * (-1: none, or not the line) */
        static const uint16_t FC[3] = {C_SLATE, C_BATT_LOW, C_RED};
        const uint16_t *r = &fb[CL_Y * 240 + CL_X];
        int32_t n = 0, z, x;
        for (z = 0; z < 3 && swap16(r[0]) != FC[z]; z++)
            ;
        if (z < 3)
            while (n < CL_W && swap16(r[n]) == FC[z])
                n++;
        for (x = n; x < CL_W && swap16(r[x]) == C_CL_TRACK; x++)
            ;
        if (x < CL_W)
            n = -1;
        *got = what[3] == 'l' ? (int)n : n > 0 ? (int)z : -1;
    }
    else if (!strcmp(what, "cpulabel")) {    /* the label's box as the screen has it (x 0 .. CL_X - 1, rows CL_LY ..
                                              * CL_Y): its pixels lit, every one CPU's slate over the night at one of
                                              * the font's 16 levels (-1: anything else in the box) */
        int32_t n = 0, x, y, a;
        for (y = CL_LY; y <= CL_Y && n >= 0; y++)
            for (x = 0; x < CL_X && n >= 0; x++) {
                uint32_t c = swap16(fb[y * 240 + x]);
                for (a = 0; a < 16; a++)  /* (gfx.c cv_text's blend, over C_BG's black) */
                    if (c == ((uint32_t)(C_SLATE >> 11) * (uint32_t)a / 15u << 11 |
                              ((uint32_t)C_SLATE >> 5 & 63u) * (uint32_t)a / 15u << 5 | (C_SLATE & 31u) * (uint32_t)a / 15u))
                        break;
                n = a == 16 ? -1 : n + (a > 0);
            }
        *got = (int)n;
    }
    else if (!strcmp(what, "cpushown"))      /* the load the line shows, whole % (its steps: up at once, down slower) */
        *got = (int)((cl.v + 8u) / 16u);
    else if (!strcmp(what, "cpurows")) {     /* the row drawn alone since last asked (a step that moved the line) */
        *got = (int)cl.rows;
        cl.rows = 0;
    }
    else if (!strcmp(what, "cpulabels")) {   /* the label drawn since last asked (with a band that reached its box) */
        *got = (int)cl.labels;
        cl.labels = 0;
    }
#endif
    else if (!strcmp(what, "bassnote"))      /* the bass's note held, as it sounds (OCTAVE on it), -1 none */
        *got = bass_note < 0 ? -1 : bass_sounding(bass_note);
    else if (!strcmp(what, "barpsteps")) {   /* the bass's arpeggio: its steps played since last asked; barpnotes, barpplay,
                                              * barplatch, bpaused: as GLINT's arpnotes .. paused */
        static uint32_t was;
        *got = (int)(barp.steps - was);
        was = barp.steps;
    }
    else if (!strcmp(what, "barpnotes"))
        *got = barp.n;
    else if (!strcmp(what, "barpplay"))
        *got = barp.pn;
    else if (!strcmp(what, "barplatch"))
        *got = barp.latch;
    else if (!strcmp(what, "bpaused"))
        *got = barp.paused;
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
    } else if (!strcmp(what, "grid")) {     /* the grid now, MIDI clocks from its top (grid.tick); clickgrid: where the
                                              * metronome's last tick fell on it */
        *got = (int)grid.tick;
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
    } else if (!strcmp(what, "octbig")) {   /* the octave large on the screen */
        *got = oct_big();
    } else if (!strcmp(what, "clicks")) {    /* the metronome's ticks; hiclicks: the higher ones; metro: it on */
        *got = (int)st.clicks;
    } else if (!strcmp(what, "hiclicks")) {
        *got = (int)st.hiclicks;
    } else if (!strcmp(what, "metro")) {
        *got = st.metro;
    } else if (!strcmp(what, "extclock")) {  /* a MIDI clock leading the grid now (one came in this half second) */
        *got = grid_clocked(eng_ms());
    } else if (!strcmp(what, "extbpm")) {    /* GLOBAL's TEMPO (EXT): the MIDI clock's tempo as shown, whole BPM; 0 none */
        *got = (int)ext_bpm();
    } else if (!strcmp(what, "clkbpm")) {    /* the MIDI clock's tempo as followed (grid.clk_ms), BPM x 100; 0 none */
        *got = grid_clocked(eng_ms()) ? (int)(250000.0f / grid.clk_ms + 0.5f) : 0;
    } else if (!strcmp(what, "prefsdue")) {  /* the settings asked to be kept, not yet (something sounding) */
        *got = fm.prefs_due;
    } else if (!strcmp(what, "sparkles")) {  /* SPARKLE's glints played */
        *got = (int)spk.n;
    } else if (!strcmp(what, "compgr")) {    /* the compressor's reduction, dB x 10 */
        *got = (int)(frasil_comp_gr() * 10.0f);
    } else if (!strcmp(what, "pumpg")) {     /* the drones' pump gain now x 100 */
        *got = (int)(frasil_pump_gain() * 100.0f + 0.5f);
    } else if (!strcmp(what, "pumplow")) {   /* its lowest since last asked x 100 */
        *got = (int)(pump_low * 100.0f + 0.5f);
        pump_low = 1.0f;
    } else if (!strncmp(what, "meter", 5)) { /* meterN: track N's meter x 100 (0 drones, 1 bass, 2 glints, 3 cloud;
                                              * 4 the LEAD, MIXER's K4) */
        float m[4];
        frasil_meters(m);
#if FRS_LEAD
        *got = (int)((atoi(what + 5) == 4 ? frasil_lead_meter() : m[atoi(what + 5) & 3]) * 100.0f);
#else
        *got = (int)(m[atoi(what + 5) & 3] * 100.0f);
#endif
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
    else if (!strcmp(what, "arpsound"))    /* the note its step sounds now (as played: before OCTAVE), -1 none; barpsound:
                                            * the bass's arpeggio's */
        *got = arp.sounding;
    else if (!strcmp(what, "barpsound"))
        *got = barp.sounding;
    else if (!strcmp(what, "arpnotes"))    /* notes the arpeggio is asked for */
        *got = arp.n;
    else if (!strcmp(what, "kbright") || !strcmp(what, "kdim")) {   /* the keys' lights: bit k = key k */
        uint32_t kb, kd;
        keys_lights(&kb, &kd);
        *got = (int)(what[1] == 'b' ? kb : kd);
    } else if (!strcmp(what, "arpplay"))     /* notes it plays (they change on its steps) */
        *got = arp.pn;
    else if (!strcmp(what, "help"))
        *got = fm.help;
    else if (!strcmp(what, "paused"))    /* the arpeggio waiting (PLAY tapped) */
        *got = arp.paused;
    else if (!strcmp(what, "playhold"))  /* PLAY held, counting down on the screen */
        *got = play_holding() > 0.0f;
    else if (!strcmp(what, "morph"))     /* a scene's MORPH under way */
        *got = mo.on;
    else if (!strcmp(what, "anchor"))    /* ANCHOR: 0 none, 1 set, 2 its MORPH back under way */
        *got = anchor_state(0);
    else if (!strcmp(what, "anchordiff")) {   /* the scene now as STORE keeps it against ANCHOR's (set or let go), its
                                               * name and its notes aside: the bytes that differ; and each of the scene's
                                               * settings against the anchor's as it plays (the drone's preset on them) */
        scene_t a = anc.sc, b;
        const uint8_t *x = (const uint8_t *)&a, *y = (const uint8_t *)&b;
        uint32_t i, n = 0;
        memset(&b, 0, sizeof b);
        for (i = 0; i < NSOUND; i++)
            b.v[i] = kept_v(i);
        scene_notes_take(&b);
        for (i = 0; i < 2u; i++) {
            scene_t *s = i ? &b : &a;
            memset(s->name, 0, SC_NAME);
            memset(s->name_more, 0, SC_MORE);
            s->nch = s->narp = s->nbarp = 0;
            memset(s->ch, 0, sizeof s->ch);
            memset(s->arp, 0, sizeof s->arp);
            memset(s->barp, 0, sizeof s->barp);
            s->bass = 0;
        }
        for (i = 0; i < sizeof a; i++)
            n += x[i] != y[i];
        for (i = 0; i < NSOUND; i++)
            n += st.v[i] != anc_val(i);
        for (i = 0; i < NANC_X; i++)
            n += st.v[ANC_X[i].p] != anc_val(ANC_X[i].p);
        *got = (int)n;
    } else if (!strcmp(what, "moff")) {  /* the settings a glide still moves (MORPH's, ANCHOR's): how many */
        uint32_t i, n = 0;
        for (i = 0; i < NMORPH; i++)
            n += morph_off[i] != 0.0f;
        *got = (int)n;
    } else if (!strncmp(what, "heard", 5) && atoi(what + 5) < NMORPH)   /* heardN: setting N as it sounds now, a glide
                                                                         * on it (its LFO aside) */
        *got = (int)(st.v[atoi(what + 5)] + morph_off[atoi(what + 5)] + (st.v[atoi(what + 5)] + morph_off[atoi(what + 5)] < 0.0f ? -0.5f : 0.5f));
    else if (!strncmp(what, "shown", 5) && atoi(what + 5) < NPARAM)   /* shownN: setting N as the screen shows it
                                                                          * (its number, its gauge: pv_shown) */
        *got = (int)pv_shown(atoi(what + 5));
    else if (!strcmp(what, "framegone")) /* ANCHOR's frame: its places erased (of AF_N), as the screen has it */
        *got = af.on ? af.gone : -1;
#if defined PLAT_HOST && !defined PLAT_WEB
    else if (!strcmp(what, "framelo") || !strcmp(what, "framehi")) {   /* its breath: its dimmest and brightest (of 32)
                                                                        * since asked */
        if (what[5] == 'l') {
            *got = af_lo;
            af_lo = 255u;
        } else {
            *got = af_hi;
            af_hi = 0u;
        }
    }
    else if (!strcmp(what, "drawnhome")) /* the frames since drawnmark that showed the play page (HOME's SOUND, or a
                                          * value large over the river) */
        *got = (int)ui_seen.home;
    else if (!strcmp(what, "drawnflips"))   /* the times the page shown changed since drawnmark */
        *got = (int)ui_seen.flips;
    else if (!strcmp(what, "drawnstale"))   /* the frames since drawnmark whose middle band still showed another page
                                             * than its header and footer */
        *got = (int)ui_seen.stale;
#endif
#ifndef PLAT_WEB
    else if (!strcmp(what, "framepx")) { /* its pixels on the screen (of AF_N x AF_W, 3776): the frame's places that read
                                          * violet, at any breath (blue and red over green: no band's colour, no
                                          * night's) */
        int32_t x, y, n = 0;
        for (y = 0; y < 240; y++)
            for (x = 0; x < 240; x++) {
                uint32_t c = swap16(fb[y * 240 + x]), r = c >> 11, g = c >> 5 & 63u, b = c & 31u;
                n += af_place(x, y) >= 0 && 2u * b > g + 6u && 2u * r > g;
            }
        *got = n;
    }
    else if (!strcmp(what, "pf_lost"))   /* PERFORM's LIVE: the ops dropped (its queue full), the bytes queued, on */
        *got = (int)lv.lost;
    else if (!strcmp(what, "pf_queued"))
        *got = lv.n;
    else if (!strcmp(what, "pf_live"))
        *got = lv_on;
#endif
    else if (!strcmp(what, "overlay"))   /* a value shown large over the strip */
        *got = overlay_on();
    else if (!strcmp(what, "compare"))   /* EDIT held: the scene as saved sounding */
        *got = st.compare;
    else if (!strncmp(what, "used", 4))  /* usedN: slot N has a saved scene */
        *got = scn.used >> (atoi(what + 4) % NSCENES) & 1u;
    else if (!strcmp(what, "logo"))      /* the logo's screen up */
        *got = fm.logo;
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
    else if (!strcmp(what, "headdark"))  /* GLO > SCREEN's HEADER: 1 DARK, 0 INVERT; headbyte: the settings' record's */
        *got = hdr.dark;
    else if (!strcmp(what, "battpct"))   /* the header's battery: its level drawn (-1 none), its colour (0 green, 1
                                          * yellow, 2 red), the millivolts smoothed */
        *got = batt.pct;
    else if (!strcmp(what, "battzone"))
        *got = batt.zone;
    else if (!strcmp(what, "battmv"))
        *got = batt.mv16 / 16;
#ifndef PLAT_WEB
    else if (!strcmp(what, "pfkeys"))       /* PAGE's keys as the page was last told (0 DRONE, 1 GLINT, 2 BASS, 3 LEAD) */
        *got = pf.keys;
    else if (!strcmp(what, "pfframelen"))   /* the last FRAME a PERFORM page got: its bytes, its cmd first */
        *got = (int)pf_frame_n;
    else if (!strncmp(what, "pfframe", 7))   /* pfframeN: its byte N (0 its cmd, 6; 53 .. 64 the LEAD's voices, 65 its
                                              * meter, card #144), -1 past it */
        *got = (uint32_t)atoi(what + 7) < pf_frame_n ? (int)pf_frame_last[atoi(what + 7)] : -1;
    else if (!strcmp(what, "pfbatt"))    /* the last BATTERY a PERFORM page got: its level (127 none); pfbattn: how many */
        *got = pf_batt_last[5];
    else if (!strcmp(what, "pfbattn"))
        *got = (int)pf_batt_n;
#endif
    else if (!strcmp(what, "headbyte"))
        *got = set.head;
    else if (!strcmp(what, "a4"))        /* GLO > RECORD's TUNE A4, Hz; a4byte: the settings' record's (less 440) */
        *got = a4.hz;
    else if (!strcmp(what, "a4byte"))
        *got = set.a4;
    else if (!strcmp(what, "welcome"))   /* Learn's welcome on the screen */
        *got = welcome.on;
#ifndef PLAT_WEB
    else if (!strcmp(what, "learn"))     /* Learn's START: 0 none, 1 waiting (a backup, the fading), 2 in, VALUES going
                                          * out; learnn: its pairs kept (the rest skipped) */
        *got = ls.due;
    else if (!strcmp(what, "learnn"))
        *got = ls.n;
#endif
#ifndef PLAT_WEB
    else if (!strcmp(what, "pitchhz"))   /* the output's pitch since "pitch HZ", Hz x 100 (pz); pitchcents: over the one
                                          * "pitchref" kept, cents x 10 */
        *got = (int)(pz_pitch() * 100.0 + 0.5);
    else if (!strcmp(what, "pitchcents"))
        *got = pz_pitch() > 0.0 && pz.ref > 0.0 ? (int)lround(12000.0 * log2(pz_pitch() / pz.ref)) : 0;
    else if (!strcmp(what, "edgedb"))    /* the blocks' edges over their middles since "edges", dB x 10 (eg) */
        *got = eg.edge > 0.0 && eg.mid > 0.0 ? (int)lround(100.0 * log10(eg.edge / eg.mid)) : 0;
#endif
    else if (!strncmp(what, "knob", 4))  /* knobN: the setting under KN as it is now */
        *got = page_knob(fm.page, (uint32_t)(atoi(what + 4) - 1) & 3u);
    else if (!strcmp(what, "keymid"))    /* the keys at the mid level */
        *got = (int)keymid_now;
    else if (!strcmp(what, "arpled") || !strcmp(what, "seqled")) {   /* as recled: 0 off, 1 glow, 2 mid, 3 lit */
        uint32_t b = what[0] == 'a' ? B_ARP : B_SEQ;
        *got = (leds_now >> b & 1u) ? 3 : (midleds_now >> b & 1u) ? 2 : (glowleds_now >> b & 1u) ? 1 : 0;
    } else if (!strcmp(what, "arpflashes") || !strcmp(what, "seqflashes")) {   /* the flashes since last asked */
        *got = (int)flashes[what[0] == 's'];
        flashes[what[0] == 's'] = 0;
    } else if (!strcmp(what, "counting"))   /* COUNT-IN under way */
        *got = cnt.run;
    else if (!strcmp(what, "ringn"))     /* GLINT's ring of four: how many */
        *got = rung.n;
    else if (!strcmp(what, "grid_on"))   /* the grid of names up; gridv: the choice in its list; gridp: its setting */
        *got = ng.on;
    else if (!strcmp(what, "gridv"))
        *got = ng.v;
    else if (!strcmp(what, "gridp"))
        *got = ng.p;
    else if (!strcmp(what, "editmap"))   /* EDIT's keys up */
        *got = fm.edit_map;
    else if (!strncmp(what, "upn", 3))   /* upnT: track T's presets of the player's; upcurT: the one taken last, -1 */
        *got = (int)up_n((uint32_t)atoi(what + 3) % (3u + FRS_LEAD));
    else if (!strncmp(what, "upcur", 5))
        *got = up_cur[(uint32_t)atoi(what + 5) % (3u + FRS_LEAD)];
    else if (!strcmp(what, "svinto"))    /* the flow's INTO, WHAT, LOAD */
        *got = sv.into;
    else if (!strcmp(what, "svwhat"))
        *got = sv.what;
    else if (!strcmp(what, "svload"))
        *got = sv.load;
    else if (!strcmp(what, "nameflow"))  /* the name up is the flow's */
        *got = nm.on && nm.flow;
    else if (!strcmp(what, "size_upresets"))
        *got = (int)sizeof(upresets_t);
    else if (!strcmp(what, "bseqpos"))
        *got = bq.pos;
    else if (!strcmp(what, "bseqsteps")) /* the NOTEs it has played */
        *got = (int)bq.steps;
    else if (!strcmp(what, "bseqsame"))  /* the bass's sequence as bseqkeep kept it: its steps, SEQ STEPS, STEP LENGTH */
        *got = !memcmp(bq_kept, bnote, SC_BSEQ) && bq_kept[SC_BSEQ] == st.v[P_BQ_STEPS] &&
               bq_kept[SC_BSEQ + 1u] == st.v[P_BQ_LEN];
    else if (!strcmp(what, "dqrun"))     /* the drone's sequence playing; dqpos its step (-1 none); dqsnd the notes it
                                          * sounds now; dqnN: step N's notes (127: a tie); dqslotS: slot S's scene's, every
                                          * step's */
        *got = dq.run;
    else if (!strcmp(what, "dqpos"))
        *got = dq.run ? dq.pos : -1;
    else if (!strcmp(what, "dqsnd"))
        *got = dq.nsnd;
    else if (!strncmp(what, "dqn", 3) || !strncmp(what, "dqslot", 6)) {
        const uint8_t *d = what[2] == 'n' ? dnote[atoi(what + 3) % SC_DSEQ] : &scn.s[atoi(what + 6) % NSCENES].dnote[0][0];
        uint32_t j, m = what[2] == 'n' ? DQ_NOTES : SC_DSEQ * DQ_NOTES;
        *got = what[2] == 'n' && d[0] == DQ_TIE ? 127 : 0;
        for (j = 0; j < m && *got != 127; j++)
            *got += d[j] != 0u && d[j] != DQ_TIE;
    } else if (!strncmp(what, "bseq", 4))  /* bseqN: step N's note (0 CUT, 127 HOLD), baccN its ACCENT; blenN: the LENGTH of
                                          * the NOTE at N, in steps (0: no NOTE there) */
        *got = bnote[atoi(what + 4) % SC_BSEQ] & 127u;
    else if (!strncmp(what, "bacc", 4))
        *got = bnote[atoi(what + 4) % SC_BSEQ] >> 7;
    else if (!strncmp(what, "blen", 4)) {
        uint32_t c = (uint32_t)atoi(what + 4) % SC_BSEQ;
        *got = step_start(bnote, c) == (int32_t)c ? (int)step_len(bnote, c, st.v[P_BQ_STEPS] + 1u) : 0;
    }
    else if (!strncmp(what, "lfoon", 5)) /* lfoonN: LFO N (0, 1) running */
        *got = lfo[(uint32_t)(what[5] - '0') % NLFO].on;
    else if (!strncmp(what, "lfopeak", 7)) {   /* lfopeakN: its largest offset since last asked, x 100 */
        uint32_t i = (uint32_t)(what[7] - '0') % NLFO;
        *got = (int)(lfo[i].peak * 100.0f + 0.5f);
        lfo[i].peak = 0.0f;
    }
    else if (!strcmp(what, "helppage"))
        *got = fm.help_page;
    else if (!strcmp(what, "page"))
        *got = fm.page;
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
    else if (!strncmp(what, "scnchar", 7)) {   /* scncharN: the current scene's name, its character N (0: none) */
        char t[SC_LONG + 1];
        memset(t, 0, sizeof t);
        if (fm.scene < NSCENES && !(scn.used >> fm.scene & 1u))   /* (an empty slot: what its record holds, as 3.0) */
            memcpy(t, scn.s[fm.scene].name, SC_NAME);
        else
            scene_name(fm.scene, t);
        *got = (uint8_t)t[atoi(what + 7) % SC_LONG];
    } else if (!strcmp(what, "namelen"))   /* the name editor's places (32 a scene's, 16 a preset's) */
        *got = nm.len;
    else if (!strcmp(what, "namesstored"))   /* the store's OBJ_NAMES holds something: a name's rest, an AMOUNT, SWELL */
        *got = names_stored;
    else if (!strncmp(what, "scnamt", 6))   /* scnamtN: the current slot's scene as saved, its AMOUNT N (0 .. 11);
                                             * scnswell: its SWELL */
        *got = fm.scene < NSCENES ? scn.s[fm.scene].lfo_amt[atoi(what + 6) % 12] : -1;
    else if (!strcmp(what, "scnswell"))
        *got = fm.scene < NSCENES ? scn.s[fm.scene].swell : -1;
    else if (!strcmp(what, "sayamt"))    /* the header's AMOUNT gauge (SEL + K): its slot + 1, while it is up; 0 none */
        *got = plat_ms() < say.msg_until ? say.amt : 0;
    else if (!strcmp(what, "rolling"))   /* the header's name rolling (ui.c roll), rolloff: how far it is now, rolldist: its
                                          * way, rollw: its room, rolldraws and rollpx: its windows drawn since asked */
        *got = roll.on;
    else if (!strcmp(what, "rolloff"))
        *got = roll.off;
    else if (!strcmp(what, "rolldist"))
        *got = roll.dist;
    else if (!strcmp(what, "rollw"))
        *got = roll.w;
    else if (!strcmp(what, "rolldraws")) {
        *got = (int)roll.draws;
        roll.draws = 0;
    } else if (!strcmp(what, "rollpx")) {
        *got = (int)roll.px;
        roll.px = 0;
    } else if (!strncmp(what, "lroll", 5)) {   /* SCENES' (LOAD's list) long name K1 points at, rolling (ui.c lroll):
                                                * lrolling, lrolloff, lrolldist, lrollw, lrolldraws, lrollpx as the
                                                * header's; lrollfont 1 while it is FONT_B, the list's own */
        const char *k = what + 5;
        *got = !strcmp(k, "ing") ? lroll.on : !strcmp(k, "off") ? lroll.off : !strcmp(k, "dist") ? lroll.dist
             : !strcmp(k, "w") ? lroll.w : !strcmp(k, "draws") ? (int)lroll.draws : !strcmp(k, "px") ? (int)lroll.px
             : !strcmp(k, "font") ? lroll.f == &FONT_B : -1;
        if (!strcmp(k, "draws"))
            lroll.draws = 0;
        if (!strcmp(k, "px"))
            lroll.px = 0;
    }
    else if (!strcmp(what, "pick"))      /* SCENES: the slot K1 points at (16: NEW SHARD) */
        *got = fm.pick;
    else if (!strcmp(what, "stacktag"))  /* the STACK hint at the foot, as drawn last: 0 none, 1 OFF, 2 ON */
        *got = ui.stack;
    else if (!strcmp(what, "semistag"))  /* LOFI TAPE II's tones between the reels, as drawn last: the half-tones
                                          * they stand for (0 none) */
        *got = ui.semis;
    else if (!strcmp(what, "toneword"))  /* and their word: 0 none, 1 TONE, 2 TONES */
        *got = ui.tonew;
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
    else if (!strcmp(what, "size_names"))   /* OBJ_NAMES' */
        *got = (int)NMS_BYTES;
    else if (!strcmp(what, "at_scene"))     /* the settings' scene byte: where */
        *got = (int)offsetof(settings_t, scene);
#ifndef PLAT_WEB
    else if (!strncmp(what, "saver", 5))   /* the screensaver (saver_expect) */
        *got = saver_expect(what + 5);
#endif
    else if (!strcmp(what, "popup"))     /* a pop-up up: 1 a warning, 2 done */
        *got = pop.on ? 1 + pop.kind : 0;
    else if (!strcmp(what, "octave"))
        *got = fm.octave;
    else if (!strcmp(what, "latch"))
        *got = !st.v[P_KEYS];
    else if (!strcmp(what, "freeze"))
        *got = st.v[P_FREEZE];
    else if (!strcmp(what, "dirty"))
        *got = st.dirty;
    else if (!strcmp(what, "scene"))
        *got = fm.scene;
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
     * in OBJ_BSEQ's (bseqs_t: after its magic, each scene's GLINT sequence, bnote ... barp). bnote: the bass's
     * SC_BSEQ steps (a step a byte: bseq's cut, hold, else a note, its accent in seq's bit); bq: PLAYS, SEQ LENGTH,
     * TRANSPOSE; bq_len: RATE (bseq's ticks' index); ba: its arpeggio's MODE, RANGE, CHANCE; gate; nbarp and barp: its
     * arpeggio's notes (as narp and arp); name, name_len: its first sixteen characters (NUL-padded), name_more,
     * name_more_len: the rest, to name_long (only after a first sixteen all used; FRS|'s); lfo_amt: the twelve LFO TARGETs'
     * AMOUNTs (LFO 1's TARGET 1 .. LFO 3's TARGET 4: 0 .. 200, 100 is 0 %, 200 +100 %); swell: SWELL */
    fprintf(f, "\"scene_t\":{\"size\":%u,\"name\":%u,\"name_len\":%u,\"name_more\":%u,\"name_more_len\":%u,"
               "\"name_long\":%u,\"lfo_amt\":%u,\"swell\":%u,\"v\":%u,\"nch\":%u,\"narp\":%u,\"ch\":%u,\"arp\":%u,\"bass\":%u,"
               "\"seq\":%u,\"bnote\":%u,\"bq\":%u,\"tempo\":%u,\"bq_len\":%u,\"ba\":%u,\"gate\":%u,\"nbarp\":%u,"
               "\"barp\":%u,\"bs\":%u,\"bs_space\":%u,\"gl_vib\":%u,\"dq\":%u,\"dnote\":%u,\"tp\":%u,\"rec\":%u},\n",
            (unsigned)sizeof(scene_t), (unsigned)offsetof(scene_t, name), SC_NAME, (unsigned)offsetof(scene_t, name_more),
            SC_MORE, SC_LONG, (unsigned)offsetof(scene_t, lfo_amt), (unsigned)offsetof(scene_t, swell),
            (unsigned)offsetof(scene_t, v), (unsigned)offsetof(scene_t, nch), (unsigned)offsetof(scene_t, narp), (unsigned)offsetof(scene_t, ch),
            (unsigned)offsetof(scene_t, arp), (unsigned)offsetof(scene_t, bass), (unsigned)offsetof(scene_t, seq),
            (unsigned)offsetof(scene_t, bnote), (unsigned)offsetof(scene_t, bq_on), (unsigned)offsetof(scene_t, tempo),
            (unsigned)offsetof(scene_t, bq_len), (unsigned)offsetof(scene_t, ba_mode),
            (unsigned)offsetof(scene_t, bq_gate), (unsigned)offsetof(scene_t, nbarp), (unsigned)offsetof(scene_t, barp),
            (unsigned)offsetof(scene_t, bs_weight), (unsigned)offsetof(scene_t, bs_space), (unsigned)offsetof(scene_t, gl_vib),
            (unsigned)offsetof(scene_t, dq_on), (unsigned)offsetof(scene_t, dnote), (unsigned)offsetof(scene_t, tp_flutter),
            (unsigned)SC_REC);
    fprintf(f, "\"scenes_t\":{\"size\":%u,\"magic\":%u,\"used\":%u,\"s\":%u,\"rec\":%u},\n", (unsigned)sizeof(scenes_t),
            (unsigned)offsetof(scenes_t, magic), (unsigned)offsetof(scenes_t, used), (unsigned)offsetof(scenes_t, s),
            (unsigned)SCN_BYTES);
    fprintf(f, "\"bseqs_t\":{\"size\":%u,\"magic\":0,\"s\":4,\"scene\":%u},\n", (unsigned)BSQ_BYTES, (unsigned)BSQ_SCENE);
    /* OBJ_DSEQ (3.0), the scenes' drone sequences: after its magic, each scene's DSQ_SCENE bytes, its NDQ settings as
     * scene_t has them from dq (ON, RATE, SEQ LENGTH - 1, GATE, CHANCE, TRANSPOSE), then its SC_DSEQ x DQ_NOTES notes
     * (scene_t's dnote, step by step) seven bits each, note j at bits 7j .. 7j + 6 of the bytes read as one little-endian
     * stream (dsq_pack), then LOFI TAPE II's four (scene_t's tp_flutter, tp_tone, tp_noise, tp_dropout) at "tape" (3.0's
     * FRDQ, "old": without them, DSQ_SCENE_FRDQ a scene); absent (an earlier FRASIL's store or backup): every scene's drone
     * sequence none (dseq.none), the four as the tape was (tape.none: TAPE_NONE, TAPE SPEED x1.00, TONE 0; not their
     * defaults) */
    fprintf(f, "\"dseqs_t\":{\"size\":%u,\"magic\":0,\"s\":4,\"scene\":%u,\"set\":%u,\"bits\":7,\"tape\":%u,"
               "\"old\":{\"magic\":", (unsigned)DSQ_BYTES, (unsigned)DSQ_SCENE, (unsigned)DSQ_SET, (unsigned)DSQ_SCENE_FRDQ);
    js_magic(f, DSQ_FRDQ);
    fprintf(f, ",\"size\":%u,\"scene\":%u}},\n", (unsigned)DSQ_BYTES_FRDQ, (unsigned)DSQ_SCENE_FRDQ);
    fprintf(f, "\"tape\":{\"none\":[%u,%u,%u,%u]},\n", TAPE_NONE[0], TAPE_NONE[1], TAPE_NONE[2], TAPE_NONE[3]);
    /* OBJ_NAMES (3.0.1; FRNO 3.1), what of each scene the records above have no room for: after its magic, each slot's
     * "scene" bytes: at "sum" the sum of its record in OBJ_SCENES as written with it (FNV-1a over its "rec" bytes, 32 bits
     * little-endian: rec_sum), then scene_t's name_more, lfo_amt and swell as they are (at "more"). Read onto the slot
     * whose record has that sum (its own first, then any other's: a scene moved); a slot none has: more.none. FRNM
     * ("old"): the sum, then name_more alone (the AMOUNTs, SWELL: more.none). FRNN ("pct", 3.0.2-b5's): this layout,
     * the AMOUNTs in percent, each made |v - 100| (one past 200: more.none's). Absent (an earlier FRASIL's store or
     * backup): every slot's more.none. more.none, a scene's (every shard before FRS|, made over) and the settings' (every
     * layout before FRSR): name_more all 0, each lfo_amt "lfo_amt", swell "swell" */
    fprintf(f, "\"names_t\":{\"size\":%u,\"magic\":0,\"s\":4,\"scene\":%u,\"sum\":0,\"more\":4,\"bytes\":%u,"
               "\"pct\":{\"magic\":", (unsigned)NMS_BYTES, (unsigned)NMS_SCENE, (unsigned)NMS_MORE);
    js_magic(f, NMS_FRNN);
    fprintf(f, "},\"old\":{\"magic\":");
    js_magic(f, NMS_FRNM);
    fprintf(f, ",\"size\":%u,\"scene\":%u,\"bytes\":%u}},\n", (unsigned)NMS_BYTES_FRNM, (unsigned)NMS_SCENE_FRNM, SC_MORE);
    fprintf(f, "\"more\":{\"none\":{\"lfo_amt\":%u,\"swell\":%u}},\n", PD[P_L1_AMT1].def, PD[P_SWELL].def);
    fprintf(f, "\"settings_t\":{\"size\":%u,\"magic\":%u,\"v\":%u,\"octave\":%u,\"keys\":%u,\"channel\":%u,"
               "\"scene\":%u,\"tempo\":%u,\"ch_glint\":%u,\"ch_bass\":%u,\"seq\":%u,\"metro_off\":%u,\"speaker_off\":%u,"
               "\"load\":%u,\"learn_cc\":%u,\"learn_p\":%u,\"bnote\":%u,\"bq_on\":%u,\"bq_steps\":%u,"
               "\"bq_trans\":%u,\"anim\":%u,\"clock\":%u,\"bq_len\":%u,\"ba\":%u,\"gate\":%u,\"bs\":%u,\"bs_space\":%u,\"gl_vib\":%u,"
               "\"ky\":%u,\"metro\":%u,\"metro_on\":%u,\"count_in\":%u,\"arp_snap\":%u,\"saver_off\":%u,\"tp\":%u,"
               "\"lfo_amt\":%u,\"swell\":%u},\n",
            (unsigned)sizeof(settings_t), (unsigned)offsetof(settings_t, magic), (unsigned)offsetof(settings_t, v),
            (unsigned)offsetof(settings_t, octave), (unsigned)offsetof(settings_t, keys),
            (unsigned)offsetof(settings_t, channel), (unsigned)offsetof(settings_t, scene),
            (unsigned)offsetof(settings_t, tempo), (unsigned)offsetof(settings_t, ch_glint),
            (unsigned)offsetof(settings_t, ch_bass), (unsigned)offsetof(settings_t, seq),
            (unsigned)offsetof(settings_t, metro_off), (unsigned)offsetof(settings_t, speaker_off),
            (unsigned)offsetof(settings_t, load), (unsigned)offsetof(settings_t, learn_cc),
            (unsigned)offsetof(settings_t, learn_p), (unsigned)offsetof(settings_t, bnote),
            (unsigned)offsetof(settings_t, bq_on), (unsigned)offsetof(settings_t, bq_steps),
            (unsigned)offsetof(settings_t, bq_trans), (unsigned)offsetof(settings_t, anim),
            (unsigned)offsetof(settings_t, clock), (unsigned)offsetof(settings_t, bq_len),
            (unsigned)offsetof(settings_t, ba_mode), (unsigned)offsetof(settings_t, bq_gate),
            (unsigned)offsetof(settings_t, bs_weight), (unsigned)offsetof(settings_t, bs_space),
            (unsigned)offsetof(settings_t, gl_vib), (unsigned)offsetof(settings_t, ky),
            (unsigned)offsetof(settings_t, metro), (unsigned)offsetof(settings_t, metro_on),
            (unsigned)offsetof(settings_t, count_in), (unsigned)offsetof(settings_t, arp_snap),
            (unsigned)offsetof(settings_t, saver_off), (unsigned)offsetof(settings_t, tp_flutter),
            (unsigned)offsetof(settings_t, lfo_amt), (unsigned)offsetof(settings_t, swell));
    fprintf(f, "\"magic\":{\"scenes\":");
    js_magic(f, SCN_MAGIC);
    fprintf(f, ",\"bseqs\":");
    js_magic(f, BSQ_MAGIC);
    fprintf(f, ",\"shard\":");
    js_magic(f, SHARD_MAGIC);
    fprintf(f, ",\"dseqs\":");
    js_magic(f, DSQ_MAGIC);
    fprintf(f, ",\"upresets\":");
    js_magic(f, UPR_MAGIC);
    fprintf(f, ",\"names\":");
    js_magic(f, NMS_MAGIC);
    fprintf(f, ",\"settings\":");
    js_magic(f, SET_MAGIC);
    fprintf(f, ",\"backup\":");
    js_magic(f, APP_BACKUP_MAGIC);
    fprintf(f, "},\n\"objs\":{\"settings\":%d,\"panel\":%d,\"scenes\":%d,\"bseqs\":%d,\"dseqs\":%d,\"upresets\":%d,\"names\":%d",
            OBJ_SETTINGS, OBJ_PANEL, OBJ_SCENES, OBJ_BSEQ, OBJ_DSEQ, OBJ_UPRE, OBJ_NAMES);
#if FRS_LEAD
    fprintf(f, ",\"lead\":%d", OBJ_LEAD);
#endif
    fprintf(f, "},\n\"seq\":{\"rest\":%u,\"tie\":%u,\"accent\":%u},\n", SQ_REST, SQ_TIE, SQ_ACC);
    /* OBJ_UPRE (3.0), the player's presets: after its magic NUP of the drone's, of GLINT's, of the bass's, each its bytes
     * (the engine's "FRp": "FRp", version, track, n, name of SC_NAME, n pairs of setting and value; its first byte 0: none);
     * absent (an earlier FRASIL's store or backup): none */
    fprintf(f, "\"upresets_t\":{\"size\":%u,\"magic\":0,\"slots\":%u,\"at\":[%u,%u,%u],\"bytes\":[%u,%u,%u]},\n",
            (unsigned)sizeof(upresets_t), NUP, (unsigned)offsetof(upresets_t, dr), (unsigned)offsetof(upresets_t, gl),
            (unsigned)offsetof(upresets_t, bs), (unsigned)sizeof up.dr[0], (unsigned)sizeof up.gl[0], (unsigned)sizeof up.bs[0]);
#if FRS_LEAD
    /* OBJ_LEAD (card #144), each scene's LEAD and the player's LEAD presets (ldp_*): "FRLD", its version, its slots, an
     * entry's bytes, its settings (from "first", P_LD_WAVE on); then each slot's entry (a byte, 1 stored with a LEAD,
     * else MOSS; its settings); at "presets", 'U', NUP, a preset's bytes, its settings, then each preset (a byte, 1
     * saved; its name, SC_NAME; its settings); absent (an earlier FRASIL's store or backup): every slot MOSS */
    fprintf(f, "\"lead_t\":{\"magic\":%u,\"version\":%u,\"head\":%u,\"slots\":%u,\"entry\":%u,\"settings\":%u,"
               "\"first\":%u,\"presets\":%u,\"preset\":%u,\"size\":%u},\n",
            (unsigned)LDP_MAGIC, LDP_VER, LDP_HEAD, NSCENES, (unsigned)LDP_ENTRY, (unsigned)NLDS, (unsigned)P_LD_WAVE,
            (unsigned)LDP_BYTES, (unsigned)LDU_ENTRY, (unsigned)LDP_ALL);
    /* a shard (magic.shard, card #152): scene_t's bytes, then at "lead" the LEAD's NLDS settings, P_LD_WAVE on */
    fprintf(f, "\"shard_t\":{\"size\":%u,\"lead\":%u},\n", (unsigned)(sizeof(scene_t) + NLDS), (unsigned)sizeof(scene_t));
#else
    fprintf(f, "\"shard_t\":{\"size\":%u},\n", (unsigned)sizeof(scene_t));
#endif
    /* the drone's sequence (scene_t's dq, dnote): SC_DSEQ steps of DQ_NOTES bytes, each step its notes low to high (1 ..
     * 126, MIDI's, TRANSPOSE off them), 0 after the last, all 0 a rest, "tie" first (the rest 0) a tie; RATE's ticks the
     * bass's (bseq.ticks); "none": its NDQ settings in a scene that has none (every layout before FRSz, an absent
     * OBJ_DSEQ), its steps all 0 */
    fprintf(f, "\"dseq\":{\"steps\":%u,\"notes\":%u,\"tie\":%u,\"none\":[0,%u,%u,%u,%u,%u]},\n", SC_DSEQ, DQ_NOTES, DQ_TIE,
            PD[P_DQ_RATE].def, PD[P_DQ_LEN].def, PD[P_DQ_GATE].def, PD[P_DQ_CHANCE].def, PD[P_DQ_TRANS].def);
    fprintf(f, "\"bseq\":{\"cut\":%u,\"hold\":%u,\"ticks\":[", SQ_REST, SQ_TIE);   /* (a RATE's, MIDI clocks) */
    for (i = 0; i < NBQLEN; i++)
        fprintf(f, "%s%u", i ? "," : "", BQ_TICKS[i]);
    /* the bass's sequence as every layout before FRBV, FRSN and FRSu kept it (the old records and shards below, FRSM's
     * settings): at bnote its OLD steps' notes, then their lengths (old_ticks' indices), then SEQUENCE, SEQ STEPS (of
     * the sixteen), TRANSPOSE; no RATE. Made over as bseq_over does (the firmware's; the River's page has it in
     * web_scene_over) */
    fprintf(f, "],\"old\":%u,\"old_ticks\":[", OLD_BSEQ);
    for (i = 0; i < NBQOLD; i++)
        fprintf(f, "%s%u", i ? "," : "", BQ_OLD[i]);
    /* a shard of an earlier layout: its name and NV sounds, then the rest of its scene up to the bass's sequence (to
     * REC: GLINT's sequence in it), then its bass's sequence (SIZE - REC bytes, from bnote on, the old one's: bseq.old;
     * none in FRSh's: an empty one's, TEMPO at its default); SCATTER where GLINT's RELEASE is, made over (scatter_gone):
     * FRS| (this layout, "pct": its twelve AMOUNTs in percent, each made |v - 100|, one past 200 more.none's),
     * FRS{ (this layout to tp_dropout: name_more, lfo_amt, swell more.none: same), FRSz (to dnote's end: and LOFI TAPE
     * II's four tape.none: same), FRSy (this layout to gl_vib: the drone's sequence none, dseq.none: same), FRSx (that but its last byte, AUTO VIB: 0, its bass kept: same), FRSw (and SPACE before it, the same), FRSu (grid: its bass's sequence on
     * the one grid already, this layout's to bq_len), FRSt, FRSr, FRSp, FRSn, FRSm,
     * FRSk, FRSi, then FRSh. Every one, as every old_bseqs and old_settings_over: PLAYS' 1 is 2 here (SEQUENCE on was 1),
     * the bass's arpeggio none, its settings and GATE their defaults; and every one: the drone's sequence none (dseq.none) */
    fprintf(f, "]},\n\"old_shards\":[{\"magic\":");
#if FRS_LEAD
    js_magic(f, SHARD_FRSBRACE);         /* (3.1 .. 3.2's: the scene alone, the LEAD after it MOSS) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0,\"grid\":1,\"same\":1},{\"magic\":",
            (unsigned)sizeof(scene_t), (unsigned)NSOUND, (unsigned)(SC_NAME + NSOUND + SC_TAIL));
#endif
    js_magic(f, SHARD_FRSBAR);           /* (3.0.2-b5's: this layout, its AMOUNTs in percent, pct: |v - 100| each) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0,\"grid\":1,\"same\":1,\"pct\":1},{\"magic\":",
            (unsigned)sizeof(scene_t), (unsigned)NSOUND, (unsigned)(SC_NAME + NSOUND + SC_TAIL));
    js_magic(f, SHARD_FRSTP);            /* (3.0.2's: this layout to tp_dropout; name_more, lfo_amt, swell: more.none) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0,\"grid\":1,\"same\":1},{\"magic\":",
            (unsigned)offsetof(scene_t, name_more), (unsigned)NSOUND, (unsigned)(SC_NAME + NSOUND + SC_TAIL));
    js_magic(f, SHARD_FRSZ);             /* (3.0's: this layout to dnote's end; LOFI TAPE II's four as the tape was, tape.none) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0,\"grid\":1,\"same\":1},{\"magic\":", FRSZ_SCENE,
            (unsigned)NSOUND, (unsigned)(SC_NAME + NSOUND + SC_TAIL));
    js_magic(f, SHARD_FRSY);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0,\"grid\":1,\"same\":1},{\"magic\":", FRSY_SCENE,
            (unsigned)NSOUND, (unsigned)(SC_NAME + NSOUND + SC_TAIL));
    js_magic(f, SHARD_FRSX);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0,\"grid\":1,\"same\":1},{\"magic\":", FRSX_SCENE,
            (unsigned)NSOUND, (unsigned)(SC_NAME + NSOUND + SC_TAIL));
    js_magic(f, SHARD_FRSW);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0,\"grid\":1,\"same\":1},{\"magic\":", FRSW_SCENE,
            (unsigned)NSOUND, (unsigned)(SC_NAME + NSOUND + SC_TAIL));
    js_magic(f, SHARD_FRSU);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0,\"grid\":1},{\"magic\":", (unsigned)FRSU_SCENE, (unsigned)NSOUND,
            (unsigned)(SC_NAME + NSOUND + SC_TAIL));
    js_magic(f, SHARD_FRST);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0},{\"magic\":",
            (unsigned)(SC_NAME + NSOUND + SC_TAIL + FRBT_SCENE), (unsigned)NSOUND, (unsigned)(SC_NAME + NSOUND + SC_TAIL));
    js_magic(f, SHARD_FRSR);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"rec\":%u,\"scatter\":0},{\"magic\":",
            (unsigned)(SC_NAME + FRSQ_NV + SC_TAIL + FRBT_SCENE), FRSQ_NV, (unsigned)(SC_NAME + FRSQ_NV + SC_TAIL));
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
    js_magic(f, BSQ_FRBV);               /* (2.7's first test builds': a scene's FRBV_SCENE bytes, to RATE, on the one grid) */
    fprintf(f, ",\"scene\":%u,\"at\":0,\"grid\":1},{\"magic\":", (unsigned)FRBV_SCENE);
    js_magic(f, BSQ_FRBU);               /* (2.6's: a scene's FRBU_SCENE bytes, GLINT's sequence first, to TEMPO) */
    fprintf(f, ",\"scene\":%u,\"at\":0},{\"magic\":", (unsigned)FRBU_SCENE);
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
     * FRSH, FRSG. Every one: the LFOs' AMOUNTs and SWELL more.none but where "pct" (FRSR, 3.0.2-b5's: this layout, its
     * AMOUNTs in percent, each made |v - 100|, one past 200 more.none's; its SWELL kept); LOFI TAPE II's four tape.none
     * but where "tape" (FRSQ, FRSR have them) */
    fprintf(f, ",\n\"old_settings_over\":[{\"magic\":");
    js_magic(f, SET_FRSR);               /* (3.0.2-b5's: this layout whole, the AMOUNTs in percent) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":0,\"speaker\":1,\"anim\":1,\"grid\":1,\"keys\":1,"
               "\"kept\":1,\"tape\":1,\"pct\":1},{\"magic\":", (unsigned)sizeof(settings_t), (unsigned)NSOUND,
            (unsigned)sizeof(settings_t));
    js_magic(f, SET_FRSQ);              /* (3.0.2's: this layout to tp_dropout, its KEYS kept; the AMOUNTs, SWELL: more.none) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":0,\"speaker\":1,\"anim\":1,\"grid\":1,\"keys\":1,"
               "\"kept\":1,\"tape\":1},{\"magic\":", FRSQ_BYTES, (unsigned)NSOUND, FRSQ_BYTES);
    js_magic(f, SET_FRSP);               /* (3.0's before LOFI TAPE II: this layout to saver_off, its KEYS kept; the four at their
                                          * defaults) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":0,\"speaker\":1,\"anim\":1,\"grid\":1,\"keys\":1,"
               "\"kept\":1},{\"magic\":", FRSP_BYTES, (unsigned)NSOUND, FRSP_BYTES);
    js_magic(f, SET_FRSO);               /* (3.0's before KEYS: this layout to gl_vib, then KEYS as at the start, its KEY MODE
                                          * read in as the drone's and the bass's LATCH) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":0,\"speaker\":1,\"anim\":1,\"grid\":1,\"keys\":1},{\"magic\":",
            (unsigned)FRSO_BYTES, (unsigned)NSOUND, (unsigned)FRSO_BYTES);
    js_magic(f, SET_FRSN);               /* (2.7's first test builds': this layout to bq_len) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":0,\"speaker\":1,\"anim\":1,\"grid\":1},{\"magic\":",
            FRSN_BYTES, (unsigned)NSOUND, (unsigned)offsetof(settings_t, ba_mode));
    js_magic(f, SET_FRSM);               /* (2.6's: this layout, the bass's sequence the old one's, RATE none) */
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":0,\"speaker\":1,\"anim\":1},{\"magic\":",
            FRSN_BYTES, (unsigned)NSOUND, (unsigned)offsetof(settings_t, bq_len));
    js_magic(f, SET_FRSL);
    fprintf(f, ",\"size\":%u,\"nv\":%u,\"end\":%u,\"scatter\":0,\"speaker\":1,\"anim\":1},{\"magic\":", FRSL_BYTES,
            FRSQ_NV, FRSL_BYTES);
            /* (anim: its ANIMATIONS and CLOCK bytes theirs already; the others', a padding byte or none) */
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
    ID(P_BQ_STEP), ID(P_BQ_NOTE), ID(P_BQ_CLEAR), ID(P_BQ_REC), ID(P_BQ_NLEN), ID(P_EQ_PRESET), ID(P_EQ_LOW), ID(P_EQ_HIGHF), ID(P_FL_TYPE);
    ID(P_FL_CUT), ID(P_FL_RESO), ID(P_FL_TRACKS), ID(P_TP_ON), ID(P_TP_SAT), ID(P_TP_WOBBLE), ID(P_TP_TRACKS), ID(PG_FILTER);
    ID(PG_TAPE), ID(PG_EQ), ID(PG_EQ2), ID(PG_ROUTING), ID(PG_SCREEN), ID(P_FL_PRE), ID(P_TP_PRE), ID(P_PUMP_CLOUD);
    ID(P_PN_DRONE), ID(P_PN_BASS), ID(P_PN_GLINT), ID(P_PN_CLOUD), ID(P_CH_SOFT), ID(PG_CHOEUR2), ID(PG_CHOEUR3);
    ID(P_FREEZE), ID(P_SWELL), ID(P_KB), ID(P_BA_MODE), ID(P_BA_RANGE), ID(P_BA_CHANCE), ID(P_BQ_GATE), ID(P_BQ_ACC);
    ID(P_BQ_NEW), ID(P_BQ_UNDO), ID(P_SQ_NLEN), ID(P_BS_WEIGHT), ID(P_BS_PUNCH), ID(P_BS_BITE), ID(P_BS_SPACE), ID(P_GL_VIB_AUTO), ID(P_DQ_ON), ID(P_DQ_RATE), ID(P_DQ_LEN), ID(P_DQ_GATE), ID(P_DQ_CHANCE), ID(P_DQ_TRANS), ID(P_DQ_REC), ID(P_DQ_CLEAR), ID(P_DQ_STEP), ID(P_DQ_NOTES), ID(P_DQ_NLEN), ID(P_DQ_UNDO), ID(P_GA_LATCH), ID(P_BA_LATCH), ID(P_KY_DR_HARM), ID(P_KY_GL_HARM), ID(P_KY_BS_HARM), ID(P_METRO), ID(P_COUNT_IN), ID(P_ARP_SNAP), ID(P_SAVER), ID(P_TP_FLUTTER), ID(P_TP_TONE), ID(P_TP_NOISE), ID(P_TP_DROPOUT), ID(PG_TAPE2), ID(P_L1_AMT1), ID(P_L1_AMT2), ID(P_L1_AMT3), ID(P_L1_AMT4), ID(P_L2_AMT1), ID(P_L2_AMT2), ID(P_L2_AMT3), ID(P_L2_AMT4), ID(P_L3_AMT1), ID(P_L3_AMT2), ID(P_L3_AMT3), ID(P_L3_AMT4), ID(PG_LFO1T), ID(PG_LFO2T), ID(PG_LFO3T), ID(PG_BARP), ID(PG_BSEQ3), ID(PG_BDRIVE), ID(PG_CSEND), ID(P_SQ_REC), ID(PG_ARP2), ID(PG_BARP2), ID(PG_DSEQ), ID(PG_DSEQ2), ID(PG_DSEQ3), ID(PG_NOARP), ID(PG_KEYS), ID(PG_RECORD), ID(PG_SAVE), ID(PG_SAVEAS);
#if FRS_LEAD                             /* (the LEAD's, card #152: the River's editor, Learn, Perform by name) */
    ID(P_LD_WAVE), ID(P_LD_CUT), ID(P_LD_RESO), ID(P_LD_DETUNE), ID(P_LD_GLIDE), ID(P_LD_FENV), ID(P_LD_VIB), ID(P_LD_OCTAVE),
    ID(P_LD_ATTACK), ID(P_LD_DECAY), ID(P_LD_SUSTAIN), ID(P_LD_RELEASE), ID(P_LD_DTIME), ID(P_LD_DFB), ID(P_LD_DMIX),
    ID(P_LD_SPACE), ID(P_MX_LEAD), ID(P_LD_PRESET), ID(P_LD_MODE), ID(P_LD_BREATH), ID(P_LD_DRIVE), ID(P_LD_FATTACK),
    ID(P_LD_FDECAY), ID(P_LD_FSUSTAIN), ID(P_LD_FRELEASE), ID(PG_LEAD), ID(PG_LEAD2), ID(PG_LEAD3), ID(PG_LDELAY),
    ID(PG_LADSR), ID(PG_LFENV), ID(PG_LNOARP), ID(PG_LNOSEQ), ID(PG_MIXER), ID(PG_NUAGE3), ID(T_ANY);
#endif
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
        fprintf(f, ",\"button\":\"%s\",\"track\":%u,\"p\":[", BUTTON[PAGES[i].button % NB], PAGES[i].track);
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

/* a bass's sequence as the layouts before 2.7's kept it, from what it is now (the tests' earlier records): sixteen
 * steps of their own lengths, each stretch (a NOTE on through its HOLDs; a silence) in as few of them as BQ_OLD's
 * lengths make it, the longest first, as 2.6 wrote a take; at o its notes, then their lengths, SEQUENCE (as they kept
 * it: on 1), SEQ STEPS (o[SC_BSEQ + 1]). More than sixteen: an empty one (0) */
static int bseq_old(const uint8_t *b, uint32_t on, uint32_t steps, uint32_t len, uint8_t *o)
{
    uint32_t n = steps + 1u, u = BQ_TICKS[len % NBQLEN] / 6u, i, e, c, k, m = 0;
    memset(o, 0, SC_BSEQ);
    o[SC_BSEQ] = on == SQT_SEQ;
    for (i = 0; i < n; i = e) {
        uint8_t h = b[i] & 127u, f = h == SQ_REST ? SQ_REST : SQ_TIE;
        for (e = i + 1u; e < n && (b[e] & 127u) == f; e++)
            ;
        for (c = (e - i) * u; c; c -= BQ_OLD[k] / 6u, m++) {
            for (k = NBQOLD - 1u; BQ_OLD[k] / 6u > c; k--)
                ;
            if (m == OLD_BSEQ) {
                memset(o, 0, SC_BSEQ);
                o[SC_BSEQ + 1u] = OLD_BSEQ - 1u;
                return 0;
            }
            o[m] = c == (e - i) * u ? h : f;
            o[OLD_BSEQ + m] = (uint8_t)k;
        }
    }
    o[SC_BSEQ + 1u] = (uint8_t)(m - 1u);
    return 1;
}

static void set_bass_old(uint8_t *o) { bseq_old(set.bnote, set.bq_on, set.bq_steps, set.bq_len, o); }   /* (the settings'
                                                                                                      * at o) */

/* the settings as last saved, into raw as FRSH kept them (the tests: oldstore 16, 17): the sound FRSH_NV long, a
 * setting learned after it numbered so, the bass's sequence as it was (bseq_old); the magic the caller's */
static void set_frsh(void)
{
    uint32_t i;
    memcpy(raw, &set, 4u + FRSH_NV);
    memcpy(raw + 4u + FRSH_NV, (uint8_t *)&set + offsetof(settings_t, octave), FRSH_BYTES - 4u - FRSH_NV);
    set_bass_old(raw + 4u + FRSH_NV + offsetof(settings_t, bnote) - offsetof(settings_t, octave));
    for (i = 0; i < NLEARN; i++) {
        uint8_t *l = raw + offsetof(settings_t, learn_p) - (NSOUND - FRSH_NV) + i;
        if (*l >= NSOUND && *l < P_AFTER_AMT)
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
    set_bass_old(raw + 4u + FRSL_NV + offsetof(settings_t, bnote) - offsetof(settings_t, octave));
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(gm >> (8u * i));
    for (i = 0; i < NLEARN; i++) {
        uint8_t *l = raw + offsetof(settings_t, learn_p) - (NSOUND - FRSL_NV) + i;
        if (*l >= NSOUND && *l < P_AFTER_AMT)
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
    set_bass_old(raw + 4u + FRSO_NV + offsetof(settings_t, bnote) - offsetof(settings_t, octave));
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(SET_FRSK >> (8u * i));
    for (i = 0; i < NLEARN; i++) {
        uint8_t *l = raw + offsetof(settings_t, learn_p) - (NSOUND - FRSO_NV) + i;
        if (*l >= NSOUND && *l < P_AFTER_AMT)
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

/* the settings' record and the scenes' as 2.5 kept them (FRSL, FRSq: the sound FRSQ_NV long, SOFT not yet; a setting
 * learned after it numbered so); OBJ_BSEQ as it is */
static void old_frsq(void)
{
    uint32_t i, rec = SC_NAME + FRSQ_NV + SC_TAIL - SC_SEQ;
    uint8_t *o;
    memcpy(raw, &set, 4u + FRSQ_NV);
    memcpy(raw + 4u + FRSQ_NV, (uint8_t *)&set + offsetof(settings_t, octave), FRSL_BYTES - 4u - FRSQ_NV);
    set_bass_old(raw + 4u + FRSQ_NV + offsetof(settings_t, bnote) - offsetof(settings_t, octave));
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(SET_FRSL >> (8u * i));
    for (i = 0; i < NLEARN; i++) {
        uint8_t *l = raw + offsetof(settings_t, learn_p) - (NSOUND - FRSQ_NV) + i;
        if (*l == P_CH_SOFT)             /* (2.5 had no SOFT to learn a CC on: none) */
            *l = l[-(int32_t)NLEARN] = 0;
        if (*l >= NSOUND && *l < P_AFTER_AMT)
            *l = (uint8_t)(*l - (NSOUND - FRSQ_NV));
    }
    plat_store_save(OBJ_SETTINGS, raw, FRSL_BYTES);
    memcpy(raw, &scn, 8u);
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(SCN_FRSQ >> (8u * i));
    for (i = 0; i < NSCENES; i++) {
        o = raw + 8u + i * rec;
        memcpy(o, &scn.s[i], SC_NAME + FRSQ_NV);
        memcpy(o + SC_NAME + FRSQ_NV, &scn.s[i].nch, SC_TAIL - SC_SEQ);
    }
    plat_store_save(OBJ_SCENES, raw, 8u + NSCENES * rec);
}

static void old_frbt(uint32_t bm, uint32_t n)   /* (FRBT's FRBT_SCENE a scene, FRBS's FRBS_SCENE; FRBU's FRBU_SCENE:
                                                 * GLINT's sequence first; each bass's sequence as it was: bseq_old) */
{
    uint32_t i, g = bm == BSQ_FRBU ? SC_SEQ : 0u;
    for (i = 0; i < 4u; i++)
        raw[i] = (uint8_t)(bm >> (8u * i));
    for (i = 0; i < NSCENES; i++) {
        uint8_t *o = raw + 4u + i * n;
        memcpy(o, g ? scn.s[i].seq : scn.s[i].bnote, n);
        bseq_old(scn.s[i].bnote, scn.s[i].bq_on, scn.s[i].bq_steps, scn.s[i].bq_len, o + g);
    }
    plat_store_save(OBJ_BSEQ, raw, 4u + NSCENES * n);
}

/* the tests' own scene (testscene): 1.1's first factory scene, MIST, as it was (the tests were written on it; the
 * factory scenes since are another set), made as a factory scene was, loaded as slot 01 */
static void test_scene(void)
{
    static const scene_t MIST = {"MIST", {76, 89, 32, 19, 51, 44, 57, 25, 0, 70, 0, 89, 89, 83, 76, 38, 44, 51, 25, 38, 95,
                                          79, 64, 0, 95, 64, 13, 0, 25, 127, 50, 64, 57, 2, 0, 0, 0, 8},
                                 0, 0, {{0}}, {0}, -1, {0}, {0}, 0, 15, 14, 60, 0, 0, 1, 127, 127, 0, 0, 0, 0, {0}, 0, 0,
                                 0, 4, 3, 127, 127, 14, {{0}}};   /* (the drone's sequence none) */
    scene_t sc = MIST;
    more_none(&sc);                      /* (its LFOs whole, no SWELL: as it was) */
    tape_none(&sc);                      /* (LOFI TAPE II as it was: TAPE SPEED x1.00, not 0, x0.25) */
    glint_preset(sc.v, sc.v[P_GL_PRESET]);
    arp_defaults(sc.v);
    later_defaults(sc.v, P_BS_DRIVE);
    fm.scene = 0;
    fm.pick = 0;
    scene_take(&sc);
}

#ifndef PLAT_WEB
#include <fcntl.h>                       /* (pfstill: the simulator forked, pfc_still) */
#include <sys/wait.h>
#include <unistd.h>
/* PERFORM's LIVE as a page reads it (the tests': pfc, LIVE's frames as pf_send sent them). The checks' own 7-to-8 */
static uint32_t pfc_unpack(uint8_t *o, const uint8_t *s, uint32_t n)
{
    uint32_t i = 0, k = 0, j, h;
    while (i < n) {
        h = s[i++];
        for (j = 0; j < 7u && i < n; j++, i++)
            o[k++] = (uint8_t)(s[i] | (h >> j & 1u) << 7);
    }
    return k;
}

/* the next message: its cmd (10 an op; 9 STATE, 11 a long op: their parts joined, each in its place, every one but the
 * last LV_PART bytes, none over), its bytes unpacked into o (*on); 0 none, -1 a frame wrong */
static int pfc_next(uint8_t *o, uint32_t *on, uint32_t max)
{
    uint32_t parts = 0, part = 0, cmd = 0;
    *on = 0;
    while (pfc.rd < pfc.n) {
        uint32_t len = pfc.b[pfc.rd] | (uint32_t)pfc.b[pfc.rd + 1u] << 8, k;
        const uint8_t *f = pfc.b + pfc.rd + 2u;
        pfc.rd += 2u + len;
        if (len < 6u || f[0] != 0xF0u || f[1] != 0x7Du || f[2] != 0x46u || f[3] != 0x50u || f[len - 1u] != 0xF7u)
            return -1;
        if (f[4] < 9u || f[4] > 11u) {   /* (one pfwatch names: its bytes as sent) */
            if (parts || len - 6u > max)
                return -1;
            memcpy(o, f + 5, len - 6u);
            *on = len - 6u;
            cmd = f[4];
            break;
        }
        if (f[4] == 10u) {
            if (parts || len - 6u > LV_PART)
                return -1;               /* (an op amid a long one's parts; FST_LEAD's 27 bytes 31 packed: card #152) */
            *on = pfc_unpack(o, f + 5, len - 6u);
            cmd = 10;
            break;
        }
        k = len - 8u;                    /* (9, 11: part, parts, its bytes) */
        if (len < 9u || f[5] != part || !f[6] || (parts && (f[6] != parts || f[4] != cmd)) || k > LV_PART ||
            (part + 1u < f[6] && k != LV_PART) || *on + LV_RAW > max)
            return -1;
        cmd = f[4];
        parts = f[6];
        *on += pfc_unpack(o + *on, f + 7, k);
        if (++part == parts)
            break;
    }
    if (pfc.rd >= pfc.n)
        pfc.rd = pfc.n = 0;              /* (all read: room again) */
    return part == parts ? (int)cmd : -1;
}

static void pfc_say(int ok, const char *what, const char *got)
{
    printf("%s %s%s%s\n", ok ? "  ok" : "FAIL", what, ok ? "" : ": got ", ok ? "" : got);
    pfc.fails += !ok;
}

/* pfstate: the STATE asked now, as the tests make it (LIVE.md's layout, from the FM-1 as it is: the scene STORE would
 * make, frs_state_save's, each FRS_P_MOMENT setting, FRAME's transport, then the running state live_save gives at this
 * very instant, nothing rendered between, from that scene: the bytes lv_state's must be), for pfcheck; one asked while
 * one waits: that one */
static void pfc_want(void)
{
    scene_t sc;
    static uint8_t ld[LV_LEAD];
    uint8_t *w = pfc.want, st_b[STATE_BYTES];
    uint32_t i, n, sb = state_save(st_b), m = 0, ln, lm = 0;
    pfc.asks++;
    if (lv.state)
        return;
    scene_get(fm.scene, &sc);
    for (i = 0; i < NSOUND; i++)
        sc.v[i] = kept_v(i);
    scene_notes_take(&sc);
    w[0] = (uint8_t)sizeof sc;
    w[1] = (uint8_t)(sizeof sc >> 8);
    memcpy(w + 2, &sc, sizeof sc);
    n = 2u + (uint32_t)sizeof sc;
    w[n++] = (uint8_t)sb;
    memcpy(w + n, st_b, sb);
    n += sb;
    for (i = 0; i < NPARAM; i++)
        m += param_kind(i) == PK_MOMENT;
    w[n++] = (uint8_t)m;
    for (i = 0; i < NPARAM; i++)
        if (param_kind(i) == PK_MOMENT) {
            w[n++] = (uint8_t)i;
            w[n++] = i == P_GA_LATCH ? arp.latch : i == P_BA_LATCH ? barp.latch : st.v[i];
        }
    w[n++] = pf_playing();
    ln = live_save(w + n + 2u, LV_LIVE, w + 2, swell_held(btn_now), 0, 1, ld, sizeof ld, &lm);
    w[n++] = (uint8_t)ln;
    w[n++] = (uint8_t)(ln >> 8);
    n += ln;
    if (!ln || n + 2u + lm > LV_STATE)   /* (the LEAD's part after it, in the room the first leaves: card #152) */
        lm = 0;
    w[n++] = (uint8_t)lm;
    w[n++] = (uint8_t)(lm >> 8);
    memcpy(w + n, ld, lm);
    pfc.want_n = n + lm;
    pfc.want_ms = plat_ms() + 1u;        /* (the millisecond the engine runs next: the main loop ran this one's) */
}

/* the start, as STATE says it: each part in place, its layout whole (scene, state, moves, tracks, the running state,
 * the LEAD's part, nothing after), the state frs_state_save's ("FRst" 3, each FRS_P_STATE setting, the metronome last),
 * the moves each FRS_P_MOMENT's, the running state's head as a listener reads it ("FRlv", its version 1, the sound's
 * part in it, the scene's length, LIVE_NPARAM (the settings before the LEAD's), the millisecond plat_ms() + 1 at the
 * ask, the body's length; LV_LIVE at most), the LEAD's ("FRld", its version 1, the sound's part in it, LEAD_BYTES,
 * P_LD_WAVE, the same millisecond, its length; then the LEAD's settings as they are; LV_LEAD at most: card #152), and
 * every byte what pfstate asked for then (pfc_want) */
static void pfc_state(void)
{
    static uint8_t o[LV_STATE + LV_RAW];
    char t[128];
    uint32_t n = 0, sn, sb, mn, tr, ln, lm, i, ns = 0, nm = 0, bad = 0;
    const uint8_t *lv_b, *ld_b;
    int c = pfc_next(o, &n, sizeof o);
    if (c != 9) {
        snprintf(t, sizeof t, "cmd %d, not a STATE", c);
        pfc_say(0, "pfstate", t);
        return;
    }
    sn = o[0] | (uint32_t)o[1] << 8;
    sb = n > 2u + sn ? o[2u + sn] : 0u;
    mn = n > 3u + sn + sb ? o[3u + sn + sb] : 0u;
    tr = 2u + sn + 1u + sb + 1u + 2u * mn;   /* (tracks' place, the running state after it) */
    ln = n >= tr + 3u ? o[tr + 1u] | (uint32_t)o[tr + 2u] << 8 : 0u;
    lm = n >= tr + 5u + ln ? o[tr + 3u + ln] | (uint32_t)o[tr + 4u + ln] << 8 : 0u;   /* (the LEAD's after it) */
    if (sn != FRS_SCENE_BYTES || n != tr + 5u + ln + lm) {
        snprintf(t, sizeof t, "%u bytes: scene %u, state %u, moves %u, live %u, lead %u", (unsigned)n, (unsigned)sn,
                 (unsigned)sb, (unsigned)mn, (unsigned)ln, (unsigned)lm);
        pfc_say(0, "pfstate layout", t);
        return;
    }
    lv_b = o + tr + 3u;
    ld_b = lv_b + ln + 2u;
    for (i = 0; i < NPARAM; i++) {
        ns += param_kind(i) == PK_STATE;
        nm += param_kind(i) == PK_MOMENT;
    }
    bad |= n != pfc.want_n || memcmp(o, pfc.want, n) ? 1u : 0u;
    bad |= memcmp(o + 3u + sn, "FRst\3", 5) || sb != 5u + 2u * ns + 2u || o[1u + sn + sb] != STATE_METRO ? 2u : 0u;
    for (i = 0; i < ns; i++)
        bad |= param_kind(o[8u + sn + 2u * i]) != PK_STATE ? 2u : 0u;
    for (i = 0; i < mn; i++)
        bad |= param_kind(o[4u + sn + sb + 2u * i]) != PK_MOMENT ? 4u : 0u;
    bad |= mn != nm ? 4u : 0u;
    bad |= o[tr] > 7u ? 8u : 0u;
    bad |= ln < 16u || ln > LV_LIVE || memcmp(lv_b, "FRlv\1", 5) || lv_b[5] != LIVE_F_DSP ||   /* (the sound's part
                                          * in it: nothing rendered while it was read, here; no engine alone's) */
           (lv_b[6] | (uint32_t)lv_b[7] << 8) != FRS_SCENE_BYTES || (lv_b[8] | (uint32_t)lv_b[9] << 8) != LIVE_NPARAM ||
           (lv_b[10] | (uint32_t)lv_b[11] << 8 | (uint32_t)lv_b[12] << 16 | (uint32_t)lv_b[13] << 24) != pfc.want_ms ||
           16u + (lv_b[14] | (uint32_t)lv_b[15] << 8) != ln ? 16u : 0u;
#if FRS_LEAD
    bad |= lm < 16u + NLDS || lm > LV_LEAD || memcmp(ld_b, "FRld\1", 5) || ld_b[5] != LIVE_F_DSP ||
           (ld_b[6] | (uint32_t)ld_b[7] << 8) != NLDS || (ld_b[8] | (uint32_t)ld_b[9] << 8) != P_LD_WAVE ||
           memcmp(ld_b + 10, lv_b + 10, 4) || 16u + (ld_b[14] | (uint32_t)ld_b[15] << 8) != lm ||
           memcmp(ld_b + 16, &st.v[P_LD_WAVE], NLDS) ? 32u : 0u;   /* (its settings first, as they are) */
#else
    bad |= lm ? 32u : 0u;
#endif
    snprintf(t, sizeof t, "%s%s%s%s%s%s (scene %u, state %u, moves %u, tracks %u, live %u, lead %u)",
             bad & 1u ? "not as asked, " : "", bad & 2u ? "the state, " : "", bad & 4u ? "the moves, " : "",
             bad & 8u ? "the tracks, " : "", bad & 16u ? "the running state" : "", bad & 32u ? "the LEAD's" : "",
             (unsigned)sn, (unsigned)sb, (unsigned)mn, (unsigned)o[tr], (unsigned)ln, (unsigned)lm);
    pfc_say(!bad, "pfstate", t);
    if (!bad)
        printf("  pfstate: scene %u, state %u, moves %u, tracks %u, live %u (ms %u), lead %u, %u bytes in %u parts\n",
               (unsigned)sn, (unsigned)sb, (unsigned)mn, (unsigned)o[tr], (unsigned)ln, (unsigned)pfc.want_ms,
               (unsigned)lm, (unsigned)n, (unsigned)((n + LV_RAW - 1u) / LV_RAW));
}

/* pfop HEX [slot S | anchor | lead]: the next message is that op (cmd 10, its bytes HEX), or that long op (cmd 11: the
 * op, then slot S's scene, the ANCHOR's); lead: cmd 10, HEX then the LEAD's settings as they are now (FST_LEAD's: card
 * #152) */
static void pfc_op(const char *hex, const char *src, const char *s)
{
    static uint8_t o[1024], w[1 + FRS_SCENE_BYTES];
    char t[64];
    uint32_t n = 0, wn = 0, i;
    int c = pfc_next(o, &n, sizeof o), want = 10;
    for (i = 0; hex[2u * i] && hex[2u * i + 1u] && wn < 16u; i++) {
        unsigned v;
        sscanf(hex + 2u * i, "%2x", &v);
        w[wn++] = (uint8_t)v;
    }
#if FRS_LEAD
    if (!strcmp(src, "lead")) {
        memcpy(w + wn, &st.v[P_LD_WAVE], NLDS);
        wn += NLDS;
    }
#endif
    if (!strcmp(src, "slot") || !strcmp(src, "anchor")) {
        scene_t sc;
        if (*src == 's')
            scene_get((uint32_t)atoi(s), &sc);
        else
            sc = anc.sc;
        memcpy(w + 1, &sc, FRS_SCENE_BYTES);
        wn = 1u + FRS_SCENE_BYTES;
        want = 11;
    }
    t[0] = 0;
    for (i = 0; i < n && i < 16u; i++)
        snprintf(t + 2u * i, sizeof t - 2u * i, "%02x", o[i]);
    if (c != want || n != wn || memcmp(o, w, wn)) {
        char g[96];
        snprintf(g, sizeof g, "cmd %d, %u bytes %s%s", c, (unsigned)n, t, n > 16u ? "..." : "");
        pfc_say(0, "pfop", g);
        return;
    }
    snprintf(t + (n > 16u ? 32u : 2u * n), 32, "%s", n > 16u ? want == 11 ? "... (a scene)" : "... (the LEAD)" : "");
    printf("  ok pfop %s\n", t);
}

/* pfmsg HEX: the next message kept (LIVE's, pfwatch's) is that one: HEX its cmd, then its bytes as sent */
static void pfc_msg(const char *hex)
{
    uint8_t o[1024], w[16];
    char t[64];
    uint32_t n = 0, wn = 0, i;
    int c = pfc_next(o, &n, sizeof o);
    for (i = 0; hex[2u * i] && hex[2u * i + 1u] && wn < sizeof w; i++) {
        unsigned v;
        sscanf(hex + 2u * i, "%2x", &v);
        w[wn++] = (uint8_t)v;
    }
    snprintf(t, sizeof t, "%02x", c < 0 ? 0xFFu : (unsigned)c);
    for (i = 0; i < n && i < 24u; i++)
        snprintf(t + 2u + 2u * i, sizeof t - 2u - 2u * i, "%02x", o[i]);
    if (!wn || c != w[0] || n != wn - 1u || memcmp(o, w + 1, n))
        pfc_say(0, "pfmsg", t);
    else
        printf("  ok pfmsg %s\n", t);
}

/* pfvalues: the next messages kept are VALUES, every setting once, in order from the first, each as the FM-1 has it now
 * (a load's, Learn's START: pfwatch 3); the one after them left for the next read */
static void pfc_values(void)
{
    uint8_t o[1024];
    char t[64];
    uint32_t n, r, rn, next = 0, bad = 0, msgs = 0, i;
    for (;;) {
        r = pfc.rd;
        rn = pfc.n;
        if (pfc_next(o, &n, sizeof o) != 3 || n < 3u) {
            pfc.rd = r;
            pfc.n = rn;
            break;
        }
        msgs++;
        if (((uint32_t)o[0] << 7 | o[1]) != next || n != 3u + 2u * o[2]) {
            bad++;
            break;
        }
        for (i = 0; i < o[2]; i++)
            bad += next + i >= NPARAM || ((uint32_t)o[3u + 2u * i] << 7 | o[4u + 2u * i]) != st.v[next + i];
        next += o[2];
    }
    snprintf(t, sizeof t, "%u VALUES, settings 0 .. %u, %u off", (unsigned)msgs, (unsigned)next, (unsigned)bad);
    pfc_say(!bad && next == NPARAM, "pfvalues", t);
}

/* pfprs MIN MAX: every message kept since the last read a PRESSURE, MIN to MAX of them, the last the held key pressed
 * hardest as the engine has it now (pf_tick's), within its 4 (0 exactly) */
static void pfc_prs(int lo, int hi)
{
    uint8_t o[1024];
    char t[64];
    uint32_t n, k, want = 0;
    int c, cnt = 0, last = -1, bad = 0;
    while ((c = pfc_next(o, &n, sizeof o)) != 0) {
        if (c != 13 || n != 1u) {
            bad = 1;
            break;
        }
        cnt++;
        last = o[0];
    }
    for (k = 0; k < NKEYS && k < FRS_NKEYS; k++)
        if ((keys_prev >> k & 1u) && key[k].prs > want)
            want = key[k].prs;
    snprintf(t, sizeof t, "%d of them, the last %d (the engine's %u)%s", cnt, last, (unsigned)want,
             bad ? ", another message" : "");
    if (bad || cnt < lo || cnt > hi || last < 0 || (want == 0) != (last == 0) || last >= (int)want + 4 ||
        last + 4 <= (int)want)
        pfc_say(0, "pfprs", t);
    else
        printf("  ok pfprs %s\n", t);
}

/* pfpack: MIDI's 8-to-7 on what is known of it, each case its bytes as the page unpacks them */
static void pfc_pack(void)
{
    static const struct {
        uint8_t n, in[9], m, out[11];
    } K[] = {
        {1, {0x00}, 2, {0x00, 0x00}},
        {1, {0x80}, 2, {0x01, 0x00}},
        {3, {0xFF, 0x01, 0x80}, 4, {0x05, 0x7F, 0x01, 0x00}},
        {7, {0x81, 0x02, 0x83, 0x04, 0x85, 0x06, 0x87}, 8, {0x55, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}},
        {8, {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}, 10, {0x7F, 0, 0, 0, 0, 0, 0, 0, 0x01, 0x00}},
        {9, {0x40, 0x10, 0x7F, 0, 0, 0, 0, 0xC0, 0x7F}, 11, {0x00, 0x40, 0x10, 0x7F, 0, 0, 0, 0, 0x01, 0x40, 0x7F}},
    };
    uint8_t o[300], r[300], s[256];
    uint32_t i, m, bad = 0;
    for (i = 0; i < sizeof K / sizeof K[0]; i++) {
        m = pf_pack(o, K[i].in, K[i].n);
        if (m != K[i].m || memcmp(o, K[i].out, m)) {
            printf("FAIL pfpack case %u: %u bytes\n", (unsigned)i, (unsigned)m);
            bad++;
        }
    }
    for (i = 0; i < sizeof s; i++)
        s[i] = (uint8_t)(255u - i);
    m = pf_pack(o, s, sizeof s);         /* (each byte 7-bit, n + a seventh, back as it was) */
    for (i = 0; i < m; i++)
        bad += o[i] > 0x7Fu;
    bad += m != sizeof s + (sizeof s + 6u) / 7u || pfc_unpack(r, o, m) != sizeof s || memcmp(r, s, sizeof s);
    m = pf_pack(o, s, 0);
    bad += m != 0u;
    pfc.fails += bad != 0u;
    printf("%s pfpack (%u cases, 256 bytes back)\n", bad ? "FAIL" : "  ok", (unsigned)(sizeof K / sizeof K[0]));
}

/* pfstill, then pfstill same: a STATE asked changes nothing. At pfstill the simulator forks: the other run goes through
 * the lines to pfstill same without the asks (its pfstate and pfcheck not made; its output and its files nowhere), and
 * hands its renderhash back; this one waits for it, puts the files it reads back where they were (the script: its next
 * line), then runs the same lines asking: at pfstill same, the same renderhash */
static void pfc_still(int same)
{
    char t[112];
    int p[2], fd;
    off_t at[64];
    pid_t pid;
    if (same) {
        int got = pfc.still == 2 && pfc.still_got, ok = got && pfc.asks && render_hash == pfc.still_hash;
        if (pfc.still == 1) {            /* (the run without the asks: its hash to the other, and gone) */
            ssize_t w = write(pfc.still_fd, &render_hash, sizeof render_hash);
            _exit(w == (ssize_t)sizeof render_hash ? 0 : 1);
        }
        snprintf(t, sizeof t, "renderhash %016llx with %u STATEs asked, %016llx without%s",
                 (unsigned long long)render_hash, (unsigned)pfc.asks, (unsigned long long)pfc.still_hash,
                 got ? "" : " (none: no pfstill, or the other run cut short)");
        pfc_say(ok, "pfstill", t);
        if (ok)
            printf("  pfstill: %u STATEs asked, the same renders as none (renderhash %016llx)\n", (unsigned)pfc.asks,
                   (unsigned long long)render_hash);
        pfc.still = 0;
        return;
    }
    fflush(NULL);                        /* (nothing of this one's output twice) */
    if (pipe(p)) {
        pfc_say(0, "pfstill", "no pipe");
        return;
    }
    for (fd = 0; fd < 64; fd++)
        at[fd] = lseek(fd, 0, SEEK_CUR); /* (-1: none, or not a file) */
    pid = fork();
    if (pid < 0) {
        close(p[0]);
        close(p[1]);
        pfc_say(0, "pfstill", "no fork");
        return;
    }
    if (!pid) {                          /* the run without the asks: it writes into nothing but its pipe */
        int nul = open("/dev/null", O_WRONLY);
        if (nul < 0)
            _exit(1);                    /* (its output nowhere to go: cut short, the other says so) */
        for (fd = 1; fd < 64; fd++) {
            int fl = fcntl(fd, F_GETFL);
            if (fd != p[1] && fd != nul && fl >= 0 && (fl & O_ACCMODE) != O_RDONLY)
                dup2(nul, fd);
        }
        close(p[0]);
        pfc.still = 1;
        pfc.still_fd = p[1];
        return;
    }
    close(p[1]);
    pfc.still_got = read(p[0], &pfc.still_hash, sizeof pfc.still_hash) == (ssize_t)sizeof pfc.still_hash;
    close(p[0]);
    waitpid(pid, 0, 0);
    for (fd = 0; fd < 64; fd++)          /* (the other read on from the same places: back to where this one was) */
        if (at[fd] >= 0)
            lseek(fd, at[fd], SEEK_SET);
    pfc.still = 2;
    pfc.asks = 0;
}
#endif

int app_host_fails(void)                 /* (the app's own checks: PERFORM's LIVE) */
{
#ifndef PLAT_WEB
    return (int)pfc.fails;
#else
    return 0;
#endif
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
#ifndef PLAT_WEB
    if (!strcmp(cmd, "pfstill")) {       /* pfstill, pfstill same: the lines between run twice, with the STATEs asked
                                          * and without, the same renderhash (pfc_still) */
        pfc_still(!strcmp(a, "same"));
        return 1;
    }
    if (pfc.still == 1 && (!strcmp(cmd, "pfstate") || !strcmp(cmd, "pfcheck")))
        return 1;                        /* (pfstill's run without the asks) */
    if (!strcmp(cmd, "pfhello") || !strcmp(cmd, "pfbye") || !strcmp(cmd, "pfstate")) {   /* pfhello FLAGS, pfbye,
                                          * pfstate: a PERFORM page's HELLO (1 COMPUTER AUDIO, 2 LIVE), BYE, STATE? */
        uint8_t m[6] = {0x7Du, 0x46u, 0x50u, cmd[2] == 'h' ? 1u : cmd[2] == 'b' ? 2u : 9u, PF_VER,
                        (uint8_t)(atoi(a) & 0x7F)};
        if (m[3] == 9u && pf.on)
            pfc_want();
        app_sysex(m, m[3] == 1u ? 6u : 4u);
        return 1;
    }
    if (!strcmp(cmd, "pfmany")) {        /* pfmany OP N: the next messages, N or more of that op (a pressure's run) */
        uint8_t o[1024];
        uint32_t n, k2 = 0, r, rn;
        unsigned op = 0;
        char t[40];
        sscanf(a, "%2x", &op);
        for (;;) {                       /* (a look ahead: the one after the run left for the next read) */
            r = pfc.rd;
            rn = pfc.n;
            if (pfc_next(o, &n, sizeof o) != 10 || o[0] != op) {
                pfc.rd = r;
                pfc.n = rn;
                break;
            }
            k2++;
        }
        snprintf(t, sizeof t, "%u of op %02x", (unsigned)k2, op);
        pfc_say(k2 >= (uint32_t)atoi(b), "pfmany", t);
        return 1;
    }
    if (!strcmp(cmd, "pfflood")) {       /* pfflood N: FREEZE turned N times, as a hand turns it (the queue's room) */
        int i2;
        for (i2 = 0; i2 < atoi(a); i2++)
            param_set(P_FREEZE, !st.v[P_FREEZE]);
        return 1;
    }
    if (!strcmp(cmd, "pfjam")) {         /* pfjam N: LIVE's next N frames refused (the wire full) */
        pfc.jam = (uint32_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "pfop")) {          /* pfop HEX [slot S | anchor | lead]: the next LIVE message, that op (pfc_op) */
        pfc_op(a, b, c);
        return 1;
    }
    if (!strcmp(cmd, "pfwatch")) {       /* pfwatch CMD...: PERFORM's messages of those cmds kept too (4 VALUE, 13
                                          * PRESSURE...), read by pfmsg, pfnone; pfwatch alone: LIVE's only again.
                                          * Never beside a STATE or a long op: one kept between their parts breaks
                                          * their reading (pfc_next) */
        const char *w[4] = {a, b, c, d};
        pfc.watch = 0;
        for (i = 0; i < 4u; i++)
            if (*w[i])
                pfc.watch |= 1u << (atoi(w[i]) & 31);
        return 1;
    }
    if (!strcmp(cmd, "pfmsg")) {         /* pfmsg HEX: the next message kept is that one, its cmd then its bytes */
        pfc_msg(a);
        return 1;
    }
    if (!strcmp(cmd, "pfvalues")) {      /* pfvalues: the next ones kept, VALUES of every setting as they are now */
        pfc_values();
        return 1;
    }
    if (!strcmp(cmd, "pfprs")) {         /* pfprs MIN MAX: PRESSUREs alone since, MIN to MAX, the last the engine's */
        pfc_prs(atoi(a), atoi(b));
        return 1;
    }
    if (!strcmp(cmd, "pfnone")) {        /* pfnone: no LIVE message since the last read */
        uint8_t o[1024];
        uint32_t n;
        int k2 = pfc_next(o, &n, sizeof o);
        char t[24];
        snprintf(t, sizeof t, "cmd %d, %u bytes", k2, (unsigned)n);
        pfc_say(k2 == 0, "pfnone", t);
        return 1;
    }
    if (!strcmp(cmd, "pfsave")) {        /* pfsave FILE: the next LIVE message, a STATE, its bytes joined and unpacked into
                                          * FILE (engine/tests/snap.c loads an FM-1's into the engine alone) */
        static uint8_t o[LV_STATE + LV_RAW];
        uint32_t n;
        FILE *f;
        int ok = pfc_next(o, &n, sizeof o) == 9 && (f = fopen(a, "wb")) != NULL;
        if (ok) {
            ok = fwrite(o, 1, n, f) == n;
            fclose(f);
        }
        pfc_say(ok, "pfsave", a);
        return 1;
    }
    if (!strcmp(cmd, "pfdump")) {        /* pfdump: every LIVE message since the last read, printed (read) */
        static uint8_t o[LV_STATE + LV_RAW];
        uint32_t n, i;
        int k2;
        while ((k2 = pfc_next(o, &n, sizeof o)) != 0) {
            printf("  pf: cmd %d, %u bytes ", k2, (unsigned)n);
            for (i = 0; i < n && i < 24u; i++)
                printf("%02x", o[i]);
            printf("%s\n", n > 24u ? "..." : "");
            if (k2 < 0)
                break;
        }
        return 1;
    }
    if (!strcmp(cmd, "pfcheck")) {       /* pfcheck: the next LIVE message a STATE, as the FM-1 is now (pfc_state) */
        pfc_state();
        return 1;
    }
    if (!strcmp(cmd, "pfpack")) {        /* pfpack: MIDI's 8-to-7 (pf_pack) on known cases */
        pfc_pack();
        return 1;
    }
    if (!strcmp(cmd, "cpuload")) {       /* cpuload N: the load the simulator reads now, N % (load_watch sheds over
                                          * 82, gives back at 75 or less: shed.txt) */
        host_load = (uint32_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "saveridle")) {     /* saveridle MS: the screensaver after MS untouched (0: its ten minutes) */
        saver.after = (uint32_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "saveraim")) {      /* saveraim N: N starts drawn and run (saver_aim_check; expect saver_aim*) */
        saver_aim_check((uint32_t)atoi(a));
        return 1;
    }
    if (!strcmp(cmd, "saverbyte")) {     /* saverbyte N: the stored settings' SCREENSAVER byte made N (then reboot: as
                                          * a record of 3.0, or of a later FRASIL, reads) */
        set.saver_off = (uint8_t)atoi(a);
        plat_store_save(OBJ_SETTINGS, &set, sizeof set);
        return 1;
    }
    if (!strcmp(cmd, "a4byte")) {        /* a4byte N: the stored settings' TUNE A4 byte made N (-128 .. 127; then reboot:
                                          * as a later FRASIL's reads) */
        set.a4 = (int8_t)atoi(a);
        plat_store_save(OBJ_SETTINGS, &set, sizeof set);
        return 1;
    }
    if (!strcmp(cmd, "saverseed")) {     /* saverseed R: the screensaver's next starts drawn from R (0: from the time) */
        saver_seed = (uint32_t)strtoul(a, 0, 10);
        return 1;
    }
    if (!strcmp(cmd, "saverinfo")) {     /* saverinfo: where the screensaver is (the shots' scripts) */
        printf("  saverinfo on %u steps %d aim %d corners %u party %u at %d,%d way %d,%d now %u step_at %u\n",
               (unsigned)saver.on, (int)saver.steps, (int)saver.aim, (unsigned)saver.corners, (unsigned)saver.party,
               saver.x, saver.y, saver.dx, saver.dy, (unsigned)plat_ms(), (unsigned)saver.step_at);
        return 1;
    }
    if (!strcmp(cmd, "savermark")) {     /* savermark: where the logo is now (expect saver_dist: how far since) */
        saver_mark[0] = saver.x;
        saver_mark[1] = saver.y;
        return 1;
    }
    if (!strcmp(cmd, "drawnmark")) {     /* drawnmark: the screen's frames counted from here, the page it shows now the
                                          * first (expect drawnhome, drawnflips, drawnstale) */
        ui_seen.home = ui_seen.flips = ui_seen.stale = 0;
        ui_seen.last = ui_look();
        return 1;
    }
#ifndef PLAT_WEB
    if (!strcmp(cmd, "framecost")) {     /* framecost: ANCHOR's frame, what it sent the screen alone since the last (its
                                          * bands' parts aside: drawn with them) */
        printf("  framecost: %u pixels in %u fills\n", (unsigned)af_px, (unsigned)af_fills);
        af_px = af_fills = 0;
        return 1;
    }
#endif
    if (!strcmp(cmd, "uicost")) {        /* uicost: the UI's frames since the last, awake, asleep and a corner's: how
                                          * many, their time each and the longest (the host's CPU), the pixels the
                                          * saver's steps sent each, a corner's frames' pixels and blits */
        static const char *const KN[3] = {"awake", "saver", "corner"};
        uint32_t k;
        for (k = 0; k < 3u; k++) {
            printf("  uicost %s: %u frames in %u ms, %.1f us each, %.1f us the longest%s", KN[k], (unsigned)ui_cost.n[k],
                   (unsigned)ui_cost.ms[k], ui_cost.n[k] ? (double)ui_cost.ticks[k] / PLAT_TICKS_US / ui_cost.n[k] : 0.0,
                   (double)ui_cost.tmax[k] / PLAT_TICKS_US, k == 1u ? "" : "\n");
            if (k == 1u)
                printf(", %u steps drawn, %.0f pixels each\n", (unsigned)ui_cost.draws,
                       ui_cost.draws ? (double)ui_cost.px / ui_cost.draws : 0.0);
        }
        printf("  uicost corner: %u frames drawn, %.0f pixels and %.1f blits each, %u pixels the most\n",
               (unsigned)ui_cost.pframes, ui_cost.pframes ? (double)ui_cost.ppx / ui_cost.pframes : 0.0,
               ui_cost.pframes ? (double)ui_cost.prun / ui_cost.pframes : 0.0, (unsigned)ui_cost.pmax);
        memset(&ui_cost, 0, sizeof ui_cost);
        return 1;
    }
#endif
    if (!strcmp(cmd, "testscene")) {     /* testscene: the tests' own scene (test_scene), as if slot 01 were it */
        test_scene();
        return 1;
    }
#if FRS_LEAD
    if (!strcmp(cmd, "scnload")) {       /* scnload N: slot N (1 .. 16) loaded, as SAVE + its key (its LEAD with it) */
        scene_go((uint32_t)(atoi(a) - 1) % NSCENES);
        return 1;
    }
    if (!strcmp(cmd, "ldpold")) {        /* ldpold V: OBJ_LEAD written again in version V's layout: 3 (0683c49); 2
                                          * (cf592ae) and 1 (1489f1d .. 7154725) without the filter's ADSR; 4 (baf25e8
                                          * .. 284ea21) without the player's part; PRESET as it is (none made over
                                          * since admin #875) */
        uint8_t b[LDP_ALL];
        uint32_t s, k, v = (uint32_t)atoi(a), ent = v == 3u ? LDP_ENTRY : LDP_ENTRY2;
        if (!ldp_read(b) || v < 1u || v > 4u)
            return 1;
        if (v == 4u) {
            plat_store_save(OBJ_LEAD, b, LDP_BYTES);
            return 1;
        }
        for (s = 0; s < NSCENES; s++) {  /* (the first first: each entry nearer than it was, its first byte first) */
            uint8_t *e = b + LDP_HEAD + s * ent;
            for (k = 0; k < ent; k++)
                e[k] = b[LDP_HEAD + s * LDP_ENTRY + k];
        }
        b[4] = (uint8_t)v;
        b[6] = (uint8_t)ent;
        b[7] = (uint8_t)(ent - 1u);
        plat_store_save(OBJ_LEAD, b, LDP_HEAD + NSCENES * ent);
        return 1;
    }
#endif
    if (!strcmp(cmd, "scnstore")) {      /* scnstore N NAME: the sound stored into slot N (1 .. 16) under NAME (_ a space;
                                          * SC_LONG at most), as STORE and NAME IT do (scene_write) */
        char t[SC_LONG + 1];
        uint32_t i;
        for (i = 0; i < SC_LONG && b[i]; i++)
            t[i] = b[i] == '_' ? ' ' : b[i];
        t[i] = 0;
        scene_write((uint32_t)(atoi(a) - 1) % NSCENES, t);
        return 1;
    }
    if (!strcmp(cmd, "dropnames")) {     /* dropnames: OBJ_NAMES none, the rest of the store as it is (3.0's store) */
        plat_store_save(OBJ_NAMES, raw, 0);
        return 1;
    }
    if (!strcmp(cmd, "frnm")) {          /* frnm: OBJ_NAMES written again as 3.0.1 wrote it (FRNM: the names' rest alone),
                                          * from the scenes as the store has them; the rest of the store as it is */
        uint32_t s, sum[NSCENES];
        if (plat_store_load(OBJ_SCENES, raw, sizeof raw) != (int)SCN_BYTES)
            return 1;
        for (s = 0; s < NSCENES; s++)
            sum[s] = rec_sum(raw + 8u + s * SC_REC, SC_REC);
        for (s = 0; s < 4u; s++)
            raw[s] = (uint8_t)(NMS_FRNM >> (8u * s));
        for (s = 0; s < NSCENES; s++) {
            uint8_t *o = raw + 4u + s * NMS_SCENE_FRNM;
            o[0] = (uint8_t)sum[s], o[1] = (uint8_t)(sum[s] >> 8), o[2] = (uint8_t)(sum[s] >> 16), o[3] = (uint8_t)(sum[s] >> 24);
            memcpy(o + 4, scn.s[s].name_more, SC_MORE);
        }
        plat_store_save(OBJ_NAMES, raw, NMS_BYTES_FRNM);
        return 1;
    }
    if (!strcmp(cmd, "frsq")) {          /* frsq: the settings written again as 3.0.2 wrote them (FRSQ: to LOFI TAPE II's
                                          * four), from the record as last saved */
        settings_t o = set;
        o.magic = SET_FRSQ;
        plat_store_save(OBJ_SETTINGS, &o, FRSQ_BYTES);
        return 1;
    }
    if (!strcmp(cmd, "frnn") || !strcmp(cmd, "frsr")) {   /* frnn: OBJ_NAMES, frsr: the settings, written again as
                                          * 3.0.2-b5 wrote them (FRNN, FRSR: each AMOUNT in percent, x0.00 .. x1.00 as
                                          * 0 % .. +100 %, past x1.00 +100 %), from the store as it is */
        uint32_t s, k;
        int nn = cmd[2] == 'n';
        uint8_t *a;
        if (nn ? plat_store_load(OBJ_NAMES, raw, sizeof raw) != (int)NMS_BYTES
               : plat_store_load(OBJ_SETTINGS, raw, sizeof raw) != (int)sizeof(settings_t))
            return 1;
        for (s = 0; s < 4u; s++)
            raw[s] = (uint8_t)((nn ? NMS_FRNN : SET_FRSR) >> (8u * s));
        for (s = 0; s < (nn ? NSCENES : 1u); s++) {
            a = nn ? raw + 4u + s * NMS_SCENE + 4u + SC_MORE : raw + offsetof(settings_t, lfo_amt);
            for (k = 0; k < 12u; k++)
                a[k] = (uint8_t)(a[k] > 100u ? 200u : 100u + a[k]);
        }
        plat_store_save(nn ? OBJ_NAMES : OBJ_SETTINGS, raw, nn ? NMS_BYTES : (uint32_t)sizeof(settings_t));
        return 1;
    }
    if (!strcmp(cmd, "store30")) {       /* store30 N: slot N (1 .. 16) stored again as 3.0 stores it: the sound as it is
                                          * now, the name's first sixteen, OBJ_NAMES left as it was */
        uint32_t s = (uint32_t)(atoi(a) - 1) % NSCENES, i;
        for (i = 0; i < NSOUND; i++)
            scn.s[s].v[i] = kept_v(i);
        scene_notes_take(&scn.s[s]);
        scenes_store(0);
        return 1;
    }
#ifndef PLAT_WEB
    if (!strcmp(cmd, "renderhash")) {    /* renderhash: every sample rendered since the start, hashed (tests/engine.txt:
                                          * the engine's output, bit for bit) */
        printf("  renderhash %016llx %llu frames\n", (unsigned long long)render_hash, (unsigned long long)render_frames);
        return 1;
    }
    if (!strcmp(cmd, "pitch")) {         /* pitch HZ: the output's pitch near HZ measured from now (pz), 0: no more */
        pz.near = atof(a);
        pz.on = (uint8_t)(pz.near > 0.0);
        pz.n = 0;
        pz.hz = 0.0;
        return 1;
    }
    if (!strcmp(cmd, "edges")) {        /* edges: the blocks' edges measured from now (eg; expect edgedb) */
        eg.edge = eg.mid = 0.0;
        eg.on = 1;
        return 1;
    }
    if (!strcmp(cmd, "pitchref")) {      /* pitchref: the pitch measured so far kept, for pitchcents */
        pz.ref = pz_pitch();
        printf("  pitchref %.3f Hz\n", pz.ref);
        return 1;
    }
#endif
    if (!strcmp(cmd, "param")) {         /* param P X: setting P to X, as an encoder sets it (tests/engine.txt) */
        param_set(atoi(a) % NPARAM, atoi(b));
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
        if (fm.prefs_due)
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
    if (!strcmp(cmd, "oldstore") && k == 19) {   /* oldstore 22: 2.5's records (2.5, 2.5.1), before CHOIR III's SOFT
                                                  * (FRSq, FRSL; FRBU as now): the scenes and the settings as last
                                                  * saved, without it */
        old_frsq();
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 20) {   /* oldstore 23: 2.6's records (FRSM, FRBU; FRSs as now): the settings
                                                  * and the scenes' sequences as last saved, each bass's sequence in
                                                  * sixteen steps of their own lengths (bseq_old), no RATE */
        memcpy(raw, &set, FRSN_BYTES);
        for (i = 0; i < 4u; i++)
            raw[i] = (uint8_t)(SET_FRSM >> (8u * i));
        set_bass_old(raw + offsetof(settings_t, bnote));
        raw[offsetof(settings_t, bq_len)] = 0;   /* (FRSM's padding) */
        raw[offsetof(settings_t, bq_len) + 1u] = 0;
        plat_store_save(OBJ_SETTINGS, raw, FRSN_BYTES);
        old_frbt(BSQ_FRBU, FRBU_SCENE);
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 21) {   /* oldstore 24: 2.7's first test builds' records (FRSN, FRBV; FRSs as
                                                  * now): the settings and the scenes' sequences as last saved, to the
                                                  * bass's RATE, its PLAYS SEQUENCE then (SEQ's 1) */
        memcpy(raw, &set, FRSN_BYTES);
        for (i = 0; i < 4u; i++)
            raw[i] = (uint8_t)(SET_FRSN >> (8u * i));
        raw[offsetof(settings_t, bq_on)] = set.bq_on == SQT_SEQ;
        raw[offsetof(settings_t, bq_len) + 1u] = 0;   /* (its padding) */
        plat_store_save(OBJ_SETTINGS, raw, FRSN_BYTES);
        for (i = 0; i < 4u; i++)
            raw[i] = (uint8_t)(BSQ_FRBV >> (8u * i));
        for (i = 0; i < NSCENES; i++) {
            o = raw + 4u + i * FRBV_SCENE;
            memcpy(o, scn.s[i].seq, FRBV_SCENE);
            o[SC_SEQ + SC_BSEQ] = scn.s[i].bq_on == SQT_SEQ;
        }
        plat_store_save(OBJ_BSEQ, raw, 4u + NSCENES * FRBV_SCENE);
        return 1;
    }
    if (!strcmp(cmd, "oldstore") && k == 22) {   /* oldstore 25: 2.7's test builds' FRBW (b4 .. b7; FRSs and FRSO as
                                                  * now): the scenes' sequences as last saved, without the bass's SPACE */
        for (i = 0; i < 4u; i++)
            raw[i] = (uint8_t)(BSQ_FRBW >> (8u * i));
        for (i = 0; i < NSCENES; i++)
            memcpy(raw + 4u + i * FRBW_SCENE, scn.s[i].seq, FRBW_SCENE);
        plat_store_save(OBJ_BSEQ, raw, 4u + NSCENES * FRBW_SCENE);
        return 1;
    }
    if (!strcmp(cmd, "oldscene")) {      /* oldscene N: the scene as it plays now as N bytes of an earlier layout had it
                                          * (323 FRSx, 322 FRSw, 298 FRSu, 297 FRSt), made over as the River's and the iPad's are
                                          * (scene_over), taken: the bass's arpeggio gone, PLAYS' 2 written 1 first */
        static scene_t os;
        uint32_t n = (uint32_t)atoi(a);
        os = scn.s[0];
        scene_notes_take(&os);
        if (n <= FRSU_SCENE)
            os.bq_on = os.bq_on == SQT_SEQ;
        if (n == FRST_SCENE) {
            uint8_t o[SC_BSEQ + 2u];
            bseq_old(os.bnote, SQT_SEQ * os.bq_on, os.bq_steps, os.bq_len, o);
            memcpy(os.bnote, o, SC_BSEQ);
            os.bq_steps = o[SC_BSEQ + 1u];
        }
        memset((uint8_t *)&os + n, 0xEE, sizeof os - n);
        if (!scene_over(&os, n))
            scene_take(&os);
        return 1;
    }
    if (!strcmp(cmd, "setp")) {          /* setp P V: setting P set to V, as a knob or a PERFORM page's SET sets it
                                          * (RECORD has no knob: REC's own tests tap it) */
        param_set(atoi(a), atoi(b));
        return 1;
    }
    if (!strcmp(cmd, "oldfrso")) {       /* oldfrso K: the settings as 3.0's first builds kept them (FRSO, before KEYS), KEY
                                          * MODE K (a boot reads them in) */
        store_all();
        set.magic = SET_FRSO;
        set.keys = (uint8_t)atoi(a);
        plat_store_save(OBJ_SETTINGS, &set, FRSO_BYTES);
        set.magic = SET_MAGIC;
        return 1;
    }
    if (!strcmp(cmd, "stepmoves")) {     /* stepmoves 1: RECORD STEP moves STEP on (the engine's other way, basic.txt's
                                          * older parts) */
        test_step_moves = (uint8_t)atoi(a);
        step_stays = step_hand = !test_step_moves;
        return 1;
    }
    if (!strcmp(cmd, "recanywhere")) {   /* recanywhere 1: a take stays on leaving the SEQ and ARP pages */
        test_rec_anywhere = (uint8_t)atoi(a);
        return 1;
    }
#ifndef PLAT_WEB
    if (!strcmp(cmd, "arplog")) {        /* arplog 1: each arpeggio's step and each set it takes, printed (ARP_TRACE) */
        test_arp_log = (uint8_t)atoi(a);
        return 1;
    }
#endif
    if (!strcmp(cmd, "keysstay")) {      /* keysstay 1: NEW SCENE leaves KEYS as they are */
        test_keys_stay = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "loopuneven")) {    /* loopuneven 1: input passes unevenly, not one a millisecond (app_input_only) */
        test_loop_uneven = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "nogrid")) {        /* nogrid 1: the lists of names turned as any knob, at once */
        test_no_grid = (uint8_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "nudge")) {         /* nudge P D: setting P moved by D, a knob turned wherever it is */
        param_set(atoi(a), (int32_t)st.v[atoi(a) % NPARAM] + atoi(b));
        fm.touched = (uint8_t)atoi(a);
        fm.touched_at = plat_ms();
        return 1;
    }
    if (!strcmp(cmd, "gopage")) {        /* gopage N: page N on the screen at once (what these tests are about comes after;
                                          * the buttons' own way there: "the pages" in basic.txt) */
        if (atoi(a) == PG_SCENES) {
            fm.prev_page = fm.page;
            fm.pick = fm.scene;
        }
        fm.page = (uint8_t)atoi(a);
        fm.logo = 0;
        fm.help = 0;
        return 1;
    }
    if (!strcmp(cmd, "bseqkeep")) {      /* bseqkeep: the bass's sequence kept, as it is, for bseqsame */
        memcpy(bq_kept, bnote, SC_BSEQ);
        bq_kept[SC_BSEQ] = st.v[P_BQ_STEPS];
        bq_kept[SC_BSEQ + 1u] = st.v[P_BQ_LEN];
        return 1;
    }
    if (!strcmp(cmd, "oldbass")) {       /* oldbass HEX: a bass's sequence as 2.6 kept it (sixteen notes, their lengths:
                                          * BQ_OLD's indices, SEQ STEPS - 1; 33 bytes), made over as a load makes it
                                          * (bseq_over), into the one playing */
        uint8_t o[SC_BSEQ] = {0}, steps = OLD_BSEQ - 1u, len = 0;
        for (i = 0; a[0] && a[1] && i <= SC_BSEQ; i++, a += 2) {
            char h[3] = {a[0], a[1], 0};
            uint8_t x = (uint8_t)strtoul(h, 0, 16);
            if (i < SC_BSEQ)
                o[i] = x;
            else
                steps = x;
        }
        bseq_over(o, &steps, &len);
        memcpy(bnote, o, SC_BSEQ);
        st.v[P_BQ_STEPS] = steps;
        st.v[P_BQ_LEN] = len;
        bseq_view();
        st.dirty = (uint8_t)sound_changed();
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

