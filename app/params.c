/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's parameters: each one a byte (0 .. its max), how the screen writes it, which page has it, and what the
 * engine gets from them all (params_to_engine). A scene is the first NSOUND of them.
 * Part of the unity build (app.c includes it). */

enum {
    P_CHOEUR, P_PARTIELS, P_SOUFFLE, P_CHALEUR,
    P_DESACCORD, P_FILTRE, P_PENTE, P_IMPAIRS,
    P_ETIREMENT, P_RESONANCE, P_COULEUR, P_ECART,
    P_ATTAQUE, P_RELACHE, P_PRESSION, P_VELOCITE,
    P_MOUVEMENT, P_VITESSE, P_MAREE, P_PERIODE,
    P_TAILLE, P_DUREE, P_TEINTE, P_SHIMMER,
    P_DIFFUSION, P_MODULATION, P_PREDELAI, P_INTERVALLE,
    P_COUPEBAS, P_LARGEUR, P_ACCORD,
    P_TIMBRE, P_ESPACE,
    P_ROOT, P_SCALE, P_CHORD, P_VOICING, /* the harmony (harmony.c) */
    P_GL_PRESET, P_GL_RATIO, P_GL_BRIGHT, P_GL_DECAY, P_GL_LEVEL, P_GL_OCTAVE, P_GL_SPACE, P_GL_RELEASE,   /* GLINT
                                          * (RELEASE where SCATTER was: an earlier layout's made over, app.c
                                          * sound_over) */
    P_ARP_MODE, P_ARP_RATE, P_ARP_RANGE, P_ARP_CHANCE,   /* GLINT's arpeggiator */
    P_GL_ATTACK, P_GL_PING,              /* GLINT, later additions */
    P_GD_TIME, P_GD_FB, P_GD_MODE, P_GD_MIX,   /* GLINT's delay */
    P_BS_DRIVE, P_BS_FILTER, P_BS_GLIDE, P_BS_LEVEL, P_BS_OCTAVE, P_BS_ATTACK, P_BS_RELEASE, P_BS_MODEL,   /* BASS */
    P_GL_VIBRATO,                        /* GLINT: the pressure's wiggle on the pitch */
    P_BS_RESO,                           /* BASS, later: its filter's resonance */
    P_PUMP_RATE, P_PUMP_DRONE, P_PUMP_BASS, P_PUMP_GLINT,   /* PUMP: the beat's duck, on each track */
    P_MS_THRESH, P_MS_RATIO, P_MS_RELEASE, P_MS_GAIN,       /* MASTERING: the compressor */
    P_MX_DRONE, P_MX_BASS, P_MX_GLINT, P_MX_CLOUD,          /* MIXER: each track's level, and the cloud's return */
    P_SH_TONE,                           /* CLOUD: the shimmer's brightness */
    P_PUMP_REL, P_PUMP_ATT, P_PUMP_OFF,  /* PUMP II: how it comes back, how it goes down, where in the period */
    P_CH_SHAPE, P_CH_INTERVAL, P_CH_RESO, P_CH_TYPE,    /* CHOIR II: the saws' shape, the centre's interval; the filter */
    P_PA_EVOLVE, P_PA_BEAT, P_PA_FORMANT, P_PA_PEAK,    /* PARTIALS II: how they wander, in level and in tune; a formant */
    P_BR_GRAIN, P_BR_PITCH, P_BR_TONE, P_BR_AIR,        /* BREATH II: the noise and the resonators' octave; the air */
    P_GL_SUSTAIN,                        /* GLINT: the level held while the key is down */
    P_L1_SHAPE, P_L1_RATE, P_L1_DEPTH, P_L1_POL, P_L1_T1, P_L1_T2, P_L1_T3, P_L1_T4,   /* LFO 1, and its four targets */
    P_L2_SHAPE, P_L2_RATE, P_L2_DEPTH, P_L2_POL, P_L2_T1, P_L2_T2, P_L2_T3, P_L2_T4,   /* LFO 2 */
    P_SQ_LEN, P_SQ_GATE, P_SQ_TRANS,     /* GLINT SEQ: its length, its notes', its transposition (its steps: seq[]) */
    P_DR_PRESET, P_DR_FULL,              /* the drone's preset (0: CUSTOM), its BODY (DRONE PARAMS) */
    P_L3_SHAPE, P_L3_RATE, P_L3_DEPTH, P_L3_POL, P_L3_T1, P_L3_T2, P_L3_T3, P_L3_T4,   /* LFO 3 */
    P_SQ_TYPE,                           /* GLINT's sequence: OFF, the arpeggiator, the sequencer */
    P_GX_SPARK, P_GX_BLUR, P_GX_SINK,    /* GENERATIVE FX (SWELL beside them, not kept): glints by themselves, the
                                          * drone into the cloud, everything under */
    P_BS_DECAY, P_BS_SUSTAIN, P_BS_PULSE,   /* BASS III's DECAY and SUSTAIN (between its ATTACK and RELEASE), BASS II's
                                             * PULSE: the note let go and played again on the grid */
    NSOUND,                              /* a scene ends here; the rest are settings that pages show */
    P_KEYS = NSOUND,
    P_CANAL,
    P_TEMPO,                             /* the arpeggiator's, when no MIDI clock comes in */
    P_FREEZE,                            /* the cloud held (not kept: the FM-1 starts thawed) */
    P_CH_GLINT, P_CH_BASS,               /* GLINT's and BASS's MIDI channels (P_CANAL: the drones') */
    P_SQ_STEP, P_SQ_NOTE, P_SQ_ACC, P_SQ_REC, P_SQ_CLEAR,   /* GLINT SEQ's editing: the step on K1, its note and
                                                             * accent (seq[]'s, shown), RECORD, CLEAR (not kept) */
    P_SWELL,                             /* GENERATIVE FX's K1: every drone voice swelled, as far as it is turned
                                          * (not kept) */
    P_KB,                                /* SELECT's: what the keys play, DRONE, GLINT, BASS (not kept) */
    P_MORPH, P_LOAD,                     /* SCENES' K2, K3: how a scene comes in (LOAD kept with the settings;
                                          * MORPH on CUT at every start) */
    P_SC_GONE,                           /* (SCENES' UNDO, gone: its number kept, so that the ones after it stay as
                                          * the settings' MIDI LEARN and the River's backups have them; no page) */
    P_SQ_NEW, P_SQ_UNDO,                 /* GLINT SEQ III's K3, K4: a new take, the last take or CLEAR undone */
    P_BQ_STEP, P_BQ_NOTE, P_BQ_LEN, P_BQ_REC,   /* BASS SEQ: the step on K1, its note and its length (the bass's
                                                 * sequence's, shown), RECORD (not kept) */
    P_BQ_STEPS, P_BQ_ON, P_BQ_CLEAR, P_BQ_TRANS,   /* BASS SEQ II: the steps it plays, SEQUENCE, CLEAR (not kept),
                                                    * TRANSPOSE: kept with the settings, not in a scene */
    NPARAM
};

/* how a value reads */
enum { F_PCT, F_ATT, F_REL, F_DECAY, F_PREDELAY, F_HZ, F_TAU, F_PERIOD, F_INTERVAL, F_CENTS, F_CHANNEL, F_ROOT, F_SCALE,
       F_CHORD, F_VOICING, F_KEYS, F_GPRESET, F_RATIO, F_OCTAVE, F_GDECAY, F_ARPMODE, F_ARPRATE, F_RANGE, F_BPM, F_GATTACK, F_DTIME, F_DMODE,
       F_GLIDE, F_BOCTAVE, F_BATT, F_BREL, F_ONOFF, F_PCTOFF, F_HZBASS, F_BMODEL, F_TRKCH, F_PUMP, F_THRESH, F_CRATIO,
       F_CREL, F_DBGAIN, F_LEVEL, F_PATT, F_SHAPE, F_CINT, F_HARM, F_BPITCH, F_SIGNED, F_CTYPE, F_LSHAPE, F_LRATE, F_LPOL,
       F_LTGT, F_SQLEN, F_SQNOTE, F_SQREC, F_SQCLEAR, F_SQTRANS, F_DPRESET, F_SQTYPE, F_KB, F_MORPH, F_LOAD, F_UNDO, F_NEWTAKE,
       F_BQNOTE, F_BQLEN, F_BDEC, F_BPULSE, F_GREL };

typedef struct {
    const char *name;
    uint8_t max, def, fmt;
} pdesc_t;

#define NLFO_TGT 41u                     /* an LFO TARGET's choices: OFF and the settings (LFO_TGT, below) */
#define NDRONE 32u                       /* the settings a drone preset sets (DRONE_P, below) */
#define NDPRESET 24u
#define NPULSE 7u                        /* PULSE's choices: OFF, then its divisions (PULSE_TICKS, below) */
static const pdesc_t PD[NPARAM] = {
    [P_CHOEUR] = {"CHOIR", 127, 76, F_PCT},
    [P_PARTIELS] = {"PARTIALS", 127, 89, F_PCT},
    [P_SOUFFLE] = {"BREATH", 127, 32, F_PCT},
    [P_CHALEUR] = {"WARMTH", 127, 19, F_PCT},
    [P_DESACCORD] = {"DETUNE", 127, 51, F_PCT},
    [P_FILTRE] = {"FILTER", 127, 44, F_PCT},
    [P_PENTE] = {"TILT", 127, 57, F_PCT},
    [P_IMPAIRS] = {"ODD", 127, 25, F_PCT},
    [P_ETIREMENT] = {"STRETCH", 127, 0, F_PCT},
    [P_RESONANCE] = {"RESONANCE", 127, 70, F_PCT},
    [P_COULEUR] = {"COLOR", 127, 0, F_PCT},
    [P_ECART] = {"SPREAD", 127, 89, F_PCT},
    [P_ATTAQUE] = {"ATTACK", 127, 89, F_ATT},
    [P_RELACHE] = {"RELEASE", 127, 83, F_REL},
    [P_PRESSION] = {"PRESSURE", 127, 76, F_PCT},
    [P_VELOCITE] = {"VELOCITY", 127, 38, F_PCT},
    [P_MOUVEMENT] = {"MOTION", 127, 44, F_PCT},
    [P_VITESSE] = {"RATE", 127, 51, F_TAU},
    [P_MAREE] = {"TIDE", 127, 25, F_PCT},
    [P_PERIODE] = {"PERIOD", 127, 38, F_PERIOD},
    [P_TAILLE] = {"SIZE", 127, 95, F_PCT},
    [P_DUREE] = {"DECAY", 127, 79, F_DECAY},
    [P_TEINTE] = {"TONE", 127, 64, F_PCT},
    [P_SHIMMER] = {"SHIMMER", 127, 0, F_PCT},
    [P_DIFFUSION] = {"DIFFUSION", 127, 95, F_PCT},
    [P_MODULATION] = {"MODULATION", 127, 64, F_PCT},
    [P_PREDELAI] = {"PRE-DELAY", 127, 13, F_PREDELAY},
    [P_INTERVALLE] = {"INTERVAL", 4, 0, F_INTERVAL},
    [P_COUPEBAS] = {"LOW CUT", 127, 25, F_HZ},
    [P_LARGEUR] = {"WIDTH", 127, 127, F_PCT},
    [P_ACCORD] = {"TUNE", 100, 50, F_CENTS},
    [P_TIMBRE] = {"TIMBRE", 127, 64, F_PCT},
    [P_ESPACE] = {"SPACE", 127, 57, F_PCT},
    [P_ROOT] = {"ROOT", 11, 2, F_ROOT},
    [P_SCALE] = {"SCALE", 11, 0, F_SCALE},
    [P_CHORD] = {"CHORD", 14, 0, F_CHORD},
    [P_VOICING] = {"VOICING", 2, 0, F_VOICING},
    [P_GL_PRESET] = {"PRESET", 21, 8, F_GPRESET},
    [P_GL_RATIO] = {"RATIO", 12, 6, F_RATIO},
    [P_GL_BRIGHT] = {"BRIGHT", 127, 70, F_PCT},
    [P_GL_DECAY] = {"DECAY", 127, 70, F_GDECAY},
    [P_GL_LEVEL] = {"LEVEL", 127, 90, F_PCT},
    [P_GL_OCTAVE] = {"OCTAVE", 3, 2, F_OCTAVE},
    [P_GL_SPACE] = {"SPACE", 127, 90, F_PCT},
    [P_GL_RELEASE] = {"RELEASE", 127, 76, F_GREL},     /* 20 ms .. 6 s, once let go (600 MS: as DECAY's default
                                                        * let it go before) */
    [P_GL_ATTACK] = {"ATTACK", 127, 0, F_GATTACK},
    [P_GL_PING] = {"PING", 127, 68, F_PCT},
    [P_GD_TIME] = {"TIME", 23, 19, F_DTIME},           /* 1/32 .. 1/2 mono, then the same ping-pong */
    [P_GD_FB] = {"FEEDBACK", 127, 50, F_PCT},
    [P_GD_MODE] = {"MODE", 1, 1, F_DMODE},             /* (kept for the store; TIME says it now) */
    [P_GD_MIX] = {"MIX", 127, 40, F_PCT},
    [P_ARP_MODE] = {"MODE", 4, 0, F_ARPMODE},          /* UP, DOWN, UPDOWN, RANDOM, DRIFT */
    [P_ARP_RATE] = {"RATE", 9, 5, F_ARPRATE},
    [P_ARP_RANGE] = {"RANGE", 2, 1, F_RANGE},
    [P_ARP_CHANCE] = {"CHANCE", 127, 127, F_PCT},
    [P_BS_DRIVE] = {"DRIVE", 127, 20, F_PCT},
    [P_BS_FILTER] = {"FILTER", 127, 80, F_HZBASS},
    [P_BS_GLIDE] = {"GLIDE", 127, 20, F_GLIDE},
    [P_BS_LEVEL] = {"LEVEL", 127, 90, F_PCT},
    [P_BS_OCTAVE] = {"OCTAVE", 2, 1, F_BOCTAVE},   /* -2 .. 0 under the keys */
    [P_BS_ATTACK] = {"ATTACK", 127, 30, F_BATT},
    [P_BS_RELEASE] = {"RELEASE", 127, 50, F_BREL},
    [P_BS_DECAY] = {"DECAY", 127, 64, F_BDEC},         /* 10 ms .. 10 s, from the top down to SUSTAIN */
    [P_BS_SUSTAIN] = {"SUSTAIN", 127, 127, F_PCT},     /* the level held (100 %: no decay, the bass as it was) */
    [P_BS_PULSE] = {"PULSE", NPULSE - 1u, 0, F_BPULSE},   /* OFF, 1/8 BEAT .. 4 BEATS */
    [P_BS_MODEL] = {"MODEL", BASS_NMODEL - 1, 0, F_BMODEL},
    [P_BS_RESO] = {"RESONANCE", 127, 20, F_PCT},
    [P_PUMP_RATE] = {"RATE", 6, 0, F_PUMP},
    [P_PUMP_DRONE] = {"DRONE", 127, 90, F_PCT},
    [P_PUMP_BASS] = {"BASS", 127, 60, F_PCT},
    [P_PUMP_GLINT] = {"GLINT", 127, 40, F_PCT},
    [P_MS_THRESH] = {"THRESHOLD", 40, 28, F_THRESH},   /* -40 .. 0 dB */
    [P_MS_RATIO] = {"RATIO", 7, 0, F_CRATIO},
    [P_MS_RELEASE] = {"RELEASE", 127, 62, F_CREL},
    [P_MS_GAIN] = {"GAIN", 18, 0, F_DBGAIN},           /* 0 .. +18 dB */
    [P_MX_DRONE] = {"DRONE", 127, 100, F_LEVEL},       /* 100: as it is (unity), up to 127 */
    [P_MX_BASS] = {"BASS", 127, 100, F_LEVEL},
    [P_MX_GLINT] = {"GLINT", 127, 100, F_LEVEL},
    [P_MX_CLOUD] = {"CLOUD", 127, 100, F_LEVEL},
    [P_SH_TONE] = {"BRIGHT", 127, 57, F_PCT},          /* the shimmer: ~2.1 kHz by default, softer than it was */
    [P_PUMP_REL] = {"RELEASE", 127, 127, F_PCT},       /* 10 .. 100 % of the period (100: as it was) */
    [P_PUMP_ATT] = {"ATTACK", 127, 36, F_PATT},        /* 1 .. 300 ms (5 ms by default) */
    [P_PUMP_OFF] = {"OFFSET", 127, 0, F_PCT},          /* the duck's place in the period, 0 .. 100 % */
    [P_CH_SHAPE] = {"SHAPE", 127, 0, F_SHAPE},         /* SAW .. SQUARE (the middle) .. a narrow PULSE */
    [P_CH_INTERVAL] = {"INTERVAL", 3, 0, F_CINT},      /* the centre saw: UNISON, -12, +7, +12 */
    [P_CH_RESO] = {"RESONANCE", 127, 0, F_PCT},
    [P_CH_TYPE] = {"TYPE", 2, 0, F_CTYPE},             /* the filter: LOW, BAND, HIGH */
    [P_PA_EVOLVE] = {"EVOLVE", 127, 0, F_PCT},         /* 0: as MOTION has them */
    [P_PA_BEAT] = {"BEAT", 127, 0, F_PCT},
    [P_PA_FORMANT] = {"FORMANT", 127, 85, F_HARM},     /* the 1st .. 8th harmonic (the 4th by default) */
    [P_PA_PEAK] = {"PEAK", 127, 0, F_PCT},
    [P_BR_GRAIN] = {"GRAIN", 127, 0, F_PCT},
    [P_BR_PITCH] = {"PITCH", 4, 1, F_BPITCH},          /* -12, UNISON, +12, +19, +24 */
    [P_BR_TONE] = {"TONE", 127, 64, F_SIGNED},         /* 64: the noise white */
    [P_BR_AIR] = {"AIR", 127, 0, F_PCT},
    [P_GL_SUSTAIN] = {"SUSTAIN", 127, 0, F_PCT},
    [P_L1_SHAPE] = {"SHAPE", 6, 0, F_LSHAPE},
    [P_L1_RATE] = {"RATE", 75, 25, F_LRATE},           /* free (10 s by default), then on the grid */
    [P_L1_DEPTH] = {"DEPTH", 127, 0, F_PCT},           /* 0: the LFO off */
    [P_L1_POL] = {"POLARITY", 2, 0, F_LPOL},
    [P_L1_T1] = {"TARGET 1", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L1_T2] = {"TARGET 2", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L1_T3] = {"TARGET 3", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L1_T4] = {"TARGET 4", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L2_SHAPE] = {"SHAPE", 6, 0, F_LSHAPE},
    [P_L2_RATE] = {"RATE", 75, 25, F_LRATE},
    [P_L2_DEPTH] = {"DEPTH", 127, 0, F_PCT},
    [P_L2_POL] = {"POLARITY", 2, 0, F_LPOL},
    [P_L2_T1] = {"TARGET 1", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L2_T2] = {"TARGET 2", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L2_T3] = {"TARGET 3", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L2_T4] = {"TARGET 4", NLFO_TGT - 1u, 0, F_LTGT},
    [P_SQ_TYPE] = {"SEQ", 2, 0, F_SQTYPE},       /* GLINT I's K2: OFF, ARP, SEQ (TYPE till 1.1) */
    [P_GX_SPARK] = {"SPARKLE", 127, 0, F_PCT},
    [P_GX_BLUR] = {"BLUR", 127, 0, F_PCT},
    [P_GX_SINK] = {"SINK", 127, 0, F_PCT},
    [P_L3_SHAPE] = {"SHAPE", 6, 0, F_LSHAPE},
    [P_L3_RATE] = {"RATE", 75, 25, F_LRATE},
    [P_L3_DEPTH] = {"DEPTH", 127, 0, F_PCT},
    [P_L3_POL] = {"POLARITY", 2, 0, F_LPOL},
    [P_L3_T1] = {"TARGET 1", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L3_T2] = {"TARGET 2", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L3_T3] = {"TARGET 3", NLFO_TGT - 1u, 0, F_LTGT},
    [P_L3_T4] = {"TARGET 4", NLFO_TGT - 1u, 0, F_LTGT},
    [P_SQ_LEN] = {"SEQ LENGTH", 31, 15, F_SQLEN},      /* 1 .. 32 steps */
    [P_SQ_GATE] = {"GATE", 127, 64, F_PCT},            /* a note's length, 5 .. 100 % of its step */
    [P_SQ_TRANS] = {"TRANSPOSE", 28, 14, F_SQTRANS},   /* -14 .. +14 steps of the scale */
    [P_SQ_STEP] = {"STEP", 31, 0, F_SQLEN},
    [P_SQ_NOTE] = {"NOTE", 127, 0, F_SQNOTE},          /* REST, a note, TIE */
    [P_SQ_ACC] = {"ACCENT", 1, 0, F_ONOFF},
    [P_SQ_REC] = {"RECORD", 2, 0, F_SQREC},            /* OFF (the keys transpose), STEP, LIVE */
    [P_SQ_CLEAR] = {"CLEAR", 2, 0, F_SQCLEAR},         /* turned twice: the steps cleared */
    [P_DR_PRESET] = {"PRESET", NDPRESET, 0, F_DPRESET},   /* PRESETS': CUSTOM, then the 24 */
    [P_DR_FULL] = {"BODY", 127, 127, F_PCT},           /* 100 %: the drone as its settings have it; down, thinner */
    [P_SWELL] = {"SWELL", 127, 0, F_PCT},
    [P_KB] = {"KEYS", 2, 0, F_KB},                     /* what they play */
    [P_MORPH] = {"MORPH", 8, 0, F_MORPH},              /* CUT, 1 S .. 2:00: the sound glides to the scene's */
    [P_LOAD] = {"LOAD", 1, 0, F_LOAD},                 /* SND+NOTES: the sound and its notes; SND ONLY: what plays goes on */
    [P_SC_GONE] = {"-", 0, 0, F_UNDO},                 /* (nothing to turn: a CC learned on UNDO turns nothing) */
    [P_SQ_NEW] = {"NEW TAKE", 2, 0, F_NEWTAKE},
    [P_SQ_UNDO] = {"UNDO TAKE", 2, 0, F_UNDO},
    [P_BQ_STEP] = {"STEP", 15, 0, F_SQLEN},
    [P_BQ_NOTE] = {"NOTE", 127, 0, F_BQNOTE},          /* CUT, a note, HOLD */
    [P_BQ_LEN] = {"STEP LENGTH", 12, 0, F_BQLEN},      /* a step's: 1/16 .. 16 BARS (BQ_TICKS) */
    [P_BQ_REC] = {"RECORD", 2, 0, F_SQREC},            /* OFF (the keys transpose), STEP, LIVE */
    [P_BQ_STEPS] = {"SEQ STEPS", 15, 15, F_SQLEN},     /* 1 .. 16 steps */
    [P_BQ_ON] = {"SEQUENCE", 1, 0, F_ONOFF},
    [P_BQ_CLEAR] = {"CLEAR", 2, 0, F_SQCLEAR},         /* turned twice: every step a CUT of a sixteenth */
    [P_BQ_TRANS] = {"TRANSPOSE", 28, 14, F_SQTRANS},   /* -14 .. +14 steps of the scale */
    [P_GL_VIBRATO] = {"VIBRATO", 127, 50, F_PCTOFF},
    [P_KEYS] = {"KEY MODE", 1, 0, F_KEYS},
    [P_CANAL] = {"DRONE CH", 17, 1, F_TRKCH},   /* a track's MIDI channel: OFF, 1 .. 16, OMNI */
    [P_CH_GLINT] = {"GLINT CH", 17, 2, F_TRKCH},
    [P_CH_BASS] = {"BASS CH", 17, 3, F_TRKCH},
    [P_TEMPO] = {"TEMPO", 210, 60, F_BPM},   /* 30 .. 240 BPM */
    [P_FREEZE] = {"FREEZE", 1, 0, F_ONOFF},
};

static const float INTERVALS[5] = {12.0f, 7.0f, 5.0f, 19.0f, -12.0f};
static const float COMP_RATIO[8] = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 10.0f, 20.0f};   /* the first: OFF */
static const char *const COMP_RATIO_NAME[8] = {"OFF", "1.5:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1"};
/* PUMP's rates, in MIDI clocks (24 a beat, 96 a bar) */
static const uint16_t PUMP_TICKS[7] = {0, 384, 192, 96, 48, 24, 12};
static const char *const PUMP_NAME[7] = {"OFF", "4 BARS", "2 BARS", "1 BAR", "1/2", "1/4", "1/8"};
/* BASS II's PULSE: the bass's note let go and played again every this many MIDI clocks, an eighth of a beat .. four
 * beats (app.c pulse_run) */
static const uint8_t PULSE_TICKS[NPULSE] = {0, 3, 6, 12, 24, 48, 96};
static const char *const PULSE_NAME[NPULSE] = {"OFF", "1/8 BEAT", "1/4 BEAT", "1/2 BEAT", "1 BEAT", "2 BEATS",
                                               "4 BEATS"};

/* the pages: a page button, and the four parameters K1 .. K4 turn; a button with more than one page steps
 * through them */
enum { PG_SON, PG_DRONE, PG_CHOEUR, PG_CHOEUR2, PG_PARTIELS, PG_PARTIELS2, PG_SOUFFLE, PG_SOUFFLE2, PG_GLINT, PG_GLINT2, PG_GLINT3, PG_GDELAY, PG_ARP, PG_SEQ, PG_SEQ2, PG_SEQ3, PG_BASS,
       PG_BASS2,
       PG_BASS3, PG_FORME, PG_MOUVEMENT, PG_LFO1, PG_LFO1T, PG_LFO2, PG_LFO2T, PG_LFO3, PG_LFO3T, PG_HARMONY, PG_GENFX, PG_NUAGE, PG_NUAGE2, PG_NUAGE3, PG_PUMP, PG_PUMP2, PG_GLOBAL, PG_GLOBAL2,
       PG_MASTER, PG_MIXER, PG_SCENES,
       PG_BSEQ, PG_BSEQ2,                /* the bass's sequence: SEQ's after BASS III (app.c page_button), numbered last
                                          * (the others' numbers kept) */
       NPAGES };
typedef struct {
    const char *name;
    uint8_t button;
    uint8_t p[4];
} page_t;
static const page_t PAGES[NPAGES] = {
    [PG_SON] = {"SOUND", B_HOME, {P_CHOEUR, P_PARTIELS, P_SOUFFLE, P_CHALEUR}},
    [PG_DRONE] = {"DRONE PARAMS", B_EDIT, {P_TIMBRE, P_ESPACE, P_MOUVEMENT, P_DR_FULL}},   /* the whole drone */
    [PG_CHOEUR] = {"CHOIR", B_EDIT, {P_CHOEUR, P_DESACCORD, P_FILTRE, P_CHALEUR}},
    [PG_CHOEUR2] = {"CHOIR II", B_EDIT, {P_CH_SHAPE, P_CH_INTERVAL, P_CH_RESO, P_CH_TYPE}},   /* the source, the filter */
    [PG_PARTIELS] = {"PARTIALS", B_EDIT, {P_PARTIELS, P_PENTE, P_IMPAIRS, P_ETIREMENT}},
    [PG_PARTIELS2] = {"PARTIALS II", B_EDIT, {P_PA_EVOLVE, P_PA_BEAT, P_PA_FORMANT, P_PA_PEAK}},
    [PG_SOUFFLE] = {"BREATH", B_EDIT, {P_SOUFFLE, P_RESONANCE, P_COULEUR, P_ECART}},
    [PG_SOUFFLE2] = {"BREATH II", B_EDIT, {P_BR_GRAIN, P_BR_PITCH, P_BR_TONE, P_BR_AIR}},
    [PG_GLINT] = {"GLINT I", B_ARP, {P_GL_PRESET, P_SQ_TYPE, P_GL_LEVEL, P_GL_BRIGHT}},
    [PG_GLINT2] = {"GLINT II", B_ARP, {P_GL_OCTAVE, P_GL_RATIO, P_GL_PING, P_GL_VIBRATO}},
    [PG_GLINT3] = {"GLINT ADSR", B_ARP, {P_GL_ATTACK, P_GL_DECAY, P_GL_SUSTAIN, P_GL_RELEASE}},   /* (ui.c adsr_scope) */
    [PG_GDELAY] = {"GLINT DELAY", B_ARP, {P_GD_TIME, P_GD_FB, P_GD_MIX, P_GL_SPACE}},   /* and the reverb */
    [PG_ARP] = {"GLINT ARP", B_ARP, {P_ARP_MODE, P_ARP_RATE, P_ARP_RANGE, P_ARP_CHANCE}},
    [PG_SEQ] = {"GLINT SEQ", B_ARP, {P_SQ_STEP, P_SQ_NOTE, P_SQ_ACC, P_SQ_REC}},
    [PG_SEQ2] = {"GLINT SEQ II", B_ARP, {P_SQ_LEN, P_ARP_RATE, P_SQ_GATE, P_ARP_CHANCE}},
    [PG_SEQ3] = {"GLINT SEQ III", B_ARP, {P_SQ_TRANS, P_SQ_CLEAR, P_SQ_NEW, P_SQ_UNDO}},
    [PG_BASS] = {"BASS", B_SEQ, {P_BS_MODEL, P_BS_DRIVE, P_BS_FILTER, P_BS_RESO}},
    [PG_BASS2] = {"BASS II", B_SEQ, {P_BS_LEVEL, P_BS_OCTAVE, P_BS_GLIDE, P_BS_PULSE}},
    [PG_BASS3] = {"BASS ADSR", B_SEQ, {P_BS_ATTACK, P_BS_DECAY, P_BS_SUSTAIN, P_BS_RELEASE}},   /* (ui.c adsr_scope) */
    [PG_BSEQ] = {"BASS SEQ", B_SEQ, {P_BQ_STEP, P_BQ_NOTE, P_BQ_LEN, P_BQ_REC}},
    [PG_BSEQ2] = {"BASS SEQ II", B_SEQ, {P_BQ_STEPS, P_BQ_ON, P_BQ_CLEAR, P_BQ_TRANS}},
    [PG_FORME] = {"DRONE SHAPE", B_ENV, {P_ATTAQUE, P_RELACHE, P_PRESSION, P_VELOCITE}},
    [PG_MOUVEMENT] = {"DRONE MOTION", B_LFO, {P_MOUVEMENT, P_VITESSE, P_MAREE, P_PERIODE}},
    [PG_LFO1] = {"LFO 1", B_LFO, {P_L1_SHAPE, P_L1_RATE, P_L1_DEPTH, P_L1_POL}},
    [PG_LFO1T] = {"LFO 1 TARGETS", B_LFO, {P_L1_T1, P_L1_T2, P_L1_T3, P_L1_T4}},
    [PG_LFO2] = {"LFO 2", B_LFO, {P_L2_SHAPE, P_L2_RATE, P_L2_DEPTH, P_L2_POL}},
    [PG_LFO2T] = {"LFO 2 TARGETS", B_LFO, {P_L2_T1, P_L2_T2, P_L2_T3, P_L2_T4}},
    [PG_LFO3] = {"LFO 3", B_LFO, {P_L3_SHAPE, P_L3_RATE, P_L3_DEPTH, P_L3_POL}},
    [PG_LFO3T] = {"LFO 3 TARGETS", B_LFO, {P_L3_T1, P_L3_T2, P_L3_T3, P_L3_T4}},
    [PG_HARMONY] = {"HARMONY", B_SEL, {P_ROOT, P_SCALE, P_CHORD, P_VOICING}},
    [PG_GENFX] = {"GENERATIVE FX", B_FX, {P_SWELL, P_GX_SPARK, P_GX_BLUR, P_GX_SINK}},   /* FX's first: to perform */
    [PG_NUAGE] = {"CLOUD", B_FX, {P_TAILLE, P_DUREE, P_TEINTE, P_FREEZE}},
    [PG_NUAGE2] = {"CLOUD II", B_FX, {P_DIFFUSION, P_MODULATION, P_PREDELAI, P_LARGEUR}},
    [PG_NUAGE3] = {"CLOUD III", B_FX, {P_SHIMMER, P_INTERVALLE, P_SH_TONE, NPARAM}},   /* the shimmer */
    [PG_PUMP] = {"PUMP", B_FX, {P_PUMP_RATE, P_PUMP_DRONE, P_PUMP_BASS, P_PUMP_GLINT}},
    [PG_PUMP2] = {"PUMP II", B_FX, {P_PUMP_REL, P_PUMP_ATT, P_PUMP_OFF, NPARAM}},
    [PG_GLOBAL] = {"GLOBAL", B_GLO, {P_TEMPO, P_COUPEBAS, P_ACCORD, P_KEYS}},   /* KEYS: two detents (app.c) */
    [PG_GLOBAL2] = {"MIDI", B_GLO, {P_CANAL, P_CH_GLINT, P_CH_BASS, NPARAM}},
    [PG_MASTER] = {"MASTERING", B_GLO, {P_MS_THRESH, P_MS_RATIO, P_MS_RELEASE, P_MS_GAIN}},
    [PG_MIXER] = {"MIXER", B_HOME, {P_MX_DRONE, P_MX_BASS, P_MX_GLINT, P_MX_CLOUD}},   /* HOME's second (app.c) */
    [PG_SCENES] = {"SCENES", B_SAVE, {NPARAM, P_MORPH, P_LOAD, NPARAM}},   /* (K1: the slot, app.c) */
};
/* the three left encoders, always the same: the performance's hands */
static const uint8_t MACRO_OF[NE] = {[EN_SELECT] = P_KB, [EN_ALGO] = P_DR_FULL, [EN_PRESET] = P_DR_PRESET,
                                     [EN_K1] = NPARAM, [EN_K2] = NPARAM, [EN_K3] = NPARAM, [EN_K4] = NPARAM};

/* ------------------------------------------------------------ scenes --- */
#define NSCENES 16u
#define SC_CHORDS 8u                     /* latched drone chords a scene keeps */
#define SC_ARP 16u                       /* and GLINT's arpeggio notes */
#define SC_SEQ 32u                       /* and GLINT's sequence: a byte a step (app.c, SQ_*) */
#define SC_NAME 16u                      /* a name's characters */
#define SC_BSEQ 16u                      /* and the bass's sequence: a note and a length a step (app.c, bnote) */
typedef struct {
    char name[SC_NAME];                  /* NUL-padded; all sixteen used, none (app.c name_text) */
    uint8_t v[NSOUND];
    uint8_t nch, narp;                   /* what it plays: none (a factory scene) keeps what is playing */
    int8_t ch[SC_CHORDS][5];             /* each chord: its root, then its notes (-1: none) */
    int8_t arp[SC_ARP];                  /* the arpeggio's notes, low to high */
    int8_t bass;                         /* the bass's note, -1: none */
    uint8_t seq[SC_SEQ];                 /* GLINT's sequence */
    uint8_t bnote[SC_BSEQ], blen[SC_BSEQ];   /* the bass's sequence (kept apart from the rest: app.c OBJ_BSEQ) */
    uint8_t bq_on, bq_steps, bq_trans;   /* and its SEQUENCE, SEQ STEPS, TRANSPOSE: a scene plays its own, as GLINT's */
    uint8_t tempo;                       /* its TEMPO (P_TEMPO's value), with the bass's sequence in the store: LOAD
                                          * SND+NOTES takes it (app.c scene_take) */
} scene_t;

/* the factory scenes (slots 1 .. 7 until something is saved over them: app.c factory_get), each made as NEW SHARD is
 * (every setting at its default, its GLINT preset, the arpeggiator's and the delay's defaults), then its own settings
 * (P: a parameter and its value, to P_END), or its sound whole as an FM-1 stored it (one made on the device); its TEMPO
 * (BPM - 30), and what it plays: the drones it latches (each key's chord as the harmony makes it, or the chords as
 * latched), the arpeggio's notes, the bass's note held or its sequence, GLINT's sequence. Each
 * sounding like none of the others: their tempo, key and scale, the drone (one of its presets), what GLINT and the bass
 * do, the LFOs moving them. Notes: MIDI numbers; GLINT's without its OCTAVE, the bass's sequence's without the bass's (as
 * the sequencers keep them: app.c); a step 0 is a rest or a CUT, 127 a TIE or a HOLD, A() an accent */
#define P_END 0xFFu
#define A(n) ((n) | 0x80)
typedef struct {
    char name[SC_NAME + 1];              /* (its sixteen and the literal's NUL: only SC_NAME go into the scene) */
    uint8_t tempo;
    uint8_t p[52][2];                    /* its settings over NEW SHARD's base (to P_END) */
    uint8_t whole;                       /* 1: its sound whole instead, as an FM-1 stored it (sound) */
    uint8_t sound[NSOUND];
    int8_t keys[4];                      /* the drones latched, each key's chord as the harmony makes it (0: none) */
    uint8_t nch;                         /* or its chords as latched (ch: root, then notes; -1 none) */
    int8_t ch[SC_CHORDS][5];
    int8_t arp[SC_ARP];                  /* the arpeggio's notes, low to high (0: none) */
    int8_t bass;                         /* the bass's note held, as it sounds (-1: none) */
    uint8_t seq[SC_SEQ];                 /* GLINT's sequence */
    uint8_t bnote[SC_BSEQ], blen[SC_BSEQ];   /* the bass's: its notes, its lengths (BQ_LEN_NAME's) */
    uint8_t bq_on, bq_steps, bq_trans;   /* its SEQUENCE, SEQ STEPS (steps - 1), TRANSPOSE */
} factory_t;
static const factory_t FACTORY[] = {
    /* NORTH RIVER: the user's own, made on the FM-1 and kept whole as it stored it (its sound, every setting, in
     * params.c's order: a layout that changes a setting's meaning makes it over, as it would a stored scene). F# minor
     * pentatonic at 90 BPM: two sevenths latched wide, on B2 and F#3; FROST's arpeggio at random in sixteenths over three
     * octaves; SUB's line of five steps, struck on each beat (PULSE) */
    {.name = "NORTH RIVER", .tempo = 60, .whole = 1,
     .sound = {74, 77, 103, 104, 91, 26, 39, 71, 5, 70, 0, 89, 87, 83, 114, 61, 120, 51, 79, 43, 99, 79, 88, 19, 95, 64,
               13, 0, 25, 127, 50, 58, 78, 6, 11, 3, 2, 6, 11, 45, 55, 75, 2, 110, 72, 3, 7, 2, 79, 0, 68, 23,
               111, 1, 127, 81, 38, 23, 126, 1, 3, 81, 5, 127, 53, 0, 90, 60, 40, 23, 2, 72, 5, 76, 99, 34, 96, 100,
               127, 36, 0, 0, 3, 0, 0, 0, 0, 85, 0, 23, 1, 40, 0, 0, 0, 24, 42, 0, 26, 0, 0, 0, 0, 16,
               17, 0, 35, 23, 0, 0, 15, 64, 14, 0, 51, 5, 32, 93, 0, 36, 0, 0, 0, 1, 0, 0, 0, 90, 13, 4},
     .nch = 2, .ch = {{47, 35, 52, 57, 61}, {54, 42, 59, 64, 69}},
     .arp = {52, 61, 64}, .bass = -1,
     .seq = {0},
     .bnote = {66, 57, 59, 59, 61}, .blen = {8, 6, 4, 2, 2}, .bq_on = 1, .bq_steps = 4, .bq_trans = 14},
    /* LUMIERE NOCTURNE: the user's own too, kept whole the same way. A major at 60 BPM, a light in the night: an add9
     * latched wide on D2, the drone all PARTIALS and BREATH, slow to come and to go, in a long, bright hall; GLINT's
     * arpeggio on one note, A1, an octave up; SAW's A2 held, struck on each half beat (PULSE); three slow LFOs (TIMBRE
     * and BREATH, SPACE and SPREAD, GLINT's BRIGHT and the bass's FILTER) */
    {.name = "LUMIERE NOCTURNE", .tempo = 30, .whole = 1,
     .sound = {0, 115, 98, 40, 50, 46, 60, 20, 0, 60, 0, 100, 96, 90, 76, 30, 64, 36, 30, 38, 118, 94, 95, 29, 100, 85,
               20, 2, 56, 127, 50, 64, 66, 9, 1, 6, 2, 8, 1, 32, 90, 90, 1, 90, 84, 0, 5, 1, 127, 10, 0, 19,
               50, 1, 40, 48, 54, 26, 96, 2, 0, 94, 3, 50, 45, 0, 90, 60, 40, 28, 0, 62, 0, 72, 100, 100, 100, 83,
               127, 36, 0, 0, 0, 0, 0, 0, 0, 85, 0, 0, 1, 50, 0, 0, 0, 18, 14, 0, 1, 7, 0, 0, 0, 14,
               12, 0, 2, 24, 0, 0, 15, 64, 14, 0, 87, 0, 21, 16, 0, 26, 31, 0, 0, 1, 26, 0, 0, 99, 0, 3},
     .nch = 1, .ch = {{38, 26, 42, 45, 52}},
     .arp = {33}, .bass = 45,
     .seq = {0},
     .bnote = {0}, .blen = {0}, .bq_on = 0, .bq_steps = 15, .bq_trans = 14},
    /* RESILIENCE: the user's own, kept whole. F# minor pentatonic at 90 BPM: two sevenths latched wide, on B2 and A3, a
     * full drone (all four layers up, WARMTH most) moving on its own (MOTION, TIDE); HARP's arpeggio at random in
     * sixteenths over three octaves, through a long ping-pong echo; REESE driven, its line of nine steps far under */
    {.name = "RESILIENCE", .tempo = 60, .whole = 1,
     .sound = {98, 88, 103, 126, 91, 26, 39, 71, 5, 70, 0, 89, 87, 83, 114, 61, 120, 51, 79, 43, 99, 79, 88, 0, 95, 64,
               13, 0, 25, 127, 50, 58, 78, 6, 11, 3, 2, 13, 1, 34, 85, 90, 1, 80, 80, 3, 7, 2, 91, 0, 110, 23,
               111, 1, 127, 106, 86, 35, 126, 0, 30, 50, 4, 127, 53, 0, 90, 60, 40, 23, 2, 72, 0, 81, 72, 78, 96, 57,
               127, 36, 0, 0, 3, 0, 0, 0, 0, 85, 0, 37, 1, 40, 0, 0, 0, 25, 0, 0, 0, 0, 0, 0, 0, 25,
               0, 0, 0, 0, 0, 0, 15, 64, 14, 0, 87, 0, 25, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 64, 127, 0},
     .nch = 2, .ch = {{47, 35, 52, 57, 61}, {57, 45, 61, 66, 71}},
     .arp = {47, 54, 57}, .bass = -1,
     .seq = {0},
     .bnote = {54, 127, 61, 61, 127, 57, 127, 59, 49}, .blen = {7, 1, 0, 2, 0, 5, 1, 5, 3, 8}, .bq_on = 1, .bq_steps = 8, .bq_trans = 14},
    /* LONG NIGHT: A Phrygian at 60 BPM, dark and slow (as the user remade it on the FM-1, kept whole). NOCTURNE's
     * resonant low drone on a wide fifth; REESE's line gliding in bars and halves (six bars) under BOWED glass, ratio 9,
     * long tied notes in four (they drift apart) far in an echo; LFO 1 a slow drift on both filters, LFO 3 now and then a
     * little SINK; a long, dark hall */
    {.name = "LONG NIGHT", .tempo = 30, .whole = 1,
     .sound = {76, 89, 32, 19, 51, 44, 57, 25, 0, 70, 0, 89, 89, 83, 76, 38, 44, 51, 25, 38, 120, 115, 30, 0, 95, 64,
               13, 0, 25, 127, 50, 64, 57, 9, 4, 1, 2, 15, 10, 21, 62, 38, 1, 125, 118, 0, 3, 1, 76, 120, 0, 19,
               96, 1, 99, 40, 40, 60, 95, 1, 40, 70, 4, 116, 50, 0, 90, 60, 40, 28, 0, 62, 0, 100, 100, 72, 100, 57,
               127, 36, 0, 0, 0, 0, 0, 0, 0, 85, 0, 0, 1, 64, 0, 0, 6, 8, 50, 0, 31, 9, 0, 0, 0, 67,
               30, 2, 29, 0, 0, 0, 15, 110, 14, 17, 100, 5, 31, 20, 1, 40, 28, 0, 0, 2, 0, 30, 0, 90, 100, 0},
     .nch = 1, .ch = {{45, 33, 52, -1, -1}},
     .arp = {0}, .bass = -1,
     .seq = {64, 127, 127, 127, 0, 0, 65, 127, 62, 127, 127, 0, 60, 127, 69, 127},
     .bnote = {45, 127, 41, 40, 45, 46, 45, 127}, .blen = {6, 6, 4, 4, 6, 4, 4, 6}, .bq_on = 1, .bq_steps = 7, .bq_trans = 14},
    /* OPEN WATER: the river open again, D Dorian at 118 BPM. PULSE's quick beating on a sus2, ducked on each beat (PUMP);
     * MARIMBA's sixteenths, syncopated, accents, an echo; a driven SAW in eighths, plucked (DECAY, SUSTAIN), its
     * filter swept over two bars (LFO 1); the mastering pressing a little */
    {.name = "OPEN WATER", .tempo = 88,
     .p = {{P_ROOT, 2}, {P_SCALE, 3}, {P_CHORD, 4}, {P_VOICING, 1}, {P_DR_PRESET, 19}, {P_DR_FULL, 70},
      {P_GL_PRESET, 10}, {P_SQ_TYPE, 2}, {P_GL_OCTAVE, 1}, {P_ARP_RATE, 7}, {P_SQ_LEN, 15}, {P_SQ_GATE, 50},
      {P_GL_LEVEL, 95}, {P_GL_BRIGHT, 70}, {P_GD_TIME, 19}, {P_GD_MIX, 35}, {P_GD_FB, 40},
      {P_BS_MODEL, 3}, {P_BS_FILTER, 55}, {P_BS_RESO, 60}, {P_BS_DRIVE, 50}, {P_BS_LEVEL, 95}, {P_BS_ATTACK, 0},
      {P_BS_RELEASE, 30}, {P_BS_DECAY, 45}, {P_BS_SUSTAIN, 40}, {P_BS_GLIDE, 0},
      {P_PUMP_RATE, 5}, {P_PUMP_DRONE, 100}, {P_PUMP_BASS, 0}, {P_PUMP_GLINT, 30},
      {P_TAILLE, 80}, {P_DUREE, 60}, {P_TEINTE, 70}, {P_MS_RATIO, 3}, {P_MS_THRESH, 22}, {P_MS_GAIN, 6},
      {P_L1_SHAPE, 1}, {P_L1_RATE, 67}, {P_L1_DEPTH, 45}, {P_L1_T1, 31},
      {P_END, 0}},
     .keys = {50}, .arp = {0}, .bass = -1,
     .seq = {A(62), 0, 69, 0, 65, A(69), 0, 72, 0, 67, A(64), 0, 69, 0, 72, 74},
     .bnote = {50, 0, 50, 53, 0, 55, 57, 0, 50, 0, 60, 57, 0, 55, 53, 0}, .blen = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, .bq_on = 1, .bq_steps = 15, .bq_trans = 14},
    /* WHITEOUT: C# minor at 72 BPM, snow on the wind. WIND's breath alone, a wide sus4, its air gusting (LFO 1, at
     * random) and its spread drifting (LFO 2); FROST's flurries, an arpeggio at random in sixteenth triplets over three
     * octaves, mostly silent, LFO 3 bringing them in; a sub far under, slow; BLUR and the widest cloud */
    {.name = "WHITEOUT", .tempo = 42,
     .p = {{P_ROOT, 1}, {P_SCALE, 2}, {P_CHORD, 5}, {P_VOICING, 2}, {P_DR_PRESET, 9}, {P_DR_FULL, 127},
      {P_GL_PRESET, 6}, {P_SQ_TYPE, 1}, {P_ARP_MODE, 3}, {P_ARP_RATE, 8}, {P_ARP_RANGE, 2}, {P_ARP_CHANCE, 35},
      {P_GL_OCTAVE, 1}, {P_GL_LEVEL, 70}, {P_GL_BRIGHT, 50},
      {P_BS_MODEL, 5}, {P_BS_LEVEL, 70}, {P_BS_ATTACK, 110}, {P_BS_RELEASE, 100}, {P_BS_FILTER, 30},
      {P_GX_BLUR, 70}, {P_GX_SINK, 20}, {P_MS_RATIO, 2}, {P_MS_THRESH, 24}, {P_MS_GAIN, 5},
      {P_TAILLE, 127}, {P_DUREE, 105}, {P_TEINTE, 55}, {P_DIFFUSION, 127}, {P_MODULATION, 100}, {P_LARGEUR, 127},
      {P_L1_SHAPE, 5}, {P_L1_RATE, 44}, {P_L1_DEPTH, 35}, {P_L1_T1, 23}, {P_L1_T2, 21},
      {P_L2_SHAPE, 6}, {P_L2_RATE, 4}, {P_L2_DEPTH, 60}, {P_L2_T1, 24}, {P_L2_T2, 22},
      {P_L3_SHAPE, 1}, {P_L3_RATE, 17}, {P_L3_DEPTH, 50}, {P_L3_POL, 1}, {P_L3_T1, 36}, {P_L3_T2, 30},
      {P_END, 0}},
     .keys = {49}, .arp = {61, 64, 68, 71}, .bass = 25,
     .seq = {0},
     .bnote = {0}, .blen = {0}, .bq_on = 0, .bq_steps = 15, .bq_trans = 14},
    /* THAW: E-flat major at 78 BPM, warm and dripping. EMBER's glowing square choir, add9, open, its warmth and body
     * swaying (LFO 1); DROP's arpeggio drifting in eighth triplets, half of its steps; FM's bass held, struck again on
     * each half beat (PULSE); LFO 2 sinking everything slowly now and then, LFO 3 the strike's and the bass's tone at
     * random */
    {.name = "THAW", .tempo = 48,
     .p = {{P_ROOT, 3}, {P_SCALE, 1}, {P_CHORD, 6}, {P_VOICING, 1}, {P_DR_PRESET, 6}, {P_DR_FULL, 110},
      {P_GL_PRESET, 5}, {P_SQ_TYPE, 1}, {P_ARP_MODE, 4}, {P_ARP_RATE, 6}, {P_ARP_RANGE, 1}, {P_ARP_CHANCE, 75},
      {P_GL_OCTAVE, 1}, {P_GL_LEVEL, 60},
      {P_BS_MODEL, 7}, {P_BS_LEVEL, 85}, {P_BS_FILTER, 50}, {P_BS_DECAY, 60}, {P_BS_SUSTAIN, 80},
      {P_BS_ATTACK, 10}, {P_BS_RELEASE, 45}, {P_BS_PULSE, 3}, {P_GX_SINK, 15}, {P_MS_RATIO, 2}, {P_MS_GAIN, 2},
      {P_TAILLE, 95}, {P_DUREE, 80}, {P_TEINTE, 50}, {P_MODULATION, 90},
      {P_L1_SHAPE, 0}, {P_L1_RATE, 19}, {P_L1_DEPTH, 40}, {P_L1_T1, 4}, {P_L1_T2, 35},
      {P_L2_SHAPE, 6}, {P_L2_RATE, 5}, {P_L2_DEPTH, 45}, {P_L2_POL, 1}, {P_L2_T1, 40},
      {P_L3_SHAPE, 5}, {P_L3_RATE, 38}, {P_L3_DEPTH, 30}, {P_L3_T1, 27}, {P_L3_T2, 31},
      {P_END, 0}},
     .keys = {51}, .arp = {63, 67, 70, 74, 77}, .bass = 27,
     .seq = {0},
     .bnote = {0}, .blen = {0}, .bq_on = 0, .bq_steps = 15, .bq_trans = 14},
};
#undef A
/* NEW SHARD: SCENES' first line, above slot 01. No slot (nothing is stored into it): a sound to begin a shard from,
 * in the code as the factory scenes are (app.c new_shard), playing nothing. A calm drone, CUSTOM: the choir soft, a
 * little PARTIALS and BREATH, warm; slow to come and to go; MOTION at half, its drift slow; a lush cloud, not a wash
 * (LOW CUT at 83 HZ: no boom swelling under it as the drift moves the low notes). A seventh in A minor, open (A E G C:
 * Am7 on A). GLINT's SINE */
#define S(n, ...) {n, {__VA_ARGS__}, 0, 0, {{0}}, {0}, -1, {0}, {0}, {0}, 0, 15, 14, 60}   /* playing nothing, at 90 BPM */
static const scene_t NEW_SHARD = S("NEW SHARD", 84, 50, 44, 40, 50, 46, 60, 20, 0, 60, 0, 100, 96, 90, 76, 30, 64, 36,
                                   30, 38, 98, 82, 60, 0, 100, 85, 20, 0, 56, 127, 50, 64, 66,
                                   9, 2, 3, 1, 8);
/* ... and its settings past the factory scenes' (the rest at their defaults): BODY at half (the drone thinner, the
 * SUB under it), the breath's noise darker; GLINT dim (BRIGHT a quarter); the bass a SUB, soft, slow to come. The
 * three LFOs on, slow sines (23 S, 37 S, 16 S: never in step), a little deep, each on two settings (TARGET's values:
 * LFO_TGT, below): LFO 1 the drone's breath, TIMBRE and BREATH; LFO 2 its distance, SPACE and SPREAD; LFO 3 the
 * other two tracks' tone, GLINT's BRIGHT and the bass's FILTER */
static const uint8_t NEW_SHARD_P[][2] = {
    {P_DR_FULL, 64}, {P_BR_TONE, 50}, {P_GL_BRIGHT, 32},
    {P_BS_MODEL, 5}, {P_BS_DRIVE, 16}, {P_BS_FILTER, 34}, {P_BS_RESO, 10}, {P_BS_ATTACK, 96}, {P_BS_RELEASE, 88},
    {P_L1_RATE, 18}, {P_L1_DEPTH, 14}, {P_L1_T1, 1}, {P_L1_T2, 7},       /* TIMBRE, BREATH */
    {P_L2_RATE, 14}, {P_L2_DEPTH, 12}, {P_L2_T1, 2}, {P_L2_T2, 24},      /* SPACE, SPREAD */
    {P_L3_RATE, 21}, {P_L3_DEPTH, 16}, {P_L3_T1, 26}, {P_L3_T2, 31},     /* GL BRIGHT, BS FILTER */
    {P_SQ_TYPE, 1},                      /* GLINT's SEQ on ARP (app.c SQT_ARP): its keys arpeggiated, latched */
};
#undef S
#define NFACTORY (sizeof FACTORY / sizeof FACTORY[0])
#define SC_NEW NSCENES                   /* NEW SHARD's number: the scene loaded (st.scene), SCENES' K1, the settings'
                                          * scene byte (app.c settings_t) */

/* GLINT's presets: PRESET loads one into the parameters after it, which then shape it */
#define NGPRESET 22u
static const char *const GLINT_NAME[NGPRESET] = {"ICE", "BELL", "BLIP", "GLASS", "CHIME", "DROP", "FROST", "CELESTA",
                                                 "SINE", "KALIMBA", "MARIMBA", "MUSIC BOX", "VIBES", "HARP", "PIPE",
                                                 "BOWED", "WARM LEAD", "BRASS", "SQUARE", "SUB LEAD", "STRINGS", "REED"};
static const uint8_t GLINT_PRESET[NGPRESET][10] = {
    /* RATIO BRIGHT DECAY LEVEL OCTAVE SPACE RELEASE ATTACK PING SUSTAIN (RELEASE as DECAY let each go before it:
     * glint_release_of) */
    {6, 70, 70, 90, 2, 90, 76, 0, 68, 0},       /* ICE: a high bell */
    {2, 80, 95, 85, 1, 80, 87, 0, 68, 0},       /* BELL: inharmonic, long */
    {3, 55, 25, 95, 2, 70, 50, 0, 68, 0},       /* BLIP */
    {9, 50, 85, 80, 2, 100, 80, 0, 68, 0},      /* GLASS */
    {4, 75, 110, 80, 1, 90, 99, 0, 68, 0},      /* CHIME: a bar's modes */
    {1, 90, 30, 100, 1, 60, 54, 0, 68, 0},      /* DROP */
    {11, 45, 55, 75, 2, 110, 72, 0, 68, 0},     /* FROST: glitter high up */
    {7, 60, 60, 90, 2, 80, 76, 0, 68, 0},       /* CELESTA */
    {1, 0, 90, 90, 1, 90, 84, 10, 0, 0},        /* SINE: pure */
    {1, 45, 50, 95, 1, 70, 69, 0, 120, 0},      /* KALIMBA: a woody pluck, its ring nearly pure */
    {7, 60, 35, 95, 0, 60, 57, 0, 127, 0},      /* MARIMBA: a bar, short */
    {8, 40, 75, 85, 2, 85, 76, 0, 90, 0},       /* MUSIC BOX */
    {3, 40, 100, 85, 1, 85, 91, 5, 40, 0},      /* VIBES: round, long */
    {1, 70, 80, 90, 1, 80, 76, 0, 110, 0},      /* HARP */
    {1, 25, 105, 80, 1, 90, 95, 75, 10, 0},     /* PIPE: a soft flute, breathed in */
    {3, 35, 120, 80, 1, 100, 106, 105, 0, 0},   /* BOWED: glass rubbed, slow to sound */
    /* the leads: held while the key is down (SUSTAIN), the modulator at the note or near it (a saw's harmonics, a
     * pulse's), PING the filter's sweep at the start, drier */
    {1, 62, 95, 88, 1, 55, 87, 12, 45, 105},    /* WARM LEAD: all the harmonics, round, a little bite at the start */
    {1, 78, 80, 88, 1, 50, 76, 45, 85, 95},     /* BRASS: swells in bright, settles darker */
    {3, 58, 100, 85, 1, 55, 91, 8, 25, 115},    /* SQUARE: the odd harmonics, hollow */
    {0, 55, 100, 90, 1, 45, 91, 8, 35, 110},    /* SUB LEAD: an octave under it too */
    {1, 48, 110, 85, 1, 85, 99, 85, 15, 120},   /* STRINGS: slow to come, an ensemble */
    {5, 45, 95, 85, 1, 55, 87, 15, 40, 100},    /* REED: nasal (no third harmonic) */
};
/* the drone's presets (PRESETS turns them): each sets the NDRONE settings in DRONE_P, the three layers (their
 * levels, their pages and their second pages) and the drone's own (DRONE PARAMS but FULLNESS, ALGORITHM's: a hand
 * that stays where it is; DRONE SHAPE's ATTACK and RELEASE; DRONE MOTION). 0 is CUSTOM: the settings as turned (app.c
 * keeps them while a preset plays, and in the scenes) */
static const uint8_t DRONE_P[NDRONE] = {P_CHOEUR, P_PARTIELS, P_SOUFFLE, P_CHALEUR, P_DESACCORD, P_FILTRE, P_PENTE,
                                        P_IMPAIRS, P_ETIREMENT, P_RESONANCE, P_COULEUR, P_ECART, P_ATTAQUE, P_RELACHE,
                                        P_MOUVEMENT, P_VITESSE, P_MAREE, P_PERIODE, P_TIMBRE, P_ESPACE, P_CH_SHAPE,
                                        P_CH_INTERVAL, P_CH_RESO, P_CH_TYPE, P_PA_EVOLVE, P_PA_BEAT, P_PA_FORMANT,
                                        P_PA_PEAK, P_BR_GRAIN, P_BR_PITCH, P_BR_TONE, P_BR_AIR};
static const char *const DRONE_NAME[NDPRESET + 1u] = {"CUSTOM", "MIST", "CATHEDRAL", "GLACIER", "ABYSS", "AURORA", "EMBER", "TIDE", "DUST", "WIND", "ORGAN", "HOLLOW", "VOWEL", "SHIMMER", "CHANTER", "ENSEMBLE", "BELLS", "NOCTURNE", "HAZE", "PULSE", "RIME", "DEEP", "SPECTRAL", "RUST", "STILL"};
static const uint8_t DRONE_PRESET[NDPRESET][NDRONE] = {
    /* CHO PAR SOU WAR DET FIL TIL ODD STR BRES COL SPR ATT REL MOT RAT TID PER TIM SPA SHP INT CRES TYP EVO BEAT
     * FORM PEAK GRN PIT TONE AIR */
    {76, 89, 32, 19, 51, 44, 57, 25, 0, 70, 0, 89, 89, 83, 44, 51, 25, 38, 64, 57, 0, 0, 0, 0, 0, 0, 85, 0, 0, 1, 64, 0},  /* MIST: the default: all three, gently */
    {110, 50, 20, 25, 85, 55, 60, 30, 0, 60, 0, 110, 100, 100, 60, 40, 20, 50, 60, 95, 0, 1, 20, 0, 10, 0, 85, 0, 0, 1, 64, 0},  /* CATHEDRAL: a wide choir, an octave under, a long hall */
    {20, 100, 70, 5, 30, 60, 35, 50, 60, 115, 127, 100, 80, 110, 40, 30, 30, 40, 80, 90, 0, 0, 0, 0, 40, 15, 100, 30, 20, 2, 80, 0},  /* GLACIER: glass: stretched partials, bell resonators */
    {90, 70, 25, 50, 70, 20, 90, 40, 0, 50, 0, 70, 100, 105, 50, 25, 50, 25, 40, 70, 30, 1, 40, 0, 20, 0, 60, 0, 0, 0, 30, 0},  /* ABYSS: dark and deep, an octave under, warm */
    {60, 105, 40, 15, 45, 65, 40, 15, 10, 85, 60, 115, 90, 105, 55, 45, 45, 50, 75, 95, 0, 0, 0, 0, 70, 25, 90, 35, 0, 2, 75, 10},  /* AURORA: the spectrum wandering, a light formant */
    {100, 55, 20, 80, 40, 40, 70, 60, 0, 50, 30, 80, 70, 80, 35, 40, 20, 40, 55, 50, 64, 0, 50, 0, 10, 0, 70, 0, 0, 1, 50, 0},  /* EMBER: a warm square choir, glowing low */
    {80, 80, 45, 25, 60, 45, 55, 30, 10, 75, 40, 100, 95, 100, 70, 30, 110, 60, 60, 70, 0, 0, 30, 0, 30, 0, 85, 0, 0, 1, 64, 0},  /* TIDE: a deep tide on the timbre */
    {25, 40, 85, 20, 40, 50, 60, 30, 20, 100, 60, 120, 60, 90, 50, 50, 30, 40, 60, 75, 0, 0, 0, 0, 30, 0, 85, 0, 90, 2, 85, 40},  /* DUST: grains ringing in resonators, crackle */
    {0, 20, 85, 0, 40, 50, 60, 30, 0, 20, 30, 127, 80, 95, 90, 80, 50, 70, 64, 80, 0, 0, 0, 0, 0, 0, 85, 0, 0, 1, 75, 50},  /* WIND: air: breath alone, moving */
    {40, 115, 10, 30, 10, 50, 45, 0, 0, 60, 0, 60, 40, 60, 15, 40, 10, 40, 64, 50, 0, 2, 0, 0, 0, 0, 85, 0, 0, 1, 64, 0},  /* ORGAN: steady pipes, a fifth over, still */
    {105, 80, 25, 20, 35, 50, 50, 110, 0, 70, 64, 90, 80, 90, 40, 45, 25, 40, 60, 65, 64, 0, 40, 1, 15, 0, 85, 0, 0, 1, 60, 0},  /* HOLLOW: odd harmonics, a band-passed square */
    {65, 125, 15, 10, 30, 55, 30, 20, 0, 70, 0, 85, 90, 95, 45, 35, 60, 45, 64, 70, 0, 0, 60, 1, 20, 0, 95, 105, 0, 1, 64, 0},  /* VOWEL: a sung overtone, a vowel */
    {45, 100, 50, 10, 55, 75, 25, 20, 15, 100, 90, 120, 85, 110, 50, 55, 30, 40, 85, 100, 0, 3, 0, 0, 40, 50, 100, 20, 10, 3, 90, 20},  /* SHIMMER: bright beating partials, a halo a twelfth up */
    {105, 45, 15, 40, 30, 55, 60, 70, 0, 60, 30, 70, 50, 70, 30, 45, 15, 40, 60, 45, 110, 0, 35, 1, 0, 0, 85, 0, 0, 1, 64, 0},  /* CHANTER: a reed: a narrow pulse, nasal */
    {115, 40, 20, 30, 75, 60, 55, 25, 0, 60, 0, 115, 95, 90, 55, 55, 20, 40, 65, 75, 90, 0, 15, 0, 0, 0, 85, 0, 0, 1, 64, 0},  /* ENSEMBLE: strings: detuned pulses swaying */
    {15, 115, 45, 5, 25, 50, 30, 20, 95, 110, 127, 95, 40, 110, 30, 40, 20, 40, 75, 85, 0, 0, 0, 0, 30, 30, 85, 0, 0, 2, 80, 0},  /* BELLS: stretched into bells, ringing */
    {90, 60, 30, 35, 55, 25, 85, 30, 0, 55, 0, 90, 110, 115, 45, 20, 40, 20, 45, 85, 20, 1, 70, 0, 25, 0, 70, 0, 0, 0, 35, 0},  /* NOCTURNE: slow and dark, a resonant low-pass */
    {85, 50, 60, 10, 60, 70, 50, 25, 0, 40, 30, 120, 100, 105, 60, 50, 30, 50, 70, 90, 0, 3, 20, 2, 20, 0, 85, 0, 0, 2, 80, 30},  /* HAZE: airy: high-passed, an octave up */
    {50, 95, 25, 20, 30, 50, 45, 30, 0, 70, 20, 100, 60, 80, 30, 50, 20, 40, 64, 65, 0, 0, 0, 0, 15, 100, 85, 0, 0, 1, 64, 0},  /* PULSE: every partial beating, quick */
    {30, 110, 70, 0, 20, 80, 20, 40, 30, 120, 100, 110, 70, 100, 35, 60, 20, 40, 90, 90, 0, 3, 0, 2, 30, 20, 110, 40, 40, 4, 100, 15},  /* RIME: thin ice, two octaves up, glittering */
    {100, 100, 0, 40, 25, 25, 100, 20, 0, 50, 0, 60, 90, 95, 25, 30, 20, 30, 40, 45, 0, 1, 10, 0, 0, 0, 40, 0, 0, 0, 30, 0},  /* DEEP: weight: the fundamental, an octave under */
    {20, 120, 15, 5, 20, 50, 20, 10, 0, 70, 0, 90, 90, 100, 50, 40, 80, 50, 64, 80, 0, 0, 0, 0, 50, 10, 85, 127, 0, 1, 64, 0},  /* SPECTRAL: one overtone alone, singing */
    {80, 50, 40, 100, 50, 45, 65, 60, 20, 80, 40, 80, 60, 80, 40, 55, 25, 40, 55, 55, 70, 0, 80, 1, 20, 15, 75, 0, 60, 1, 45, 20},  /* RUST: gritty: driven, grains, a resonant band */
    {0, 125, 0, 0, 0, 50, 127, 0, 0, 50, 0, 60, 100, 100, 5, 20, 0, 40, 50, 70, 0, 0, 0, 0, 0, 0, 85, 0, 0, 1, 64, 0},  /* STILL: almost a sine, unmoving */
};

/* the LFOs: RATE's free values (LFO_NFREE of them, 0.005 Hz .. 10 Hz), then its values on the grid, in MIDI clocks
 * (24 a quarter note); what they can move (TARGET's values, 0 OFF): the settings the engine takes as they turn */
#define NLFO 3u
static const uint8_t LFO_BASE[NLFO] = {P_L1_SHAPE, P_L2_SHAPE, P_L3_SHAPE};   /* each LFO's first parameter (SHAPE,
                                                                             * RATE, DEPTH, POLARITY, its TARGETs) */
#define LFO_NFREE 64u
static const uint16_t LFO_TICKS[12] = {1536, 768, 384, 192, 96, 48, 32, 24, 16, 12, 8, 6};
static const char *const LFO_SYNC_NAME[12] = {"16 BARS", "8 BARS", "4 BARS", "2 BARS", "1 BAR", "1/2", "1/2T", "1/4",
                                             "1/4T", "1/8", "1/8T", "1/16"};
static const char *const LFO_SHAPE_NAME[7] = {"SINE", "TRIANGLE", "SAW", "RAMP", "SQUARE", "RANDOM", "DRIFT"};
static const char *const LFO_POL_NAME[3] = {"BIPOLAR", "UP", "DOWN"};
static const uint8_t LFO_TGT[NLFO_TGT] = {NPARAM, P_TIMBRE, P_ESPACE, P_MOUVEMENT, P_CHALEUR, P_CHOEUR, P_PARTIELS, P_SOUFFLE,
                                    P_DESACCORD, P_FILTRE, P_CH_SHAPE, P_CH_RESO, P_PENTE, P_IMPAIRS, P_ETIREMENT,
                                    P_PA_EVOLVE, P_PA_BEAT, P_PA_FORMANT, P_PA_PEAK, P_RESONANCE, P_COULEUR, P_BR_GRAIN,
                                    P_BR_TONE, P_BR_AIR, P_ECART, P_GL_LEVEL, P_GL_BRIGHT, P_GL_PING, P_GL_SPACE,
                                    P_MX_DRONE, P_MX_GLINT, P_BS_FILTER, P_BS_RESO, P_BS_DRIVE, P_MX_BASS, P_DR_FULL,
                                    P_ARP_CHANCE, P_SQ_LEN, P_GX_SPARK, P_GX_BLUR, P_GX_SINK};   /* (GLINT's CHANCE,
                                                                * its SEQ LENGTH, SPARKLE: app.c's, arp_chance,
                                                                * seq_len, sparkle_run) */
static const char *const LFO_TGT_NAME[NLFO_TGT] = {"OFF", "TIMBRE", "SPACE", "MOTION", "WARMTH", "CHOIR", "PARTIALS",
                                             "BREATH", "DETUNE", "FILTER", "SHAPE", "CH RESO", "TILT", "ODD",
                                             "STRETCH", "EVOLVE", "BEAT", "FORMANT", "PEAK", "BR RESO", "COLOR",
                                             "GRAIN", "BR TONE", "AIR", "SPREAD", "GL LEVEL", "GL BRIGHT", "GL PING",
                                             "GL SPACE", "DRONE LVL", "GLINT LVL", "BS FILTER", "BS RESO",
                                             "BS DRIVE", "BASS LVL", "BODY", "GL CHANCE", "SEQ LEN",
                                             "SPARKLE", "BLUR", "SINK"};   /* (new ones at the end: a saved TARGET
                                                                         * keeps its setting) */
static float lfo_hz(uint32_t r) { return 0.005f * fm_powf(2000.0f, (float)r / (float)(LFO_NFREE - 1u)); }
/* GLINT's OCTAVE, in semitones: every glint is played without it, the engine puts it on (a key's, a MIDI note's, the
 * arpeggio's, the sequence's; those sounding too, at once: glint.c), and NOTE shows the steps so (app.c sets it) */
static int32_t seq_shift;
/* BASS's OCTAVE, in semitones: every note of the bass is played without it, the engine puts it on (folded up to C1;
 * the one sounding moves there at once: bass.c), and NOTE shows the steps so */
static int32_t bseq_shift;
/* a scene's MORPH: what each setting still lacks of the scene's, in its own steps, toward 0 (app.c, morph_run) */
static float morph_off[NSOUND];
/* what the LFOs add to each setting now, in its own steps (app.c, lfo_run); params_to_engine takes them */
static float mod[NSOUND];

/* GLINT's delay, in MIDI clocks (24 a quarter note): T triplets, D dotted */
static const uint8_t GD_TICKS[12] = {3, 4, 6, 8, 9, 12, 16, 18, 24, 32, 36, 48};
static const char *const GD_NAME[12] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2T",
                                        "1/4D", "1/2"};
/* the arpeggiator's steps, in MIDI clocks (24 a quarter note) */
static const uint8_t ARP_TICKS[10] = {96, 48, 32, 24, 16, 12, 8, 6, 4, 3};
static const char *const ARP_RATE_NAME[10] = {"1/1", "1/2", "1/2T", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T",
                                              "1/32"};
/* a step of the bass's sequence, its STEP LENGTH: a quarter of a beat .. sixty-four beats (16 BARS), in MIDI clocks (24 a
 * beat) */
#define NBQLEN 13u
static const uint16_t BQ_TICKS[NBQLEN] = {6, 12, 24, 36, 48, 72, 96, 144, 192, 288, 384, 768, 1536};
static const char *const BQ_LEN_NAME[NBQLEN] = {"1/16", "1/8", "1/4", "1/4D", "1/2", "1/2D", "1 BAR", "1.5 BARS",
                                                "2 BARS", "3 BARS", "4 BARS", "8 BARS", "16 BARS"};
static const float GLINT_RATIO[13] = {0.5f, 1.0f, 1.414f, 2.0f, 2.756f, 3.0f, 3.5f, 4.0f, 5.0f, 7.0f, 9.0f, 11.0f, 13.6f};
static const char *const RATIO_NAME[13] = {"X0.5", "X1", "X1.41", "X2", "X2.76", "X3", "X3.5", "X4", "X5", "X7", "X9",
                                           "X11", "X13.6"};

static void arp_defaults(uint8_t *v)     /* a factory scene's arpeggiator (off, eighths over two octaves) and
                                          * GLINT's delay (dotted eighths, ping-pong, some) */
{
    v[P_GD_TIME] = 19;                   /* (1/8D, ping-pong) */
    v[P_GD_FB] = 50;
    v[P_GD_MODE] = 1;
    v[P_GD_MIX] = 40;
    v[P_SQ_TYPE] = 0;
    v[P_ARP_MODE] = 0;
    v[P_ARP_RATE] = 5;
    v[P_ARP_RANGE] = 1;
    v[P_ARP_CHANCE] = 127;
}

static void later_defaults(uint8_t *v, uint32_t from)   /* the parameters from `from` on (added after the factory
                                                       * scenes, or after a saved scene's layout): their defaults */
{
    uint32_t p;
    for (p = from; p < NSOUND; p++)
        v[p] = PD[p].def;
}

/* GLINT's RELEASE as DECAY let a glint go before RELEASE was one (where SCATTER was: an earlier layout's made over,
 * app.c sound_over; the River's and the server's from the schema's table): DECAY's time up to 0.6 s, else half of it,
 * 0.6 s at least */
static uint8_t glint_release_of(uint32_t decay)
{
    float t = 0.08f * fm_powf(75.0f, (float)(decay > 127u ? 127u : decay) / 127.0f), r, x;
    r = t < 0.6f ? t : t * 0.5f > 0.6f ? t * 0.5f : 0.6f;
    x = 127.0f * fm_log2f(r / 0.02f) / fm_log2f(300.0f) + 0.5f;
    return (uint8_t)(x < 0.0f ? 0.0f : x > 127.0f ? 127.0f : x);
}

static void glint_preset(uint8_t *v, uint32_t i)   /* preset i into the GLINT parameters */
{
    uint32_t k;
    i %= NGPRESET;
    v[P_GL_PRESET] = (uint8_t)i;
    for (k = 0; k < 7u; k++)
        v[P_GL_RATIO + k] = GLINT_PRESET[i][k];
    v[P_GL_ATTACK] = GLINT_PRESET[i][7];
    v[P_GL_PING] = GLINT_PRESET[i][8];
    v[P_GL_SUSTAIN] = GLINT_PRESET[i][9];
}

/* ---------------------------------------------------------- the engine --- */
static float u7(const uint8_t *v, int p)  /* a setting, 0..1, with what the LFOs add to it */
{
    float x = ((float)v[p] + (p < NSOUND ? mod[p] + morph_off[p] : 0.0f)) / (float)PD[p].max;
    return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
}

static void params_to_engine(const uint8_t *v, int freeze, frasil_params_t *b)
{
    b->choir = u7(v, P_CHOEUR);
    b->partials = u7(v, P_PARTIELS);
    b->souffle = u7(v, P_SOUFFLE);
    b->drive = u7(v, P_CHALEUR);
    b->detune = u7(v, P_DESACCORD);
    b->cutoff = u7(v, P_FILTRE);
    b->tilt = u7(v, P_PENTE);
    b->odd = u7(v, P_IMPAIRS);
    b->stretch = u7(v, P_ETIREMENT);
    b->resonance = u7(v, P_RESONANCE);
    b->color = u7(v, P_COULEUR);
    b->spread = u7(v, P_ECART);
    b->attack = u7(v, P_ATTAQUE);
    b->release = u7(v, P_RELACHE);
    b->pressure = u7(v, P_PRESSION);
    b->velocity = u7(v, P_VELOCITE);
    b->motion = u7(v, P_MOUVEMENT);
    b->motion_rate = u7(v, P_VITESSE);
    b->tide = u7(v, P_MAREE);
    b->tide_rate = u7(v, P_PERIODE);
    b->timbre = u7(v, P_TIMBRE);
    b->mix = u7(v, P_ESPACE);
    b->tune = ((float)v[P_ACCORD] - 50.0f) * 0.01f;
    b->rev.size = u7(v, P_TAILLE);
    b->rev.decay = u7(v, P_DUREE);
    b->rev.damp = u7(v, P_TEINTE);
    b->rev.shimmer = u7(v, P_SHIMMER);
    b->rev.shimmer_tone = u7(v, P_SH_TONE);
    b->rev.diffusion = u7(v, P_DIFFUSION);
    b->rev.mod = u7(v, P_MODULATION);
    b->rev.predelay = u7(v, P_PREDELAI);
    b->rev.interval = INTERVALS[v[P_INTERVALLE] % 5u];
    b->rev.lowcut = u7(v, P_COUPEBAS);
    b->rev.width = u7(v, P_LARGEUR);
    b->rev.freeze = freeze;
    b->glint.ratio = GLINT_RATIO[v[P_GL_RATIO] % 13u];
    b->glint.octave = (int)v[P_GL_OCTAVE];
    b->glint.bright = u7(v, P_GL_BRIGHT);
    b->glint.decay = u7(v, P_GL_DECAY);
    b->glint.level = u7(v, P_GL_LEVEL);
    b->glint.space = u7(v, P_GL_SPACE);
    b->glint.release = u7(v, P_GL_RELEASE);
    b->glint.attack = u7(v, P_GL_ATTACK);
    b->glint.ping = u7(v, P_GL_PING);
    b->glint.delay_fb = u7(v, P_GD_FB);
    b->glint.delay_mix = u7(v, P_GD_MIX);
    b->glint.delay_pp = v[P_GD_TIME] >= 12u;   /* (TIME's second half) */
    b->glint.delay_s = 0.25f;            /* the app puts the tempo's in (glint_delay_s) */
    b->glint.vibrato = u7(v, P_GL_VIBRATO);
    b->glint.sustain = u7(v, P_GL_SUSTAIN);
    b->pump_drone = u7(v, P_PUMP_DRONE);
    b->pump_bass = u7(v, P_PUMP_BASS);
    b->pump_glint = u7(v, P_PUMP_GLINT);
    b->pump_rel = 0.1f + 0.9f * u7(v, P_PUMP_REL);
    b->pump_att = 0.001f * fm_powf(300.0f, u7(v, P_PUMP_ATT));
    b->pump_off = u7(v, P_PUMP_OFF);
    b->comp_thr = (float)v[P_MS_THRESH] - 40.0f;
    b->comp_ratio = COMP_RATIO[v[P_MS_RATIO] % 8u];
    b->comp_rel = 0.03f * fm_powf(50.0f, u7(v, P_MS_RELEASE));
    b->comp_gain = v[P_MS_RATIO] ? fm_exp2f((float)v[P_MS_GAIN] * 0.166096405f) : 1.0f;   /* (RATIO OFF: no GAIN) */
    b->mx_drone = u7(v, P_MX_DRONE) * 1.27f;
    b->mx_bass = u7(v, P_MX_BASS) * 1.27f;
    b->mx_glint = u7(v, P_MX_GLINT) * 1.27f;
    b->mx_cloud = (float)v[P_MX_CLOUD] * 0.01f;
    {                                    /* the layers' second pages */
        static const float CH_IV[4] = {1.0f, 0.5f, 1.4983071f, 2.0f}, BR_IV[5] = {0.5f, 1.0f, 2.0f, 2.9966142f, 4.0f};
        float tn = u7(v, P_BR_TONE) * 127.0f;
        b->ch_shape = u7(v, P_CH_SHAPE);
        b->ch_interval = CH_IV[v[P_CH_INTERVAL] % 4u];
        b->ch_reso = u7(v, P_CH_RESO);
        b->ch_type = v[P_CH_TYPE] % 3u;
        b->pa_evolve = u7(v, P_PA_EVOLVE);
        b->pa_beat = u7(v, P_PA_BEAT);
        b->pa_formant = 3.0f * u7(v, P_PA_FORMANT);
        b->pa_peak = u7(v, P_PA_PEAK);
        b->br_grain = u7(v, P_BR_GRAIN);
        b->br_pitch = BR_IV[v[P_BR_PITCH] % 5u];
        b->br_tone = tn >= 126.5f ? 1.0f : (tn - 64.0f) * (1.0f / 63.0f);   /* -1 .. 1 */
        b->br_air = u7(v, P_BR_AIR);
    }
    b->fullness = u7(v, P_DR_FULL);
    {                                    /* GENERATIVE FX: BLUR, the drone into the cloud (wetter, longer, more
                                          * diffuse, the glints further in too); SINK, the engine's */
        float bl = u7(v, P_GX_BLUR);
        if (b->mix < 0.9f)
            b->mix += (0.9f - b->mix) * bl;
        b->rev.decay += (1.0f - b->rev.decay) * 0.7f * bl;
        b->rev.diffusion += (1.0f - b->rev.diffusion) * bl;
        b->rev.mod += (1.0f - b->rev.mod) * 0.5f * bl;
        b->glint.space += (1.0f - b->glint.space) * 0.8f * bl;
        b->sink = u7(v, P_GX_SINK);
    }
    b->bass.model = v[P_BS_MODEL];
    b->bass.drive = u7(v, P_BS_DRIVE);
    b->bass.filter = u7(v, P_BS_FILTER);
    b->bass.resonance = u7(v, P_BS_RESO);
    b->bass.glide = u7(v, P_BS_GLIDE);
    b->bass.level = u7(v, P_BS_LEVEL);
    b->bass.attack = u7(v, P_BS_ATTACK);
    b->bass.release = u7(v, P_BS_RELEASE);
    b->bass.decay = u7(v, P_BS_DECAY);
    b->bass.sustain = u7(v, P_BS_SUSTAIN);
    b->bass.octave = (int)v[P_BS_OCTAVE] - 2;   /* (-2 .. 0) */
}

#include "harmony.c"

/* ---------------------------------------------------------- the words --- */
static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)     /* signed, no padding */
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

/* a time in seconds as the screen writes it: 12 MS, 340 MS, 1.4 S, 12 S, 2:30 */
static void time_text(float s, char *b)
{
    char t[12];
    b[0] = 0;
    if (s < 1.0f) {
        int_text((int32_t)(s * 1000.0f + 0.5f), b);
        text_cat(b, " MS");
    } else if (s < 10.0f) {
        int32_t d = (int32_t)(s * 10.0f + 0.5f);
        int_text(d / 10, b);
        text_cat(b, ".");
        int_text(d % 10, t);
        text_cat(b, t);
        text_cat(b, " S");
    } else if (s < 100.0f) {
        int_text((int32_t)(s + 0.5f), b);
        text_cat(b, " S");
    } else {
        int32_t m = (int32_t)(s + 0.5f);
        int_text(m / 60, b);
        text_cat(b, ":");
        if (m % 60 < 10)
            text_cat(b, "0");
        int_text(m % 60, t);
        text_cat(b, t);
    }
}

/* parameter p at value x, as the screen writes it (the same mappings as the engine's) */
static void value_text(int p, uint8_t x, char *b)
{
    float u = (float)x / (float)PD[p].max;
    b[0] = 0;
    switch (PD[p].fmt) {
    case F_ATT:
        time_text(0.005f * fm_powf(6000.0f, u), b);
        break;
    case F_REL:
        time_text(0.05f * fm_powf(1200.0f, u), b);
        break;
    case F_DECAY:
        time_text(0.4f * fm_powf(150.0f, u), b);
        break;
    case F_TAU:                          /* the drift's time constant */
        time_text(20.0f * fm_powf(0.025f, u), b);
        break;
    case F_PERIOD:                       /* one turn of the tide */
        time_text(300.0f * fm_powf(1.0f / 30.0f, u), b);
        break;
    case F_PREDELAY:
        int_text((int32_t)(u * 200.0f + 0.5f), b);
        text_cat(b, " MS");
        break;
    case F_HZ:
        int_text((int32_t)(20.0f * fm_powf(25.0f, u) + 0.5f), b);
        text_cat(b, " HZ");
        break;
    case F_INTERVAL: {
        static const char *const IV[5] = {"+12", "+7", "+5", "+19", "-12"};
        text_cat(b, IV[x % 5u]);
        break;
    }
    case F_CENTS:
        if (x > 50u)
            text_cat(b, "+");
        int_text((int32_t)x - 50, b + (x > 50u));
        text_cat(b, " CT");
        break;
    case F_ROOT:
        text_cat(b, NOTE_NAME[x % 12u]);
        break;
    case F_SCALE:
        text_cat(b, SCALE_NAME[x % NSCALES]);
        break;
    case F_CHORD:
        text_cat(b, CHORD_NAME[x % NCHORDS]);
        break;
    case F_VOICING: {
        static const char *const VO[3] = {"CLOSE", "OPEN", "WIDE"};
        text_cat(b, VO[x % 3u]);
        break;
    }
    case F_GPRESET:
        text_cat(b, GLINT_NAME[x % NGPRESET]);
        break;
    case F_RATIO:
        text_cat(b, RATIO_NAME[x % 13u]);
        break;
    case F_OCTAVE:
        text_cat(b, "+");
        int_text(x, b + 1);
        break;
    case F_GDECAY:
        time_text(0.08f * fm_powf(75.0f, u), b);
        break;
    case F_GREL:
        time_text(0.02f * fm_powf(300.0f, u), b);
        break;
    case F_DTIME:                        /* the time, then PP: ping-pong */
        text_cat(b, GD_NAME[x % 12u]);
        if (x >= 12u)
            text_cat(b, " PP");
        break;
    case F_MORPH: {
        static const char *const MO[9] = {"CUT", "1 S", "2 S", "4 S", "8 S", "15 S", "30 S", "60 S", "2:00"};
        text_cat(b, MO[x % 9u]);
        break;
    }
    case F_LOAD:
        text_cat(b, x ? "SND ONLY" : "SND+NOTES");
        break;
    case F_UNDO:
    case F_NEWTAKE:
        text_cat(b, x ? "AGAIN" : "-");   /* (turned twice: it acts, and is back at -) */
        break;
    case F_KB: {
        static const char *const KB[3] = {"DRONE", "GLINT", "BASS"};
        text_cat(b, KB[x % 3u]);
        break;
    }
    case F_SQTYPE: {
        static const char *const TY[3] = {"OFF", "ARP", "SEQ"};
        text_cat(b, TY[x % 3u]);
        break;
    }
    case F_DMODE:
        text_cat(b, x ? "PING-PONG" : "MONO");
        break;
    case F_GATTACK:
        time_text(0.002f * fm_powf(750.0f, u), b);
        break;
    case F_BMODEL: {
        static const char *const BM[BASS_NMODEL] = {"SINE", "TRIANGLE", "SQUARE", "SAW", "REESE", "SUB", "ORGAN", "FM"};
        text_cat(b, BM[x % BASS_NMODEL]);
        break;
    }
    case F_HZBASS:                       /* BASS's low-pass */
        int_text((int32_t)(40.0f * fm_powf(125.0f, u) + 0.5f), b);
        text_cat(b, " HZ");
        break;
    case F_GLIDE:
        if (2.0f * u * u < 0.002f)
            text_cat(b, "OFF");
        else
            time_text(2.0f * u * u, b);
        break;
    case F_BOCTAVE:
        int_text((int32_t)x - 2, b);
        break;
    case F_BATT:
        time_text(0.005f * fm_powf(1000.0f, u), b);
        break;
    case F_BREL:
        time_text(0.03f * fm_powf(500.0f, u), b);
        break;
    case F_BDEC:
        time_text(0.01f * fm_powf(1000.0f, u), b);
        break;
    case F_BPULSE:
        text_cat(b, PULSE_NAME[x % NPULSE]);
        break;
    case F_ONOFF:
        text_cat(b, x ? "ON" : "OFF");
        break;
    case F_PCTOFF:
        if (!x)
            text_cat(b, "OFF");
        else
            int_text((int32_t)((u * 100.0f) + 0.5f), b);
        break;
    case F_ARPMODE: {
        static const char *const AM[5] = {"UP", "DOWN", "UPDOWN", "RANDOM", "DRIFT"};
        text_cat(b, AM[x % 5u]);
        break;
    }
    case F_ARPRATE:
        text_cat(b, ARP_RATE_NAME[x % 10u]);
        break;
    case F_RANGE:
        int_text(x + 1, b);
        text_cat(b, " OCT");
        break;
    case F_BPM:
        int_text(x + 30, b);
        text_cat(b, " BPM");
        break;
    case F_KEYS:
        text_cat(b, x ? "HOLD" : "LATCH");
        break;
    case F_PUMP:
        text_cat(b, PUMP_NAME[x % 7u]);
        break;
    case F_PATT:
        time_text(0.001f * fm_powf(300.0f, u), b);
        break;
    case F_SHAPE:
        if (x == 0)
            text_cat(b, "SAW");
        else if (x >= PD[p].max)
            text_cat(b, "SQUARE");
        else
            int_text((int32_t)((u * 100.0f) + 0.5f), b);
        break;
    case F_CINT: {
        static const char *const IV[4] = {"UNISON", "-12", "+7", "+12"};
        text_cat(b, IV[x % 4u]);
        break;
    }
    case F_BPITCH: {
        static const char *const IV[5] = {"-12", "UNISON", "+12", "+19", "+24"};
        text_cat(b, IV[x % 5u]);
        break;
    }
    case F_HARM: {                       /* the harmonic, a decimal: H1.0 .. H8.0 */
        int32_t h = (int32_t)(fm_exp2f(3.0f * u) * 10.0f + 0.5f);
        char *e;
        text_cat(b, "H");
        int_text(h / 10, b + 1);
        text_cat(b, ".");
        for (e = b; *e; e++)
            ;
        int_text(h % 10, e);
        break;
    }
    case F_CTYPE: {
        static const char *const TY[3] = {"LOW", "BAND", "HIGH"};
        text_cat(b, TY[x % 3u]);
        break;
    }
    case F_LSHAPE:
        text_cat(b, LFO_SHAPE_NAME[x % 7u]);
        break;
    case F_LPOL:
        text_cat(b, LFO_POL_NAME[x % 3u]);
        break;
    case F_LTGT:
        text_cat(b, LFO_TGT_NAME[x % NLFO_TGT]);
        break;
    case F_LRATE:                        /* free: a period (under 1 Hz) or Hz; then the grid's */
        if (x >= LFO_NFREE) {
            text_cat(b, LFO_SYNC_NAME[(x - LFO_NFREE) % 12u]);
        } else {
            float hz = lfo_hz(x);
            if (hz >= 1.0f) {
                int32_t d = (int32_t)(hz * 10.0f + 0.5f);
                char *e;
                int_text(d / 10, b);
                text_cat(b, ".");
                for (e = b; *e; e++)
                    ;
                int_text(d % 10, e);
                text_cat(b, " HZ");
            } else {
                time_text(1.0f / hz, b);
            }
        }
        break;
    case F_DPRESET:
        text_cat(b, DRONE_NAME[x % (NDPRESET + 1u)]);
        break;
    case F_SQLEN:
        int_text(x + 1, b);
        break;
    case F_SQNOTE:                       /* REST, TIE, or the note with GLINT's OCTAVE on it (TRANSPOSE aside; C4: 60) */
        if (x == 0)
            text_cat(b, "REST");
        else if (x >= 127u)
            text_cat(b, "TIE");
        else {
            char *e;
            int32_t n = (int32_t)x + seq_shift;
            n = n > 127 ? 127 : n;
            text_cat(b, NOTE_NAME[n % 12]);
            for (e = b; *e; e++)
                ;
            int_text(n / 12 - 1, e);
        }
        break;
    case F_BQNOTE:                       /* CUT, HOLD, or the note with BASS's OCTAVE on it, as it sounds (TRANSPOSE
                                          * aside) */
        if (x == 0)
            text_cat(b, "CUT");
        else if (x >= 127u)
            text_cat(b, "HOLD");
        else {
            char *e;
            int32_t n = (int32_t)x + bseq_shift;
            while (n < 24)
                n += 12;
            n = n > 127 ? 127 : n;
            text_cat(b, NOTE_NAME[n % 12]);
            for (e = b; *e; e++)
                ;
            int_text(n / 12 - 1, e);
        }
        break;
    case F_BQLEN:
        text_cat(b, BQ_LEN_NAME[x % NBQLEN]);
        break;
    case F_SQREC: {
        static const char *const RC[3] = {"OFF", "STEP", "LIVE"};
        text_cat(b, RC[x % 3u]);
        break;
    }
    case F_SQCLEAR: {
        static const char *const CL[3] = {"-", "AGAIN", "CLEARED"};
        text_cat(b, CL[x % 3u]);
        break;
    }
    case F_SQTRANS:                      /* steps of the scale, around 0 */
        if (x > 14u)
            text_cat(b, "+");
        int_text((int32_t)x - 14, b + (x > 14u));
        break;
    case F_SIGNED:                       /* around the middle: 0 there */
        if (x > 64u)
            text_cat(b, "+");
        int_text((int32_t)x - 64, b + (x > 64u));
        break;
    case F_THRESH:
        int_text((int32_t)x - 40, b);
        text_cat(b, " DB");
        break;
    case F_CRATIO:
        text_cat(b, COMP_RATIO_NAME[x % 8u]);
        break;
    case F_CREL:
        time_text(0.03f * fm_powf(50.0f, u), b);
        break;
    case F_DBGAIN:
        text_cat(b, "+");
        int_text(x, b + 1);
        text_cat(b, " DB");
        break;
    case F_LEVEL:
        int_text(x, b);
        break;
    case F_TRKCH:
        if (x == 0)
            text_cat(b, "OFF");
        else if (x > 16u)
            text_cat(b, "OMNI");
        else
            int_text(x, b);
        break;
    case F_CHANNEL:
        if (x == 0)
            text_cat(b, "OMNI");
        else
            int_text(x, b);
        break;
    default:
        int_text((int32_t)((u * 100.0f) + 0.5f), b);
        break;
    }
}
