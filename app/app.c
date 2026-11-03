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
#define FRS_NKEYS NKEYS
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

#define SET_MAGIC 0x50535246u            /* "FRSP": the settings record (a new one when its layout changes) */
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
#define DSQ_MAGIC 0x51445246u            /* "FRDQ": the scenes' drone sequences (OBJ_DSEQ, 3.0), each its settings (dq_on ..
                                          * dq_trans), then its steps' notes seven bits each (dsq_pack) */
#define UPR_MAGIC 0x50555246u            /* "FRUP": the player's presets (OBJ_UPRE, 3.0) */
#define BSQ_FRBW 0x57425246u             /* "FRBW": 2.7's test builds' (b4 ..): to the arpeggio's notes, no SPACE */
#define BSQ_FRBV 0x56425246u             /* "FRBV": 2.7's first test builds': to RATE, PLAYS SEQUENCE then (1: on) */
#define BSQ_FRBU 0x55425246u             /* "FRBU": 2.6's (2.1 .. 2.6.1): to TEMPO, the bass's sixteen notes and their
                                          * lengths (bseq_over) */
#define BSQ_FRBT 0x54425246u             /* "FRBT": the one before (with FRSl): from the bass's notes on */
#define BSQ_FRBS 0x53425246u             /* "FRBS": the one before the scenes' TEMPO (FRBT's less one a scene) */
#define SHARD_MAGIC 0x7A535246u          /* "FRSz": a scene whole (scene_t: its sequences, its TEMPO, the bass's RATE,
                                          * arpeggio, BASS TONE, SPACE and GLINT's AUTO VIB after the rest, then the
                                          * drone's sequence), as the River keeps it, a shard; "FRSy": 3.0's before the
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
static int8_t up_cur[3];                 /* each track's: the preset of the player's taken or stored last, -1 none (a factory
                                          * one taken, a scene): turned since or not, INTO starts on it */

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
#define DSQ_SCENE (DSQ_SET + DSQ_PACK)   /* 230 bytes: sixteen scenes fit the object (scene_t has them a byte each) */
#define DSQ_BYTES (4u + NSCENES * DSQ_SCENE)
static uint8_t raw[8u + NSCENES * FRSL_REC];   /* an old record as it was saved (the largest, FRSl's scenes: 3800),
                                                * and the scenes' records as they are written and read (FRSs's:
                                                * 3672) */
static uint8_t store_old;                /* bit 0 the scenes, bit 1 the settings: made over from an earlier layout at
                                          * load, the store's still in it (a backup saves them first) */
_Static_assert(sizeof(scene_t) == SC_REC + BSQ_SCENE + DSQ_SET + SC_DSEQ * DQ_NOTES && offsetof(scene_t, seq) == SC_REC &&
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
_Static_assert(NBQLEN == 9u && SC_BSEQ == 32u && SC_SEQ == SC_BSEQ && P_BQ_NLEN == P_CLOCK + 1 && P_GL_VIB_AUTO == 196 &&
                   P_DQ_ON == P_GL_VIB_AUTO + 1 && P_DQ_NOTES == P_DQ_STEP + 1 && P_COUNT_IN + 1 == P_ARP_SNAP &&
                   P_SAVER == P_ARP_SNAP + 1 && P_SAVER + 1 == NPARAM,
               "the bass's sequence's settings (params.c PD: RATE's 9, STEP's and SEQ LENGTH's 32, LENGTH's 1 .. 32); "
               "LENGTH numbered last, then what came after it; the two sequences' steps alike (step_*)");

_Static_assert(sizeof(settings_t) == 280u && offsetof(settings_t, arp_snap) == 278u && offsetof(settings_t, saver_off) == 279u,
               "ARP SNAP and SCREENSAVER in FRSP's padding: the record's size as 3.0's first builds wrote it (their backups "
               "restored as they are)");
_Static_assert(sizeof raw >= sizeof(upresets_t) && sizeof raw >= SCN_BYTES && sizeof raw >= BSQ_BYTES && sizeof raw >= DSQ_BYTES && sizeof raw >= sizeof(settings_t) &&
                   sizeof raw >= 8u + NSCENES * FRSH_REC, "raw holds the largest");
static uint32_t name_bytes(const layout_t *l) { return l->extra >= 3u ? SC_NAME : OLD_NAME; }
static uint32_t seq_bytes(const layout_t *l) { return l->extra >= 2u && l->extra != 5u ? SC_SEQ : 0u; }
static uint32_t scn_bytes(const layout_t *l)
{
    return name_bytes(l) + l->nv + 2u + SC_CHORDS * 5u + SC_ARP + (l->extra ? 1u : 0u) + seq_bytes(l);
}
static uint32_t set_bytes(const layout_t *l) { return (4u + l->nv + 5u + 2u * (l->extra ? 1u : 0u) + seq_bytes(l) + 3u) & ~3u; }
static uint32_t rd32(const uint8_t *b) { return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

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
    uint8_t touched;                     /* the parameter turned last (the screen shows it a moment) */
    uint8_t store_hold;                  /* a backup streaming to USB: nothing saved until it ends (app_backup_hold) */
    uint8_t prefs_due;                   /* prefs_save asked: 1 once nothing sounds (prefs_quiet), 2 now (SPK/JACK) */
    uint8_t booting;                     /* app_init done, the UI not started yet (app_ui_init: the logo) */
    uint8_t metro_on;                    /* METRONOME's last on value (REC held comes back to it) */
    uint8_t edit_map;                    /* EDIT held: its three keys up */
    uint32_t oct_at;                     /* OCT+, OCT-: when, the octave large a moment */
    int8_t oct_v;
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
#ifndef PLAT_HOST
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

/* ---------------------------------------------------------- the screensaver --- */
/* SCREENSAVER (GLO > SCREEN's K2, ON by default): ten minutes without a touch of the panel (a button, a knob, a key;
 * MIDI, a PERFORM page and the River are not touches: they play, the screen sleeps on), the screen black and the logo
 * drifting on it, a pixel a step on both axes, bouncing off the edges, its colour the next at each bounce (ui.c
 * saver_screen: only its old and new boxes drawn again). The sound, the sequences, MIDI, the clock and a take go on as
 * they were: the screen alone sleeps (it costs less than the river). The touch that wakes it only wakes it: the button
 * or key that did it is dropped till it is let go (no press, no tap, no hold, no note: a sleepy hand on a lit key does
 * not let a drone go), the knob that did it turns nothing till it has rested SAVER_GRACE_MS (SAVER_GRACE_MAX at most:
 * one turning on is heard). What was on the screen comes back as it was (a page, the help, a pop-up, a name). A pop-up
 * that comes meanwhile (a restore from the River) wakes it. The lights stay as they are (they tell what plays) */
#include "frasil_saver.h"                /* SAVER_W x SAVER_H: the logo, smaller (tools/gen_logo.py) */
#define SAVER_MS 600000u                 /* ten minutes */
#define SAVER_GRACE_MS 350u
#define SAVER_GRACE_MAX 1500u
static struct {
    uint32_t idle_at;                    /* the last touch (plat_ms) */
    uint32_t after;                      /* idle this long: SAVER_MS (the tests' saveridle: less) */
    uint32_t woke_at, enc_until;         /* woken: when; the knobs' detents dropped till then */
    uint32_t btn_spent, keys_spent;      /* down when it woke: dropped till let go */
    uint32_t step_at;                    /* the drift's last step (ui.c) */
    uint32_t steps, bounces;             /* since it came (the tests) */
    int16_t x, y;                        /* the logo's top left */
    int8_t dx, dy;
    uint8_t on, drawn, col;
} saver;
#if defined PLAT_HOST && !defined PLAT_WEB
static uint32_t saver_out;               /* (the tests: steps that left the logo outside the screen) */
static struct {
    uint32_t n[2], ms[2], ticks[2];      /* the UI's frames, awake [0] and asleep [1]: how many, over what, their time */
    uint32_t draws, px;                  /* the saver's steps drawn, the pixels they sent */
} ui_cost;                               /* (uicost) */
#endif

static void saver_wake(void)
{
    uint32_t now = plat_ms();
    saver.on = 0;
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
    if (saver.on || !st.v[P_SAVER] || now - saver.idle_at < (saver.after ? saver.after : SAVER_MS))
        return;
    r = now * 2654435761u;               /* (where it starts and which way: anywhere) */
    saver.on = 1;
    saver.drawn = 0;
    saver.steps = saver.bounces = 0;
    saver.col = 0;
    saver.x = (int16_t)((r >> 8) % (240u - SAVER_W + 1u));
    saver.y = (int16_t)((r >> 20) % (240u - SAVER_H + 1u));
    saver.dx = (r & 1u) ? 1 : -1;
    saver.dy = (r & 2u) ? 1 : -1;
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
    if (saver.on) {                      /* (it wants the eyes: the screen wakes, nothing dropped) */
        saver.on = 0;
        saver_touch();
    }
    pop.drawn = 0;
    pop.btn = plat_buttons();            /* (SEL already down closes it only pressed again) */
    fm.hold_done = ~0u;                  /* the buttons down now: no tap, no hold */
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
           ((p == PG_DSEQ2 || p == PG_DSEQ3) && !ways_on(0u, SQT_SEQ));
}

/* a button's pages: the selected track's, and those every track has; the flow's two SAVE pages, its own */
static int page_hidden(uint32_t p)
{
    return (PAGES[p].track != T_ANY && PAGES[p].track != st.v[P_KB]) || page_off(p) || p == PG_SAVE || p == PG_SAVEAS;
}

/* the setting under knob i of page p. KEYS: the selected track's. A first page of ARP or SEQ turned OFF: its ON/OFF alone.
 * RECORD on LIVE a track's steps page is no step editor: what a take asks for (LIVE_K) */
static const uint8_t LIVE_K[3][4] = {{P_DQ_LEN, P_DQ_RATE, P_DQ_UNDO, P_KY_DR_STACK},
                                     {P_SQ_LEN, P_ARP_RATE, P_SQ_UNDO, NPARAM},
                                     {P_BQ_STEPS, P_BQ_LEN, P_BQ_UNDO, NPARAM}};

static uint8_t page_knob(uint32_t p, uint32_t i)
{
    if (p == PG_KEYS)
        return (uint8_t)(P_KY_DR_HARM + 4u * st.v[P_KB] + i);
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
#endif
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    frasil_render(lr, n, (float)master_q12 * (1.0f / 4096.0f));
#if defined PLAT_HOST && !defined PLAT_WEB
    {
        uint32_t i;
        for (i = 0; i < 2u * n; i++) {
            render_hash ^= (uint32_t)lr[i];
            render_hash *= 0x100000001B3u;
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

static void scene_load(uint32_t s)
{
    scene_t sc;
    scene_get(s, &sc);
    fm.scene = (uint8_t)s;
    scene_take(&sc);
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
    up_cur[0] = up_cur[1] = up_cur[2] = -1;   /* (the scene's sound now, no preset's) */
    if (s >= NSCENES && !test_keys_stay) {   /* NEW SCENE: KEYS back to their defaults */
        eng_keys_defaults();
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
    set.saver_off = !st.v[P_SAVER];
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

/* the scenes into the store: their drone sequences first (OBJ_DSEQ), their bass sequences (OBJ_BSEQ), then the rest of
 * them (OBJ_SCENES), each record made in raw. 0: all taken */
static int scenes_save(void)
{
    uint32_t s;
    for (s = 0; s < 4u; s++)
        raw[s] = (uint8_t)(DSQ_MAGIC >> (8u * s));
    for (s = 0; s < NSCENES; s++) {
        uint8_t *o = raw + 4u + s * DSQ_SCENE;
        memcpy(o, &scn.s[s].dq_on, DSQ_SET);
        dsq_pack(&scn.s[s].dnote[0][0], o + DSQ_SET);
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
    return plat_store_save(OBJ_SCENES, raw, SCN_BYTES);
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
    set.saver_off = !st.v[P_SAVER];
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
    char say2[4 + SC_NAME], t[SC_NAME + 1];
    scene_get(s, &sc);                   /* keeps its name */
    if (!(scn.used >> s & 1u) && s >= NFACTORY && st.v[P_DR_PRESET]) {   /* an empty slot: the drone's preset's name */
        sc.name[0] = 0;
        text_cat(sc.name, DRONE_NAME[st.v[P_DR_PRESET] % (NDPRESET + 1u)]);
    }
    if (name) {
        for (i = 0; i < SC_NAME; i++)
            sc.name[i] = 0;
        for (i = 0; i < SC_NAME && name[i]; i++)
            sc.name[i] = name[i];
    }
    for (i = 0; i < NSOUND; i++)
        sc.v[i] = kept_v(i);
    scene_notes_take(&sc);
    scn.magic = SCN_MAGIC;
    scn.s[s] = sc;
    scn.used |= (uint16_t)(1u << s);
    fm.scene = (uint8_t)s;
    if (scenes_save() == 0 && store_all() == 0) {
        store_old = 0;
        sound_mark();
        say2[0] = (char)('0' + (s + 1u) / 10u);   /* "03 ABYSS" */
        say2[1] = (char)('0' + (s + 1u) % 10u);
        say2[2] = ' ';
        say2[3] = 0;
        text_cat(say2, name_text(&sc, t));
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
 * cursor along the sixteen places; K4 turns its letter (a space before A), K3 its digit (and - .); SEL (OK) stores the name (the
 * scenes written again if it changed); FX leaves it as it was stored. Meanwhile the keys and MIDI play; the other
 * buttons and the encoders wait */
static const char NM_LETTERS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ", NM_DIGITS[] = "0123456789-.";
static struct {
    uint8_t on, slot, cur, drawn;
    uint8_t flow;                        /* the SAVE flow's: SAVE stores what WHAT and INTO say, FX back (else STORE's, the
                                          * scene stored: SEL keeps the name, FX leaves it) */
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

/* ---------------------------------------------------- the SAVE flow, presets --- */
/* SAVE: LOAD | SAVE (K1, PG_SAVE); LOAD: SCENES. SAVE: WHAT and INTO (PG_SAVEAS); then the name (nm.flow); SEL goes on, FX
 * goes back a step. WHAT: 0 SCENE, 1 DRONE PRESET, 2 GLINT PRESET, 3 BASS PRESET. INTO: a slot, or for a preset 0 NEW, n
 * the player's nth (replaced). The player's presets come after the factory ones in PRESET's lists */
static struct {
    uint8_t load;                        /* K1: 1 LOAD (SAVE to begin with; kept) */
    uint8_t what, into;
} sv;

static uint8_t *up_at(uint32_t t, uint32_t i) { return t == 0u ? up.dr[i] : t == 1u ? up.gl[i] : up.bs[i]; }
static uint32_t up_bytes(uint32_t t) { return t == 0u ? sizeof up.dr[0] : t == 1u ? sizeof up.gl[0] : sizeof up.bs[0]; }
static uint32_t up_n(uint32_t t)         /* how many the track has */
{
    uint32_t n = 0;
    while (n < NUP && up_at(t, n)[0] == 'F')
        n++;
    return n;
}
static int up_in_use(uint32_t t)         /* the track's preset of the player's, as it sounds now, -1 none */
{
    int c = up_cur[t];
    return c >= 0 && eng_preset_is(up_at(t, (uint32_t)c), up_bytes(t)) == 1 ? c : -1;
}
static void up_name(uint32_t t, uint32_t i, char *o)   /* its name (SC_NAME + 1 at most) */
{
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
    if (PAGES[p].button == B_EDIT)
        return t == 1u ? PG_GLINT : t == 2u ? PG_BASS : PG_DRONE;
    for (i = 0; i < sizeof TRACK_PAGES / sizeof TRACK_PAGES[0]; i++)
        for (w = 0; w < 3u; w++)
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
    char t[SC_NAME + 25];
    uint32_t i, end = 0;
    t[0] = 0;
    if (!sv.what) {
        scene_t sc;
        scene_get(fm.scene, &sc);
        name_text(&sc, t);
        if (fm.scene >= NSCENES && st.v[P_DR_PRESET]) {
            t[0] = 0;
            text_cat(t, DRONE_NAME[st.v[P_DR_PRESET] % (NDPRESET + 1u)]);
        }
    } else {
        uint32_t k = sv.what - 1u;
        int u = up_cur[k];
        if (u >= 0)
            up_name(k, (uint32_t)u, t);
        else
            value_text(k == 0u ? P_DR_PRESET : k == 1u ? P_GL_PRESET : P_BS_MODEL,
                       st.v[k == 0u ? P_DR_PRESET : k == 1u ? P_GL_PRESET : P_BS_MODEL], t);
    }
    for (i = 0; i < SC_NAME; i++) {
        end |= !t[i];
        nm.name[i] = end ? ' ' : t[i];
    }
    nm.name[SC_NAME] = 0;
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
    char t[SC_NAME + 1];
    int32_t n = (int32_t)SC_NAME;
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
            nm.cur = (uint8_t)clamp((int32_t)nm.cur + d, 0, (int32_t)SC_NAME - 1);
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
    if ((down >> B_OCTUP & 1u) && nm.cur < SC_NAME - 1u) {
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

static int settings_old(void)             /* the settings in an earlier layout, the same (the bass's sequence none),
                                           * its bass's sequence as it kept it */
{
    int n = plat_store_load(OBJ_SETTINGS, raw, sizeof raw);
    uint32_t k, j;
    set.bq_steps = PD[P_BQ_STEPS].def;
    set.bq_trans = PD[P_BQ_TRANS].def;
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
            if (set.learn_p[j] >= FRSQ_NV && set.learn_p[j] < NPARAM - (NSOUND - FRSQ_NV))
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

static int settings_over(void)           /* (settings_old's, the bass's sequence made over: bseq_over, but FRSN's; PLAYS
                                          * as this layout has it, the arpeggio's settings and GATE their defaults) */
{
    int r = settings_old();
    if (r == 1)
        bseq_over(set.bnote, &set.bq_steps, &set.bq_len);
    if (r && r != 3) {
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
        for (i = 0; i < NSCENES; i++) {
            scene_t *d = &scn.s[i];
            if (ok) {
                memcpy(&d->dq_on, raw + 4u + i * DSQ_SCENE, DSQ_SET);
                dsq_unpack(raw + 4u + i * DSQ_SCENE + DSQ_SET, &d->dnote[0][0]);
            } else {
                dseq_none(d);
            }
        }
    }
    if (plat_store_load(OBJ_UPRE, &up, sizeof up) != (int)sizeof up || up.magic != UPR_MAGIC)
        memset(&up, 0, sizeof up);       /* (none: an earlier FRASIL's store, or its backup restored) */
    up_cur[0] = up_cur[1] = up_cur[2] = -1;
    keys_kept = plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC;
    if (!keys_kept) {
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
        st.v[P_SAVER] = !set.saver_off;  /* (an earlier record, or one before it: ON) */
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
    while (plat_midi_in(&pkt, &port))
        eng_midi(pkt);
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
        return 1;
    }
    for (i = 0; i < NLEARN; i++)         /* a learned one first */
        if (cc && set.learn_cc[i] == cc && set.learn_p[i] < NPARAM) {
            param_set(set.learn_p[i], ((int32_t)v * PD[set.learn_p[i]].max + 63) / 127);
            learned = 1;
        }
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
    if (!st.v[LFO_BASE[nx - 1u] + 2u])
        param_set((int)(LFO_BASE[nx - 1u] + 2u), 32);   /* (DEPTH: a quarter) */
    m[0] = 0;
    text_cat(m, "> LFO ");
    int_text((int32_t)nx, m + 6);
    app_say(LFO_TGT_NAME[t], m);
}

static int32_t accel(uint32_t e, int32_t d)
{
    uint32_t now = plat_ms(), dt = now - fm.enc_at[e];
    fm.enc_at[e] = now;
    return dt < 25u ? d * 4 : dt < 60u ? d * 2 : d;
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
    uint8_t on, p, v, pick, due, enc, drawn;   /* v: its place in the list; due: not yet heard */
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
/* an LFO's TARGET in the grid: four pages, in sections, what each moves together; a scene keeps LFO_TGT's numbers, this is
 * the order they are shown in. A slot: a number of LFO_TGT, TH_NONE, or a section's name (TH(n), TGT_HEAD) */
#define NG_TPAGES 4u
#define TH_NONE (-1)
#define TH(n) (-2 - (n))
static const char *const TGT_HEAD[8] = {"DRONE", "CHOIR", "PARTIALS", "BREATH", "GLINT", "GLINT II", "BASS", "GLOBAL"};
static const int8_t TGT_SLOTS[NG_TPAGES * NG_PER] = {
    0, TH(0), 1, 2, 3, 35, 4, 29, 43, TH_NONE,                                   /* OFF; TIMBRE .. WARMTH, level, pan */
    TH_NONE, TH(1), 5, 8, 9, 10, 11, 47, TH_NONE, TH_NONE,
    TH(2), 6, 12, 13, 14, 15, 16, 17, 18, TH_NONE,
    TH(3), 7, 19, 20, 21, 22, 23, 24, TH_NONE, TH_NONE,
    TH(4), 30, 45, 25, 26, 27, 28, TH_NONE, TH_NONE, TH_NONE,                    /* level, pan, its sound */
    TH(5), 48, 49, 50, 51, 36, 37, 38, TH_NONE, TH_NONE,                         /* ADSR, CHANCE, SEQ LEN, SPARKLE */
    TH(6), 34, 44, 31, 32, 33, 52, 53, 54, 55,
    TH(7), 41, 42, 39, 40, 46, TH_NONE, TH_NONE, TH_NONE, TH_NONE,
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
static uint32_t pv_shown(int p)
{
    if (!ng.on || ng.p != (uint8_t)p || ng.v > PD[p].max)
        return st.v[p];
    return PD[p].fmt == F_LTGT ? tgt_at(ng.v) : ng.v;
}

static int list_track(uint32_t p) { return p == P_DR_PRESET ? 0 : p == P_GL_PRESET ? 1 : p == P_BS_MODEL ? 2 : -1; }
static int list_param(uint32_t p)
{
    return list_track(p) >= 0 || p == P_SCALE || p == P_CHORD || p == P_EQ_PRESET || PD[p].fmt == F_LTGT;
}
static uint32_t list_n(uint32_t p)
{
    int t = list_track(p);
    return PD[p].max + 1u + (t >= 0 ? up_n((uint32_t)t) : 0u);
}
static uint32_t list_cur(uint32_t p)     /* where the setting is in its list now */
{
    int t = list_track(p), u = t >= 0 ? up_in_use((uint32_t)t) : -1;
    if (PD[p].fmt == F_LTGT)
        return tgt_pos(st.v[p]);
    return u >= 0 ? PD[p].max + 1u + (uint32_t)u : st.v[p];
}
static void list_name(uint32_t p, uint32_t i, char *o)
{
    if (i <= PD[p].max)
        value_text((int)p, (uint8_t)(PD[p].fmt == F_LTGT ? tgt_at(i) : i), o);
    else
        up_name((uint32_t)list_track(p), i - PD[p].max - 1u, o);
}
static void list_take(uint32_t p, uint32_t i)
{
    int t = list_track(p);
    if (PD[p].fmt == F_LTGT)
        i = tgt_at(i);
    if (i > PD[p].max) {
        uint32_t u = i - PD[p].max - 1u;
        eng_preset_apply(up_at((uint32_t)t, u), up_bytes((uint32_t)t));
        up_cur[t] = (int8_t)u;
        return;
    }
    if (t >= 0) {
        if (up_cur[t] >= 0 && st.v[p] == i)   /* (the same factory one again: its sound back) */
            st.v[p] = (uint8_t)(i ? i - 1u : i + 1u);
        up_cur[t] = -1;
    }
    param_set((int)p, (int32_t)i);
}

/* an LFO's TARGET, a track's preset: heard as it is turned (the choice sounds once the knob has rested), SEL keeps it, FX
 * brings back the sound as it was before the grid (gk), only those two lit, the rest wait, no closing by itself (OCT- held,
 * a preset picked unheard: OCT-'s, as ever) */
static int grid_asks(void) { return ng.on && !ng.pick && (PD[ng.p].fmt == F_LTGT || list_track(ng.p) >= 0); }
static struct {
    uint8_t p, old;                      /* the setting, its value before */
    int8_t up;                           /* the player's preset in use before (-1 none) */
    uint32_t n;                          /* the track's sound before, as a preset (0: an LFO's TARGET, its value alone) */
    uint8_t b[PRESET_BYTES];
} gk;
static void grid_keep(void)              /* the grid opened: the sound as it is, for FX */
{
    int t = list_track(ng.p);
    gk.p = ng.p;
    gk.old = st.v[ng.p];
    gk.up = t >= 0 ? up_cur[t] : -1;
    gk.n = t >= 0 ? eng_preset_capture((uint32_t)t, 0, gk.b) : 0u;
}
static void grid_back(void)              /* FX: the sound as it was */
{
    int t = list_track(gk.p);
    if (gk.n)
        eng_preset_apply(gk.b, gk.n);
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
    int pick = b_held(B_OCTDN) && !b_held(B_OCTUP) && list_track(p) >= 0;
    if (!ng.on || ng.p != (uint8_t)p) {
        if (ng.on)
            grid_close();
        ng.on = 1;
        ng.p = (uint8_t)p;
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
static uint32_t track_preset(void) { return st.glint ? P_GL_PRESET : st.bass ? P_BS_MODEL : P_DR_PRESET; }
static uint32_t track_algo(void) { return st.glint ? P_GL_BRIGHT : st.bass ? P_BS_FILTER : P_TIMBRE; }
static uint32_t macro_of(uint32_t e)
{
    return e == EN_SELECT ? P_KB : e == EN_PRESET ? track_preset()
         : e == EN_ALGO ? (b_held(B_GLO) ? P_FL_CUT : track_algo()) : NPARAM;
}

/* SELECT turned: the track, its page followed on EDIT, ENV, ARP and SEQ (the same place, the first if hidden; EDIT: the
 * first), the track large a moment; elsewhere the page stays. OCT- held: the keys change track, the page stays (OCT- a
 * hand, its step undone). A recording: not at all */
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
        if (!d)
            continue;
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
        if (b_held(B_LFO) && e >= EN_K1) {   /* LFO held: the setting under it onto an LFO, the next, or off */
            b_used(B_LFO);
            lfo_route(fm.page == PG_SCENES ? NPARAM : page_knob(fm.page, (uint32_t)(e - EN_K1)), d);
            continue;
        }
        if (fm.page == PG_SCENES && (e == EN_K1 || e == EN_SELECT)) {   /* the slots, NEW SHARD above the first:
                                                                          * SELECT as K1 (KEYS stay as they are) */
            p = clamp((fm.pick >= NSCENES ? -1 : (int32_t)fm.pick) + d, -1, NSCENES - 1);
            fm.pick = (uint8_t)(p < 0 ? SC_NEW : p);
            continue;
        }
        if (fm.page == PG_SAVE && e == EN_K1) {   /* LOAD | SAVE */
            sv.load = d < 0;
            continue;
        }
        if (fm.page == PG_SAVEAS && e == EN_K1) {   /* WHAT */
            sv.what = (uint8_t)clamp((int32_t)sv.what + d, 0, 3);
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
            if (p >= NPARAM)                 /* an empty slot */
                continue;
        }
        if (fm.page == PG_MIXER && b_held(B_OCTDN) && !b_held(B_OCTUP) && e >= EN_K1) {   /* MIXER, OCT- held: K1 .. K4
                                                                                        * each track's PAN (OCT- a
                                                                                        * hand, its step undone) */
            if (!(fm.hold_done >> B_OCTDN & 1u)) {
                fm.octave = pv.oct;
                b_used(B_OCTDN);
            }
            p = P_PN_DRONE + (int)(e - EN_K1);
            param_set(p, (int32_t)st.v[p] + accel(e, d));
            fm.touched = (uint8_t)p;
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
            fm.touched = (uint8_t)p;
            fm.touched_enc = (uint8_t)e;
            fm.touched_at = plat_ms();
            grid_turn((uint32_t)p, e, d);
            continue;
        }
        if (p >= P_KY_DR_HARM && p <= P_KY_BS_STACK && keys_turn(p, d))
            continue;
        {
            int32_t v = (int32_t)st.v[p] + (PD[p].max > 16u ? accel(e, d) : d);
            int lp = step_len_p(p), sp = len_step_p(p);
            if (!st.v[p] && d > 0 && p == P_SQ_NOTE)   /* (up from a REST: a note of the line, not the range's bottom) */
                v = step_near(seq, st.v[P_SQ_STEP], st.v[P_SQ_LEN] + 1u);
            else if (!st.v[p] && d > 0 && p == P_BQ_NOTE)
                v = step_near(bnote, st.v[P_BQ_STEP], st.v[P_BQ_STEPS] + 1u);
            param_set(p, lp >= 0 ? clamp(v, 0, st.v[lp]) : v);
            if (sp >= 0 && st.v[sp] > st.v[p])   /* (a shorter sequence: STEP back on its last) */
                param_set(sp, st.v[p]);
        }
        fm.touched = (uint8_t)p;
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
    int was = rec_any(), r = eng_record_on(st.v[P_KB], 1);
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
            eng_scene_again(&sc);
        }
    } else {                             /* nothing playing at all: its three tracks, as PLAY held; else the transport;
                                          * stopping a take ends it, RECORD off */
        int ran = sq.run || bq.run || dq.run || cnt.run;
        scene_get(fm.scene, &sc);
        eng_play_tap(&sc);
        if (ran && rec_any() && !sq.run && !bq.run && !dq.run && !cnt.run) {
            eng_record_off();
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
    if (rec_any())
        eng_record_off();
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
        } else if (fm.page == PG_MIXER) {
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

static void buttons(uint32_t btn)
{
    uint32_t down = btn & ~btn_prev, up = btn_prev & ~btn, k, now = plat_ms();
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
        } else if (k == B_OCTDN || k == B_OCTUP) {   /* at once; both: back to the start */
            if ((btn >> B_OCTDN & 1u) && (btn >> B_OCTUP & 1u))
                fm.octave = OCT_DEF;
            else
                fm.octave = (int8_t)clamp(fm.octave + (k == B_OCTUP ? 1 : -1), -3, 2);
            fm.oct_at = plat_ms() | 1u;  /* (large a moment, ui.c; undone as a hand: not shown) */
            fm.oct_v = fm.octave;
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
            param_set((int)p, !st.v[p]);
            prefs_later();
            app_say("STACK", st.v[p] ? "ON" : "OFF");
            continue;                    /* (nothing for SELECT to undo) */
        }
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

static void swell_update(uint32_t btn)
{
    eng_swell((btn >> B_ENV & 1u) && plat_ms() - fm.hold_at[B_ENV] >= HOLD_MS &&
              (fm.hold_acted >> B_ENV & 1u));   /* (ENV's hold: none once SELECT took it) */
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
static const uint8_t EDIT_GROUPS[3][3][3] = {
    {{PG_CHOEUR, PG_CHOEUR2, PG_CHOEUR3}, {PG_PARTIELS, PG_PARTIELS2, PG_PARTIELS2}, {PG_SOUFFLE, PG_SOUFFLE2, PG_SOUFFLE2}},
    {{PG_GLINT, PG_GLINT, PG_GLINT}, {PG_GLINT2, PG_GLINT2, PG_GLINT2}, {PG_GDELAY, PG_GDELAY, PG_GDELAY}},
    {{PG_BASS, PG_BASS, PG_BASS}, {PG_BASS2, PG_BASS2, PG_BASS2}, {PG_BDRIVE, PG_BDRIVE, PG_BDRIVE}},
};
static int32_t edit_group(void)          /* the group of the page on the screen, -1 none */
{
    const uint8_t (*g)[3] = EDIT_GROUPS[st.v[P_KB] % 3u];
    uint32_t i, j;
    for (i = 0; i < 3u; i++)
        for (j = 0; j < 3u; j++)
            if (g[i][j] == fm.page)
                return (int32_t)i;
    return -1;
}
static void edit_jump(uint32_t i)
{
    const uint8_t *g = EDIT_GROUPS[st.v[P_KB] % 3u][i];
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
}

static void keys_scan(void)
{
    uint32_t keys, k;
    touch_poll();
    keys = saver_keys(touch_keys());     /* (a key that woke the screen: no note) */
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
    eng_steps();                         /* (the engine's millisecond, around the MIDI) */
    midi_in();
    eng_moves();
    grid_tick();
    if (rec_any() && !test_rec_anywhere && PAGES[fm.page].button != B_SEQ && PAGES[fm.page].button != B_ARP) {   /* a take lives on the SEQ and ARP
                                                                                            * pages: leaving them ends it */
        eng_record_off();
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
    holds();
    swell_update(btn);
    if (st.engine_dirty)
        engine_push();
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
static void say_both(void) { say.red = 2; }          /* its two words alike, large, a dot between: ARP ON . SEQ OFF */

#include "ui.c"

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
    saver.on = 0;                            /* (awake: at power-on, after a restore or an update tried) */
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
 *                   page's lfo_rnd as app.c's), then the tracks' meters (drones, bass, glints, the cloud), the
 *                   transport (bits), LOFI TAPE's drag under way (how much slower: 127 is 8 %); a note 127: none;
 *                   levels 0..127
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
    scene_get(fm.scene, &sc);
    name_text(&sc, t);
    b[0] = 2u, b[1] = fm.scene, b[2] = st.dirty != 0;
    for (i = 0; i < SC_NAME; i++)
        b[3u + i] = (uint8_t)(t[i] && t[i] < 0x7F ? t[i] : ' ');
    for (i = 0; i < SC_NAME && t[i]; i++)
        ;
    if (pf_send(b, 3u + i))
        return -1;
    pf.scene = fm.scene, pf.dirty = st.dirty != 0;
    return 0;
}

/* the transport, each track playing (bits): 1 the drones (a chord latched or held, from the keys or MIDI), 2 GLINT
 * (its arpeggio going, not waiting; its sequence running), 4 the bass (its note held or latched; its arpeggio, its
 * sequence, as GLINT's) */
static uint8_t pf_playing(void)
{
    uint32_t k, dr = 0;
    int gl = (st.v[P_SQ_TYPE] == SQT_ARP && arp.n && !arp.paused) || (st.v[P_SQ_TYPE] == SQT_SEQ && sq.run);
    int bs = bass_note >= 0 || mb_n || (st.v[P_BQ_ON] == SQT_ARP && barp.n && !barp.paused) ||
             (st.v[P_BQ_ON] == SQT_SEQ && bq.run);
    for (k = 0; k < 4u; k++)
        dr |= lit[k] | mid_on[k] | sus[k];
    dr |= dq.nsnd;                       /* (the drone's sequence's notes) */
    return (uint8_t)((dr != 0) | (gl != 0) << 1 | (bs != 0) << 2);
}

static void pf_frame(void)
{
    frasil_view_t v[FR_NV];
    glint_view_t g[GLINT_NV];
    bass_view_t bv;
    uint8_t b[1u + 2u * FR_NV + 3u * GLINT_NV + 3u + 5u * NLFO + 4u + 1u + 1u], *p = b;
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
    if (fm.store_hold)                   /* a backup streaming: heard, not answered */
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
#if defined PLAT_HOST && !defined PLAT_WEB
    uint32_t t0, k, ms = plat_ms();
    static uint32_t last_ms;
#endif
    input();
    load_watch();
    saver_tick();
#if defined PLAT_HOST && !defined PLAT_WEB
    k = saver.on;
    t0 = plat_ticks();
    ui_frame();
    ui_cost.ticks[k] += plat_ticks() - t0;
    ui_cost.n[k]++;
    ui_cost.ms[k] += ms - last_ms;
    last_ms = ms;
#else
    ui_frame();
#endif
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
        *got = plat_ms() - grid.clk_at < 500u;
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
        *got = (int)up_n((uint32_t)atoi(what + 3) % 3u);
    else if (!strncmp(what, "upcur", 5))
        *got = up_cur[(uint32_t)atoi(what + 5) % 3u];
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
    else if (!strncmp(what, "scnchar", 7))   /* scncharN: the current scene's name, its character N (0: none) */
        *got = (fm.scene < NSCENES ? scn.s[fm.scene].name : NEW_SHARD.name)[atoi(what + 7) % SC_NAME];
    else if (!strcmp(what, "pick"))      /* SCENES: the slot K1 points at (16: NEW SHARD) */
        *got = fm.pick;
    else if (!strcmp(what, "stacktag"))  /* the STACK hint at the foot, as drawn last: 0 none, 1 OFF, 2 ON */
        *got = ui.stack;
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
     * arpeggio's notes (as narp and arp) */
    fprintf(f, "\"scene_t\":{\"size\":%u,\"name\":%u,\"name_len\":%u,\"v\":%u,\"nch\":%u,\"narp\":%u,\"ch\":%u,\"arp\":%u,\"bass\":%u,"
               "\"seq\":%u,\"bnote\":%u,\"bq\":%u,\"tempo\":%u,\"bq_len\":%u,\"ba\":%u,\"gate\":%u,\"nbarp\":%u,"
               "\"barp\":%u,\"bs\":%u,\"bs_space\":%u,\"gl_vib\":%u,\"dq\":%u,\"dnote\":%u,\"rec\":%u},\n",
            (unsigned)sizeof(scene_t), (unsigned)offsetof(scene_t, name), SC_NAME, (unsigned)offsetof(scene_t, v),
            (unsigned)offsetof(scene_t, nch), (unsigned)offsetof(scene_t, narp), (unsigned)offsetof(scene_t, ch),
            (unsigned)offsetof(scene_t, arp), (unsigned)offsetof(scene_t, bass), (unsigned)offsetof(scene_t, seq),
            (unsigned)offsetof(scene_t, bnote), (unsigned)offsetof(scene_t, bq_on), (unsigned)offsetof(scene_t, tempo),
            (unsigned)offsetof(scene_t, bq_len), (unsigned)offsetof(scene_t, ba_mode),
            (unsigned)offsetof(scene_t, bq_gate), (unsigned)offsetof(scene_t, nbarp), (unsigned)offsetof(scene_t, barp),
            (unsigned)offsetof(scene_t, bs_weight), (unsigned)offsetof(scene_t, bs_space), (unsigned)offsetof(scene_t, gl_vib),
            (unsigned)offsetof(scene_t, dq_on), (unsigned)offsetof(scene_t, dnote), (unsigned)SC_REC);
    fprintf(f, "\"scenes_t\":{\"size\":%u,\"magic\":%u,\"used\":%u,\"s\":%u,\"rec\":%u},\n", (unsigned)sizeof(scenes_t),
            (unsigned)offsetof(scenes_t, magic), (unsigned)offsetof(scenes_t, used), (unsigned)offsetof(scenes_t, s),
            (unsigned)SCN_BYTES);
    fprintf(f, "\"bseqs_t\":{\"size\":%u,\"magic\":0,\"s\":4,\"scene\":%u},\n", (unsigned)BSQ_BYTES, (unsigned)BSQ_SCENE);
    /* OBJ_DSEQ (3.0), the scenes' drone sequences: after its magic, each scene's DSQ_SCENE bytes, its NDQ settings as
     * scene_t has them from dq (ON, RATE, SEQ LENGTH - 1, GATE, CHANCE, TRANSPOSE), then its SC_DSEQ x DQ_NOTES notes
     * (scene_t's dnote, step by step) seven bits each, note j at bits 7j .. 7j + 6 of the bytes read as one little-endian
     * stream (dsq_pack); absent (an earlier FRASIL's store or backup): every scene's drone sequence none (dseq.none) */
    fprintf(f, "\"dseqs_t\":{\"size\":%u,\"magic\":0,\"s\":4,\"scene\":%u,\"set\":%u,\"bits\":7},\n",
            (unsigned)DSQ_BYTES, (unsigned)DSQ_SCENE, (unsigned)DSQ_SET);
    fprintf(f, "\"settings_t\":{\"size\":%u,\"magic\":%u,\"v\":%u,\"octave\":%u,\"keys\":%u,\"channel\":%u,"
               "\"scene\":%u,\"tempo\":%u,\"ch_glint\":%u,\"ch_bass\":%u,\"seq\":%u,\"metro_off\":%u,\"speaker_off\":%u,"
               "\"load\":%u,\"learn_cc\":%u,\"learn_p\":%u,\"bnote\":%u,\"bq_on\":%u,\"bq_steps\":%u,"
               "\"bq_trans\":%u,\"anim\":%u,\"clock\":%u,\"bq_len\":%u,\"ba\":%u,\"gate\":%u,\"bs\":%u,\"bs_space\":%u,\"gl_vib\":%u,"
               "\"ky\":%u,\"metro\":%u,\"metro_on\":%u,\"count_in\":%u,\"arp_snap\":%u,\"saver_off\":%u},\n",
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
            (unsigned)offsetof(settings_t, saver_off));
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
    fprintf(f, ",\"settings\":");
    js_magic(f, SET_MAGIC);
    fprintf(f, ",\"backup\":");
    js_magic(f, APP_BACKUP_MAGIC);
    fprintf(f, "},\n\"objs\":{\"settings\":%d,\"panel\":%d,\"scenes\":%d,\"bseqs\":%d,\"dseqs\":%d,\"upresets\":%d},\n\"seq\":{\"rest\":%u,\"tie\":%u,"
               "\"accent\":%u},\n",
            OBJ_SETTINGS, OBJ_PANEL, OBJ_SCENES, OBJ_BSEQ, OBJ_DSEQ, OBJ_UPRE, SQ_REST, SQ_TIE, SQ_ACC);
    /* OBJ_UPRE (3.0), the player's presets: after its magic NUP of the drone's, of GLINT's, of the bass's, each its bytes
     * (the engine's "FRp": "FRp", version, track, n, name of SC_NAME, n pairs of setting and value; its first byte 0: none);
     * absent (an earlier FRASIL's store or backup): none */
    fprintf(f, "\"upresets_t\":{\"size\":%u,\"magic\":0,\"slots\":%u,\"at\":[%u,%u,%u],\"bytes\":[%u,%u,%u]},\n",
            (unsigned)sizeof(upresets_t), NUP, (unsigned)offsetof(upresets_t, dr), (unsigned)offsetof(upresets_t, gl),
            (unsigned)offsetof(upresets_t, bs), (unsigned)sizeof up.dr[0], (unsigned)sizeof up.gl[0], (unsigned)sizeof up.bs[0]);
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
     * FRSy (this layout to gl_vib: the drone's sequence none, dseq.none: same), FRSx (that but its last byte, AUTO VIB: 0, its bass kept: same), FRSw (and SPACE before it, the same), FRSu (grid: its bass's sequence on
     * the one grid already, this layout's to bq_len), FRSt, FRSr, FRSp, FRSn, FRSm,
     * FRSk, FRSi, then FRSh. Every one, as every old_bseqs and old_settings_over: PLAYS' 1 is 2 here (SEQUENCE on was 1),
     * the bass's arpeggio none, its settings and GATE their defaults; and every one: the drone's sequence none (dseq.none) */
    fprintf(f, "]},\n\"old_shards\":[{\"magic\":");
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
     * FRSH, FRSG */
    fprintf(f, ",\n\"old_settings_over\":[{\"magic\":");
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
    ID(P_BQ_NEW), ID(P_BQ_UNDO), ID(P_SQ_NLEN), ID(P_BS_WEIGHT), ID(P_BS_PUNCH), ID(P_BS_BITE), ID(P_BS_SPACE), ID(P_GL_VIB_AUTO), ID(P_DQ_ON), ID(P_DQ_RATE), ID(P_DQ_LEN), ID(P_DQ_GATE), ID(P_DQ_CHANCE), ID(P_DQ_TRANS), ID(P_DQ_REC), ID(P_DQ_CLEAR), ID(P_DQ_STEP), ID(P_DQ_NOTES), ID(P_DQ_NLEN), ID(P_DQ_UNDO), ID(P_GA_LATCH), ID(P_BA_LATCH), ID(P_KY_DR_HARM), ID(P_KY_GL_HARM), ID(P_KY_BS_HARM), ID(P_METRO), ID(P_COUNT_IN), ID(P_ARP_SNAP), ID(P_SAVER), ID(PG_BARP), ID(PG_BSEQ3), ID(PG_BDRIVE), ID(PG_CSEND), ID(P_SQ_REC), ID(PG_ARP2), ID(PG_BARP2), ID(PG_DSEQ), ID(PG_DSEQ2), ID(PG_DSEQ3), ID(PG_NOARP), ID(PG_KEYS), ID(PG_RECORD), ID(PG_SAVE), ID(PG_SAVEAS);
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
    set_bass_old(raw + 4u + FRSL_NV + offsetof(settings_t, bnote) - offsetof(settings_t, octave));
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
    set_bass_old(raw + 4u + FRSO_NV + offsetof(settings_t, bnote) - offsetof(settings_t, octave));
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
        if (*l >= NSOUND && *l < NPARAM)
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
    glint_preset(sc.v, sc.v[P_GL_PRESET]);
    arp_defaults(sc.v);
    later_defaults(sc.v, P_BS_DRIVE);
    fm.scene = 0;
    fm.pick = 0;
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
#ifndef PLAT_WEB
    if (!strcmp(cmd, "saveridle")) {     /* saveridle MS: the screensaver after MS untouched (0: its ten minutes) */
        saver.after = (uint32_t)atoi(a);
        return 1;
    }
    if (!strcmp(cmd, "savermark")) {     /* savermark: where the logo is now (expect saver_dist: how far since) */
        saver_mark[0] = saver.x;
        saver_mark[1] = saver.y;
        return 1;
    }
    if (!strcmp(cmd, "uicost")) {        /* uicost: the UI's frames since the last, awake and asleep: how many, their
                                          * time each (the host's CPU), the pixels the saver's sent each */
        uint32_t k;
        for (k = 0; k < 2u; k++)
            printf("  uicost %s: %u frames in %u ms, %.1f us each%s", k ? "saver" : "awake", (unsigned)ui_cost.n[k],
                   (unsigned)ui_cost.ms[k], ui_cost.n[k] ? (double)ui_cost.ticks[k] / PLAT_TICKS_US / ui_cost.n[k] : 0.0,
                   k ? "" : "\n");
        printf(", %u steps drawn, %.0f pixels each\n", (unsigned)ui_cost.draws,
               ui_cost.draws ? (double)ui_cost.px / ui_cost.draws : 0.0);
        memset(&ui_cost, 0, sizeof ui_cost);
        return 1;
    }
#endif
    if (!strcmp(cmd, "testscene")) {     /* testscene: the tests' own scene (test_scene), as if slot 01 were it */
        test_scene();
        return 1;
    }
#ifndef PLAT_WEB
    if (!strcmp(cmd, "renderhash")) {    /* renderhash: every sample rendered since the start, hashed (tests/engine.txt:
                                          * the engine's output, bit for bit) */
        printf("  renderhash %016llx %llu frames\n", (unsigned long long)render_hash, (unsigned long long)render_frames);
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

