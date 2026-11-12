/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The FM-1's pages: each a button's, its four settings on K1 .. K4 (engine/params.c has the settings). Part of the
 * unity build (app.c includes it, after the engine) */

/* the pages: a page button, the track it belongs to (T_ANY: every track's), and the four parameters K1 .. K4 turn; a
 * button with more than one page steps through them, in PAGE_ORDER, those of the selected track. Numbered as they came
 * (PERFORM and the iPad's FM1Remote have the numbers), new ones last */
enum { PG_SON, PG_DRONE, PG_CHOEUR, PG_CHOEUR2, PG_CHOEUR3, PG_PARTIELS, PG_PARTIELS2, PG_SOUFFLE, PG_SOUFFLE2, PG_GLINT, PG_GLINT2, PG_GLINT3, PG_GDELAY, PG_ARP, PG_SEQ, PG_SEQ2, PG_SEQ3, PG_BASS,
       PG_BASS2,
       PG_BASS3, PG_FORME, PG_MOUVEMENT, PG_LFO1, PG_LFO1T, PG_LFO2, PG_LFO2T, PG_LFO3, PG_LFO3T, PG_HARMONY, PG_GENFX, PG_NUAGE, PG_NUAGE2, PG_NUAGE3, PG_FILTER, PG_TAPE, PG_PUMP, PG_PUMP2, PG_GLOBAL, PG_GLOBAL2,
       PG_MASTER, PG_EQ, PG_EQ2, PG_MIXER, PG_SCENES,
       PG_BSEQ, PG_BSEQ2,
       PG_ROUTING, PG_SCREEN,
       PG_BARP, PG_BSEQ3, PG_BDRIVE,
       PG_CSEND,
       PG_ARP2, PG_BARP2,                /* ARP II: CHANCE, LATCH (3.0) */
       PG_DSEQ, PG_DSEQ2, PG_DSEQ3,      /* the drone's sequence */
       PG_NOARP,                         /* ARP with DRONE selected: NOT AVAILABLE FOR DRONE */
       PG_KEYS,                          /* HOME's second: the selected track's HARMONY, NOTES, LATCH, STACK */
       PG_RECORD,                        /* GLO's last: METRONOME, COUNT-IN, TUNE A4 */
       PG_SAVE, PG_SAVEAS,               /* SAVE: LOAD or SAVE; WHAT, INTO (app.c) */
       PG_TAPE2,                         /* LOFI TAPE II: TAPE SPEED, TONE, NOISE, DROPOUT (skipped while TAPE is off) */
#if FRS_LEAD
       PG_LEAD, PG_LEAD2, PG_LDELAY,     /* LEAD's (card #144, the SMK-37's alone for now): EDIT's three, */
       PG_LADSR, PG_LNOARP, PG_LNOSEQ,   /* ENV's, ARP's and SEQ's (NOT AVAILABLE FOR LEAD); (MIXER II gone: its
                                          * level MIXER's K4, admin #744; its load the CPU line) */
       PG_LEAD3,                         /* EDIT's LEAD III (MODE, DETUNE, BREATH, DRIVE), after LEAD II */
       PG_LFENV,                         /* ENV's LEAD FILTER: FILTER ENV's own ADSR, after LEAD ADSR (admin #619) */
#endif
       NPAGES };
#define T_ANY (3u + FRS_LEAD)            /* (LEAD the fourth track, 3, with FRS_LEAD) */
#define K_ARP (NPARAM + 1u)              /* K1 of ARP and SEQ: ON, OFF, over the track's PLAYS (app.c ways_turn) */
#define K_SEQ (NPARAM + 2u)
#define K_IDLE (NPARAM + 3u)             /* SCREEN's K3, IDLE TIME: the screensaver's minutes (app.c saver.min; no setting
                                          * in the schema: kept in SCREENSAVER's byte, saver_byte) */
#define K_HEAD (NPARAM + 4u)             /* SCREEN's K4, HEADER: INVERT or DARK (app.c hdr; no setting in the schema
                                          * either: settings_t.head) */
#define K_TUNE (NPARAM + 5u)             /* RECORD's K3, TUNE A4: 415 .. 466 Hz (app.c a4; the device's, no setting in the
                                          * schema: settings_t.a4) */
#if FRS_LEAD
typedef uint16_t prm_t;                  /* a setting's number on a page, the one turned last: past 255 with LEAD's (card #144) */
#else
typedef uint8_t prm_t;
#endif
typedef struct {
    const char *name;
    uint8_t button;
    uint8_t track;                       /* 0 DRONE, 1 GLINT, 2 BASS, (3 LEAD,) T_ANY */
    prm_t p[4];
} page_t;
static const page_t PAGES[NPAGES] = {
    [PG_SON] = {"SOUND", B_HOME, T_ANY, {P_CHOEUR, P_PARTIELS, P_SOUFFLE, P_CHALEUR}},
    [PG_DRONE] = {"DRONE TONE", B_EDIT, 0, {P_DR_PRESET, P_TIMBRE, P_ESPACE, P_DR_FULL}},
    [PG_MOUVEMENT] = {"DRONE MOTION", B_EDIT, 0, {P_MOUVEMENT, P_VITESSE, P_MAREE, P_PERIODE}},   /* EDIT's second on DRONE */
    [PG_CHOEUR] = {"CHOIR", B_EDIT, 0, {P_CHOEUR, P_DESACCORD, P_FILTRE, P_CHALEUR}},
    [PG_CHOEUR2] = {"CHOIR II", B_EDIT, 0, {P_CH_SHAPE, P_CH_INTERVAL, P_CH_RESO, P_CH_TYPE}},   /* the source, the filter */
    [PG_CHOEUR3] = {"CHOIR III", B_EDIT, 0, {P_CH_SOFT, NPARAM, NPARAM, NPARAM}},   /* the wave softened (ui.c choir_scope) */
    [PG_PARTIELS] = {"PARTIALS", B_EDIT, 0, {P_PARTIELS, P_PENTE, P_IMPAIRS, P_ETIREMENT}},
    [PG_PARTIELS2] = {"PARTIALS II", B_EDIT, 0, {P_PA_EVOLVE, P_PA_BEAT, P_PA_FORMANT, P_PA_PEAK}},
    [PG_SOUFFLE] = {"BREATH", B_EDIT, 0, {P_SOUFFLE, P_RESONANCE, P_COULEUR, P_ECART}},
    [PG_SOUFFLE2] = {"BREATH II", B_EDIT, 0, {P_BR_GRAIN, P_BR_PITCH, P_BR_TONE, P_BR_AIR}},
    [PG_GLINT] = {"GLINT I", B_EDIT, 1, {P_GL_PRESET, P_GL_LEVEL, P_GL_BRIGHT, P_GL_RATIO}},
    [PG_GLINT2] = {"GLINT II", B_EDIT, 1, {P_GL_OCTAVE, P_GL_PING, P_GL_VIBRATO, P_GL_VIB_AUTO}},
    [PG_GLINT3] = {"GLINT ADSR", B_ENV, 1, {P_GL_ATTACK, P_GL_DECAY, P_GL_SUSTAIN, P_GL_RELEASE}},   /* (ui.c adsr_scope) */
    [PG_GDELAY] = {"GLINT DELAY", B_EDIT, 1, {P_GD_TIME, P_GD_FB, P_GD_MIX, P_GL_SPACE}},   /* and the reverb */
    [PG_ARP] = {"GLINT ARP", B_ARP, 1, {K_ARP, P_ARP_MODE, P_ARP_RATE, P_ARP_RANGE}},
    [PG_ARP2] = {"GLINT ARP II", B_ARP, 1, {P_ARP_CHANCE, P_GA_LATCH, NPARAM, NPARAM}},
    [PG_SEQ] = {"GLINT SEQ", B_SEQ, 1, {K_SEQ, P_SQ_LEN, P_ARP_RATE, P_SQ_GATE}},
    [PG_SEQ2] = {"GLINT SEQ II", B_SEQ, 1, {P_SQ_STEP, P_SQ_NOTE, P_SQ_NLEN, P_SQ_ACC}},   /* the steps (RECORD: REC's) */
    [PG_SEQ3] = {"GLINT SEQ III", B_SEQ, 1, {P_ARP_CHANCE, P_SQ_TRANS, P_SQ_UNDO, NPARAM}},
    [PG_BASS] = {"BASS I", B_EDIT, 2, {P_BS_MODEL, P_BS_LEVEL, P_BS_FILTER, P_BS_RESO}},
    [PG_BASS2] = {"BASS II", B_EDIT, 2, {P_BS_OCTAVE, P_BS_GLIDE, P_BS_PULSE, P_BS_SPACE}},
    [PG_BASS3] = {"BASS ADSR", B_ENV, 2, {P_BS_ATTACK, P_BS_DECAY, P_BS_SUSTAIN, P_BS_RELEASE}},   /* (ui.c adsr_scope) */
    [PG_BDRIVE] = {"BASS TONE", B_EDIT, 2, {P_BS_DRIVE, P_BS_WEIGHT, P_BS_PUNCH, P_BS_BITE}},
    [PG_BARP] = {"BASS ARP", B_ARP, 2, {K_ARP, P_BA_MODE, P_BQ_LEN, P_BA_RANGE}},
    [PG_BARP2] = {"BASS ARP II", B_ARP, 2, {P_BA_CHANCE, P_BA_LATCH, NPARAM, NPARAM}},
    [PG_BSEQ] = {"BASS SEQ", B_SEQ, 2, {K_SEQ, P_BQ_STEPS, P_BQ_LEN, P_BQ_GATE}},
    [PG_BSEQ2] = {"BASS SEQ II", B_SEQ, 2, {P_BQ_STEP, P_BQ_NOTE, P_BQ_NLEN, P_BQ_ACC}},   /* the steps (RECORD: REC's) */
    [PG_BSEQ3] = {"BASS SEQ III", B_SEQ, 2, {P_BA_CHANCE, P_BQ_TRANS, P_BQ_UNDO, NPARAM}},
    [PG_DSEQ] = {"DRONE SEQ", B_SEQ, 0, {P_DQ_ON, P_DQ_LEN, P_DQ_RATE, P_DQ_GATE}},
    [PG_DSEQ2] = {"DRONE SEQ II", B_SEQ, 0, {P_DQ_STEP, P_DQ_NOTES, P_DQ_NLEN, P_KY_DR_STACK}},   /* (no ACCENT: KEYS' STACK, at hand) */
    [PG_DSEQ3] = {"DRONE SEQ III", B_SEQ, 0, {P_DQ_CHANCE, P_DQ_TRANS, P_DQ_UNDO, NPARAM}},
    [PG_NOARP] = {"DRONE ARP", B_ARP, 0, {NPARAM, NPARAM, NPARAM, NPARAM}},
    [PG_KEYS] = {"KEYS", B_HOME, T_ANY, {NPARAM, NPARAM, NPARAM, NPARAM}},   /* (the selected track's: app.c page_knob) */
    [PG_SCREEN] = {"SCREEN", B_GLO, T_ANY, {P_ANIM, P_SAVER, K_IDLE, K_HEAD}},   /* (SCREENSAVER: app.c saver) */
    [PG_ROUTING] = {"ROUTING", B_GLO, T_ANY, {P_FL_PRE, P_TP_PRE, P_PUMP_CLOUD, P_SPEAKER}},   /* (SPK/JACK: the settings') */
    [PG_RECORD] = {"RECORD", B_GLO, T_ANY, {P_METRO, P_COUNT_IN, K_TUNE, NPARAM}},   /* (TUNE A4: the device's, as the
                                                                                    * metronome is) */
    [PG_FORME] = {"DRONE SHAPE", B_ENV, 0, {P_ATTAQUE, P_RELACHE, P_PRESSION, P_VELOCITE}},
    [PG_LFO1] = {"LFO 1", B_LFO, T_ANY, {P_L1_SHAPE, P_L1_RATE, P_L1_DEPTH, P_L1_POL}},
    [PG_LFO1T] = {"LFO 1 TARGETS", B_LFO, T_ANY, {P_L1_T1, P_L1_T2, P_L1_T3, P_L1_T4}},
    [PG_LFO2] = {"LFO 2", B_LFO, T_ANY, {P_L2_SHAPE, P_L2_RATE, P_L2_DEPTH, P_L2_POL}},
    [PG_LFO2T] = {"LFO 2 TARGETS", B_LFO, T_ANY, {P_L2_T1, P_L2_T2, P_L2_T3, P_L2_T4}},
    [PG_LFO3] = {"LFO 3", B_LFO, T_ANY, {P_L3_SHAPE, P_L3_RATE, P_L3_DEPTH, P_L3_POL}},
    [PG_LFO3T] = {"LFO 3 TARGETS", B_LFO, T_ANY, {P_L3_T1, P_L3_T2, P_L3_T3, P_L3_T4}},
    [PG_HARMONY] = {"HARMONY", B_SEL, T_ANY, {P_ROOT, P_SCALE, P_CHORD, P_VOICING}},
    [PG_GENFX] = {"GENERATIVE FX", B_FX, T_ANY, {P_SWELL, P_GX_SPARK, P_GX_BLUR, P_GX_SINK}},   /* FX's first: to perform */
    [PG_NUAGE] = {"CLOUD", B_FX, T_ANY, {P_TAILLE, P_DUREE, P_TEINTE, P_FREEZE}},
    [PG_NUAGE2] = {"CLOUD II", B_FX, T_ANY, {P_DIFFUSION, P_MODULATION, P_PREDELAI, P_LARGEUR}},
#if FRS_LEAD                             /* the shimmer, and the cloud's return (MIXER's K4 before the LEAD, Styves admin
                                          * #744: on the cloud's own page; OCT- held, its PAN, as MIXER's) */
    [PG_NUAGE3] = {"CLOUD III", B_FX, T_ANY, {P_SHIMMER, P_INTERVALLE, P_SH_TONE, P_MX_CLOUD}},
#else
    [PG_NUAGE3] = {"CLOUD III", B_FX, T_ANY, {P_SHIMMER, P_INTERVALLE, P_SH_TONE, NPARAM}},   /* the shimmer */
#endif
    [PG_FILTER] = {"MASTER FILTER", B_FX, T_ANY, {P_FL_TYPE, P_FL_CUT, P_FL_RESO, P_FL_TRACKS}},   /* before PUMP */
    [PG_TAPE] = {"LOFI TAPE", B_FX, T_ANY, {P_TP_ON, P_TP_SAT, P_TP_WOBBLE, P_TP_TRACKS}},     /* after the filter */
    [PG_TAPE2] = {"LOFI TAPE II", B_FX, T_ANY, {P_TP_FLUTTER, P_TP_TONE, P_TP_NOISE, P_TP_DROPOUT}},   /* after LOFI TAPE */
    [PG_PUMP] = {"PUMP", B_FX, T_ANY, {P_PUMP_RATE, P_PUMP_DRONE, P_PUMP_BASS, P_PUMP_GLINT}},
    [PG_PUMP2] = {"PUMP II", B_FX, T_ANY, {P_PUMP_REL, P_PUMP_ATT, P_PUMP_OFF, NPARAM}},
    [PG_GLOBAL] = {"GLOBAL", B_GLO, T_ANY, {P_TEMPO, P_COUPEBAS, P_ACCORD, P_ARP_SNAP}},   /* (ARP SNAP: the device's) */
    [PG_GLOBAL2] = {"MIDI", B_GLO, T_ANY, {P_CANAL, P_CH_GLINT, P_CH_BASS, P_CLOCK}},
    [PG_MASTER] = {"MASTERING", B_GLO, T_ANY, {P_MS_THRESH, P_MS_RATIO, P_MS_RELEASE, P_MS_GAIN}},
    [PG_EQ] = {"MASTER EQ", B_GLO, T_ANY, {P_EQ_PRESET, P_EQ_LOW, P_EQ_MID, P_EQ_HIGH}},   /* before the compressor */
    [PG_EQ2] = {"MASTER EQ II", B_GLO, T_ANY, {P_EQ_LOWF, P_EQ_MIDF, P_EQ_Q, P_EQ_HIGHF}},
#if FRS_LEAD                             /* HOME's third (app.c): each track's level, K4 the LEAD's (Styves, admin #744;
                                          * the cloud's on CLOUD III) */
    [PG_MIXER] = {"MIXER", B_HOME, T_ANY, {P_MX_DRONE, P_MX_BASS, P_MX_GLINT, P_MX_LEAD}},
#else
    [PG_MIXER] = {"MIXER", B_HOME, T_ANY, {P_MX_DRONE, P_MX_BASS, P_MX_GLINT, P_MX_CLOUD}},   /* HOME's third (app.c) */
#endif
#if FRS_LEAD                             /* each track's share of the cloud (ui.c cell_name); K4 LEAD's */
    [PG_CSEND] = {"CLOUD SEND", B_HOME, T_ANY, {P_ESPACE, P_GL_SPACE, P_BS_SPACE, P_LD_SPACE}},
#else
    [PG_CSEND] = {"CLOUD SEND", B_HOME, T_ANY, {P_ESPACE, P_GL_SPACE, P_BS_SPACE, NPARAM}},   /* each track's share of the cloud
                                                                                                * (ui.c cell_name) */
#endif
    [PG_SCENES] = {"SCENES", B_SAVE, T_ANY, {NPARAM, P_MORPH, P_LOAD, NPARAM}},   /* (K1: the slot, app.c) */
    [PG_SAVE] = {"SCENES", B_SAVE, T_ANY, {NPARAM, NPARAM, NPARAM, NPARAM}},      /* (K1: LOAD | SAVE, app.c) */
    [PG_SAVEAS] = {"SAVE", B_SAVE, T_ANY, {NPARAM, NPARAM, NPARAM, NPARAM}},      /* (K1 WHAT, K2 INTO, app.c) */
#if FRS_LEAD
    [PG_LEAD] = {"LEAD I", B_EDIT, 3, {P_LD_PRESET, P_LD_WAVE, P_LD_CUT, P_LD_RESO}},
    [PG_LEAD2] = {"LEAD II", B_EDIT, 3, {P_LD_GLIDE, P_LD_FENV, P_LD_VIB, P_LD_OCTAVE}},
    [PG_LEAD3] = {"LEAD III", B_EDIT, 3, {P_LD_MODE, P_LD_DETUNE, P_LD_BREATH, P_LD_DRIVE}},
    [PG_LDELAY] = {"LEAD DELAY", B_EDIT, 3, {P_LD_DTIME, P_LD_DFB, P_LD_DMIX, P_LD_SPACE}},   /* and the cloud */
    [PG_LADSR] = {"LEAD ADSR", B_ENV, 3, {P_LD_ATTACK, P_LD_DECAY, P_LD_SUSTAIN, P_LD_RELEASE}},   /* (ui.c adsr_scope) */
    [PG_LFENV] = {"LEAD FILTER", B_ENV, 3, {P_LD_FATTACK, P_LD_FDECAY, P_LD_FSUSTAIN, P_LD_FRELEASE}},   /* (the same) */
    [PG_LNOARP] = {"LEAD ARP", B_ARP, 3, {NPARAM, NPARAM, NPARAM, NPARAM}},   /* NOT AVAILABLE FOR LEAD */
    [PG_LNOSEQ] = {"LEAD SEQ", B_SEQ, 3, {NPARAM, NPARAM, NPARAM, NPARAM}},
#endif
};
_Static_assert(K_TUNE <= (prm_t)-1, "the pages' own knobs, past the settings, fit a page's bytes");
#if FRS_LEAD
#define LD_(...) , __VA_ARGS__              /* (LEAD's pages in the lists below) */
#else
#define LD_(...)
#endif
/* the order a button's taps go through its pages, and SELECT with the button held */
static const uint8_t PAGE_ORDER[NPAGES] = {
    PG_SON, PG_KEYS, PG_MIXER, PG_CSEND,
    PG_DRONE, PG_MOUVEMENT, PG_CHOEUR, PG_CHOEUR2, PG_CHOEUR3, PG_PARTIELS, PG_PARTIELS2, PG_SOUFFLE, PG_SOUFFLE2,
    PG_GLINT, PG_GLINT2, PG_GDELAY, PG_BASS, PG_BASS2, PG_BDRIVE LD_(PG_LEAD, PG_LEAD2, PG_LEAD3, PG_LDELAY),
    PG_FORME, PG_GLINT3, PG_BASS3 LD_(PG_LADSR, PG_LFENV),
    PG_NOARP, PG_ARP, PG_ARP2, PG_BARP, PG_BARP2 LD_(PG_LNOARP),
    PG_DSEQ, PG_DSEQ2, PG_DSEQ3, PG_SEQ, PG_SEQ2, PG_SEQ3, PG_BSEQ, PG_BSEQ2, PG_BSEQ3 LD_(PG_LNOSEQ),
    PG_LFO1, PG_LFO1T, PG_LFO2, PG_LFO2T, PG_LFO3, PG_LFO3T,
    PG_HARMONY,
    PG_GENFX, PG_NUAGE, PG_NUAGE2, PG_NUAGE3, PG_FILTER, PG_TAPE, PG_TAPE2, PG_PUMP, PG_PUMP2,
    PG_GLOBAL, PG_GLOBAL2, PG_MASTER, PG_EQ, PG_EQ2, PG_ROUTING, PG_SCREEN, PG_RECORD,
    PG_SAVE, PG_SAVEAS, PG_SCENES,
};
/* SELECT turned on a track's page: the same place on the new track's (first if hidden) */
static const uint8_t TRACK_PAGES[][3 + FRS_LEAD] = {
    {PG_FORME, PG_GLINT3, PG_BASS3 LD_(PG_LADSR)},
#if FRS_LEAD
    {PG_FORME, PG_GLINT3, PG_BASS3, PG_LFENV},   /* (LEAD FILTER left: the others' ENV page) */
#endif
    {PG_NOARP, PG_ARP, PG_BARP LD_(PG_LNOARP)},
    {PG_NOARP, PG_ARP2, PG_BARP2 LD_(PG_LNOARP)},
    {PG_DSEQ, PG_SEQ, PG_BSEQ LD_(PG_LNOSEQ)},
    {PG_DSEQ2, PG_SEQ2, PG_BSEQ2 LD_(PG_LNOSEQ)},
    {PG_DSEQ3, PG_SEQ3, PG_BSEQ3 LD_(PG_LNOSEQ)},
};
