/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* FRASIL's settings, numbered: each one a byte, 0 .. its max (frs_param_info; engine/params.c has their ranges,
 * defaults and words). A scene keeps the first NSOUND of them; the rest are the engine's state, the FM-1's or of the
 * moment (frs_param_kind). The numbers are the River's schema's ("ids"), the MIDI learns' and the backups': a setting
 * added goes before NSOUND if a scene keeps it, else before NPARAM, and the ones after it move (the stores made over by
 * the FM-1's app.c layouts). For a platform's UI: the settings by name, P_EQ_LOW rather than 131 */
#pragma once

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
    P_DR_PRESET, P_DR_FULL,              /* the drone's preset (0: CUSTOM), its BODY (DRONE TONE) */
    P_L3_SHAPE, P_L3_RATE, P_L3_DEPTH, P_L3_POL, P_L3_T1, P_L3_T2, P_L3_T3, P_L3_T4,   /* LFO 3 */
    P_SQ_TYPE,                           /* GLINT's PLAYS: KEYS, ARP (the arpeggiator), SEQ (the sequencer) */
    P_GX_SPARK, P_GX_BLUR, P_GX_SINK,    /* GENERATIVE FX (SWELL beside them, past NSOUND): glints by themselves, the
                                          * drone into the cloud, everything under */
    P_BS_DECAY, P_BS_SUSTAIN, P_BS_PULSE,   /* BASS III's DECAY and SUSTAIN (between its ATTACK and RELEASE), BASS II's
                                             * PULSE: the note let go and played again on the grid */
    P_EQ_PRESET, P_EQ_LOW, P_EQ_MID, P_EQ_HIGH,    /* MASTER EQ: OFF, CUSTOM or a preset; the three bands' gains */
    P_EQ_LOWF, P_EQ_MIDF, P_EQ_Q, P_EQ_HIGHF,      /* MASTER EQ II: where they are, the MID's width */
    P_FL_TYPE, P_FL_CUT, P_FL_RESO, P_FL_TRACKS,   /* MASTER FILTER: OFF, LOW PASS, HIGH PASS on the tracks chosen */
    P_TP_ON, P_TP_SAT, P_TP_WOBBLE, P_TP_TRACKS,   /* LOFI TAPE: on, its saturation, its wobble (and hiccups), its tracks */
    P_FL_PRE, P_TP_PRE, P_PUMP_CLOUD,    /* ROUTING: MASTER FILTER and LOFI TAPE before the cloud or after it, PUMP on the
                                          * cloud's return */
    P_PN_DRONE, P_PN_BASS, P_PN_GLINT, P_PN_CLOUD,   /* MIXER's pans (OCT- held, K1 .. K4) */
    P_CH_SOFT,                           /* CHOIR III: its wave softened (the saws integrated, once, then twice) */
    NSOUND,                              /* a scene ends here; the rest are settings that pages show */
    P_KEYS = NSOUND,
    P_CANAL,
    P_TEMPO,                             /* the arpeggiator's, when no MIDI clock comes in */
    P_FREEZE,                            /* the cloud held (not kept: the FM-1 starts thawed) */
    P_CH_GLINT, P_CH_BASS,               /* GLINT's and BASS's MIDI channels (P_CANAL: the drones') */
    P_SQ_STEP, P_SQ_NOTE, P_SQ_ACC, P_SQ_REC, P_SQ_CLEAR,   /* GLINT SEQ's editing: the step on K1, its note and
                                                             * accent (seq[]'s, shown), RECORD, CLEAR (not kept) */
    P_SWELL,                             /* GENERATIVE FX's K1: every drone voice swelled, as far as it is turned
                                          * (kept in the scene since "FRS}": scene_t's swell; ENV held and MIDI's CC 11
                                          * swell it too, of the moment) */
    P_KB,                                /* SELECT's: what the keys play, DRONE, GLINT, BASS (not kept) */
    P_MORPH, P_LOAD,                     /* SCENES' K2, K3: how a scene comes in (LOAD kept with the settings;
                                          * MORPH on CUT at every start) */
    P_SC_GONE,                           /* (SCENES' UNDO, gone: its number kept, so that the ones after it stay as
                                          * the settings' MIDI LEARN and the River's backups have them; no page) */
    P_SQ_NEW, P_SQ_UNDO,                 /* GLINT SEQ III's K3, K4: a new take, the last take or CLEAR undone */
    P_BQ_STEP, P_BQ_NOTE, P_BQ_LEN, P_BQ_REC,   /* BASS SEQ: the step on K1, its note (the bass's sequence's, shown);
                                                 * RECORD (not kept; REC's, as GLINT's); RATE, the one grid of the bass's
                                                 * arpeggio and its sequence, kept in a scene (2.7: each step's own till
                                                 * then; BASS SEQ II's K2 and BASS ARP's now) */
    P_BQ_STEPS, P_BQ_ON, P_BQ_CLEAR, P_BQ_TRANS,   /* BASS SEQ II's SEQ LENGTH: the steps it plays; BASS SEQ III's CLEAR
                                                    * (not kept), TRANSPOSE; BASS I's PLAYS: KEYS, ARP, SEQ, as GLINT's (in
                                                    * every layout before FRSv 1: its sequence on, 2 now); SEQ LENGTH,
                                                    * PLAYS and TRANSPOSE kept in a scene (scene_t's bq_steps, bq_on,
                                                    * bq_trans) */
    P_SPEAKER,                           /* ROUTING's K4, SPK/JACK: the FM-1's own output on or quiet (the USB audio goes
                                          * on); kept with the settings */
    P_ANIM,                              /* SCREEN's K1, ANIMATIONS: NORMAL, or LOW (what only decorates, still); kept with
                                          * the settings */
    P_CLOCK,                             /* MIDI's K4, CLOCK: AUTO (a MIDI clock coming in leads the grid) or INTERNAL
                                          * (TEMPO's, always: clocks and Start in ignored); kept with the settings */
    P_BQ_NLEN,                           /* BASS SEQ's K3, LENGTH: the note on STEP's, in steps (its HOLDs; shown, not
                                          * kept: 2.7, numbered last) */
    P_BA_MODE, P_BA_RANGE, P_BA_CHANCE,  /* BASS ARP's MODE, RANGE, CHANCE (CHANCE its sequence's too, as GLINT's), kept
                                          * in the scene (not in its sound: numbered last) */
    P_BQ_GATE, P_BQ_ACC, P_BQ_NEW, P_BQ_UNDO,   /* BASS SEQ II's GATE (kept in the scene), BASS SEQ's ACCENT (its step's,
                                                 * shown), BASS SEQ III's NEW TAKE and UNDO TAKE */
    P_SQ_NLEN,                           /* GLINT SEQ's K3, LENGTH: the note on STEP's, in steps (its TIEs; shown, not
                                          * kept) */
    P_BS_WEIGHT, P_BS_PUNCH, P_BS_BITE,  /* BASS TONE's K2 .. K4, kept in the scene (not in its sound: numbered last) */
    P_BS_SPACE,                          /* CLOUD SEND's K3: the bass's share of the cloud, kept in the scene (numbered last) */
    P_GL_VIB_AUTO,                       /* GLINT VIBRATO's K1: every glint's vibrato by itself, kept in the scene (numbered last) */
    P_DQ_ON, P_DQ_RATE, P_DQ_LEN, P_DQ_GATE, P_DQ_CHANCE, P_DQ_TRANS,   /* DRONE SEQ (3.0): its sequence on, RATE, SEQ
                                                                         * LENGTH, GATE, CHANCE, TRANSPOSE, kept in the
                                                                         * scene (scene_t's dq_on ...; numbered last) */
    P_DQ_REC, P_DQ_CLEAR, P_DQ_STEP,     /* its RECORD (OFF, STEP, LIVE), CLEAR (turned twice), the step STEP writes into
                                          * (not kept; no FM-1 page yet) */
    P_DQ_NOTES, P_DQ_NLEN, P_DQ_UNDO,    /* DRONE SEQ II's NOTES (the step on STEP: REST, its notes, TIE) and LENGTH (its
                                          * chord's, in steps: its TIEs), shown, not kept; DRONE SEQ III's UNDO TAKE (3.0,
                                          * numbered last) */
    P_GA_LATCH, P_BA_LATCH,              /* ARP II's LATCH: GLINT's arpeggio's, the bass's (their notes stay when the keys
                                          * let go; ARP held turns it), of the moment (3.0) */
    P_KY_DR_HARM, P_KY_DR_NOTES, P_KY_DR_LATCH, P_KY_DR_STACK,   /* KEYS (the iPad's TOUCH), each layer's: HARMONY (OFF:
                                                                  * the keys chromatic, no chord), NOTES (SINGLE NOTE,
                                                                  * CHORD), LATCH (OFF: HOLD, the keys sound while down;
                                                                  * GLINT's: the ring of four), STACK (OFF: SWAP, a new
                                                                  * key or chord in place of what is held); */
    P_KY_GL_HARM, P_KY_GL_NOTES, P_KY_GL_LATCH, P_KY_GL_STACK,   /* the drone's, GLINT's, the bass's (P_KY_DR_HARM + 4 x
                                                                  * the layer + 0 .. 3), kept with the settings */
    P_KY_BS_HARM, P_KY_BS_NOTES, P_KY_BS_LATCH, P_KY_BS_STACK,   /* (frs_state_save: the device's, not a scene's; 3.0) */
    P_METRO, P_COUNT_IN,                 /* GLO › RECORD: METRONOME (OFF, REC: a take's ticks, ALWAYS: the grid's beats
                                          * too), COUNT-IN (OFF, 1 BAR: a LIVE take from silence counted in), the FM-1's
                                          * (kept with its settings; 3.0) */
    P_ARP_SNAP,                          /* GLOBAL's K4, ARP SNAP: a new chord into an arpeggio (GLINT's, the bass's) taken
                                          * on this grid, OFF (at its next step, as before), 1/16, 1/8, 1/4, 1/2, 1 BAR; the
                                          * device's (kept with the settings, frs_state_save; 3.0) */
    P_SAVER,                             /* SCREEN's K2, SCREENSAVER: ON (after ten minutes without a touch, the screen
                                          * black, the logo drifting), OFF; the FM-1's, kept with its settings (3.0) */
    P_TP_FLUTTER, P_TP_TONE, P_TP_NOISE, P_TP_DROPOUT,   /* LOFI TAPE II: TAPE SPEED (FLUTTER RATE's until it), the tone, the hiss, the
                                          * dropouts; kept in the scene, after its drone's sequence (scene_t's tp_flutter ..;
                                          * numbered last; LFO targets since 3.2: TAPE SPEED, TAPE TONE, TAPE NOISE,
                                          * DROPOUT) */
    /* LFO AMOUNT: each LFO's on each of its TARGETs (P_L1_AMT1 + 4 x the LFO + the TARGET's 0 .. 3), a multiplier
     * (0 .. 200: x0.00 .. x1.00 by 0.01, then .. x5.00 by 0.04; 100, x1.00, as the LFOs moved them before); kept in the scene, after LOFI TAPE II's four (scene_t's
     * lfo_amt; numbered last; not LFO targets) */
    P_L1_AMT1, P_L1_AMT2, P_L1_AMT3, P_L1_AMT4,
    P_L2_AMT1, P_L2_AMT2, P_L2_AMT3, P_L2_AMT4,
    P_L3_AMT1, P_L3_AMT2, P_L3_AMT3, P_L3_AMT4,
    NPARAM
};
