/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil */
/* The panel's controls as the screens and the screen reader name them (card #111): the FM-1's printed labels, or the
 * SMK-37's. The SMK-37 carries the FM-1's functions on its own buttons (base's device/panel.c PANEL_DEFAULT): SEL on
 * SCALE, ENV on CHORD, LFO on NOTE REPEAT, EDIT on PARA, GLO on GLOBE, HOME on PATCH, SAVE on BT, PLAY and REC on SEQ PLAY
 * and SEQ REC, OCT- and OCT+ on OCTAVE- and OCTAVE+ (its panel's words: fm1-to-smk37 docs/01-hardware.md 5.3); the
 * three left encoders on its knobs K6 (SELECT), K5 (PRESETS) and K7 (ALGORITHM), K1 .. K4 on K1 .. K4, MASTER on K8
 * (APP_SMK_MASTER_APP). A badge is the knob's number on the SMK-37, its letter on the FM-1.
 * Literals, so that a phrase is the compiler's ("HOLD " CN_PLAY): the FM-1's words, and its build, exactly as before */
#pragma once
#if PLAT_SMK37
#define CN_SEL "SCALE"
#define CN_ENV "CHORD"
#define CN_LFO "NOTE REPEAT"
#define CN_EDIT "PARA"
#define CN_GLO "GLOBE"
#define CN_HOME "PATCH"
#define CN_SAVE "BT"
#define CN_PLAY "SEQ PLAY"
#define CN_REC "SEQ REC"
#define CN_OCTDN "OCTAVE-"
#define CN_OCTUP "OCTAVE+"
#define CN_SELECT "K6"                   /* the three left encoders: their knobs */
#define CN_PRESETS "K5"
#define CN_ALGO "K7"
#define CN_BADGE_SELECT '6'              /* ... and their badges (ui.c badge_of) */
#define CN_BADGE_PRESETS '5'
#define CN_BADGE_ALGO '7'
#define CN_DEVICE "SMK-37"
#define CN_KBANK "KNOB BANK"             /* what else moves two hands (Styves): KNOB BANK the track (app.c B_KBANK), the mod
                                          * wheel the track's ALGORITHM setting (smk_wheels; the LEAD's: its vibrato) */
#define CN_MODW "MOD WHEEL"
#else
#define CN_SEL "SEL"
#define CN_ENV "ENV"
#define CN_LFO "LFO"
#define CN_EDIT "EDIT"
#define CN_GLO "GLO"
#define CN_HOME "HOME"
#define CN_SAVE "SAVE"
#define CN_PLAY "PLAY"
#define CN_REC "REC"
#define CN_OCTDN "OCT-"
#define CN_OCTUP "OCT+"
#define CN_SELECT "SELECT"
#define CN_PRESETS "PRESETS"
#define CN_ALGO "ALGORITHM"
#define CN_BADGE_SELECT 'A'
#define CN_BADGE_PRESETS 'B'
#define CN_BADGE_ALGO 'C'
#define CN_DEVICE "FM-1"
#endif
#define CN_FX "FX"                       /* (the same on both) */
#define CN_ARP "ARP"
#define CN_SEQ "SEQ"
#define CN_OCTDN_S "OCT-"                /* short forms, where a line has no room for the SMK-37's whole word (the SEQ pages'
                                          * corner, the help's OCTAVE): the FM-1's, a part of the SMK-37's */
#define CN_OCTUP_S "OCT+"
#define CN_REC_S "REC"
#define FRS_PLAY_NAME CN_PLAY            /* (the engine's words: core.c "HOLD PLAY") */
