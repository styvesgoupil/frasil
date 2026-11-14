/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * Copyright (C) 2026 Charles Vestal (from fm1-base's template) */
/* FRASIL: a drone and texture instrument for the M-VAVE FM-1, on fm1-base.
 * This header is the app's half of the contract with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"
#define APP_NAME "FRASIL"
#define APP_SPLASH_RGB RGB(150, 190, 220)
#define APP_PALETTE 2                    /* CYAN: the screen's steps (gfx.c PALETTES) */
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_SMK_MASTER_APP               /* the SMK-37: MASTER the app's (app.c k8_master: K8, plat_enc_free; never saved),
                                          * K5 PRESETS; fader 4 MIXER's LEAD (Styves) */
#define APP_SMK_PAD_ANSWERS              /* the SMK-37: pads 1, 2 CANCEL, OK (app.c pad_taps; base's restore question too) */
#define FRS_LEAD 1                       /* LEAD, the fourth track (card #144; engine/dsp/lead.h): the FM-1's and the
                                          * SMK-37's (Styves, admin #741: the SMK-37's features on the FM-1 but its
                                          * pads' and controls'); the o2 units' too (fm1.toml o2_cflags), before any
                                          * engine header */
#define FRS_MIDI_OUT 1                   /* MIDI, the fifth track (card #164: app.c mout): the keys out over USB (the
                                          * SMK-37's MIDI OUT jack too), FRASIL playing on; the app's alone, the engine,
                                          * the schema and the scenes as they were */
#ifndef APP_CPU_LINE                     /* the load's line along the screen's last row (ui.c cl_*; Styves, admin #672):
                                          * the FM-1's too (card #147, admin #741) */
#define APP_CPU_LINE 1
#endif
#define APP_OVERLOAD                     /* the render took over 90 % of its time: the engine sheds */
void app_overload(void);
#define APP_LOG_WORD                     /* the log's word (fm1_remote.py log, "app"): the shedding's step now, the
                                          * highest since the last log, anything playing (tools/perf/fm1_load.py) */
uint32_t app_log_word(void);
#define FM1B_UAC_DEEP 1                  /* the USB audio input a half deeper: FRASIL's renders take most of their time,
                                          * and past ~40 % the shallow ring ran dry (frames held: a crackle in recordings,
                                          * issue #1); its 4 KB ring in the pool, 5.8 ms more latency */
#define APP_SPLASH                       /* the logo at power-on (ui.c), before the engine starts */
#define APP_SYSEX                        /* Perform's SysEx (app.c app_sysex, pf_tick): the play told to a page */
void app_splash(void);
#define APP_TICK                         /* 1 kHz, in the scan's interrupt: OCT's soft blink (app.c) */
void app_tick(void);
#define APP_BATTERY_RAW                  /* plat_battery_raw: the divider's reading, the main loop's slow average */
#define APP_HOST_BATT_RAW 0              /* the simulator: no battery until its script says (battery N) */
#define APP_BATT_RAW_4V2 651             /* the battery's divider (plat_battery_raw) at 4.20 V, the header's battery's scale
                                          * (app.c batt): a first guess (a divider in two halves, the converter's 3.3 V)
                                          * until an FM-1 full on USB says (Perform's BATTERY, its raw reading:
                                          * design/perform/scripts/fm1_listen.py) */
#define APP_TAGLINE "DRONE TEXTURES FROZEN IN MOTION"
#define APP_AUTHOR "by Styves Goupil"
#define APP_SITE "frasilsynth.com"               /* the logo's screen, up in the sky */
#define APP_SITE_NOTE "SCENES MANAGER & EDITOR"
/* the store: the app's objects and the panel calibration (the platform's), and their flash offsets. The scenes'
 * bass sequences apart (OBJ_BSEQ: the sixteen scenes no longer fit one object), in the free pair under the scenes; their
 * drone sequences (3.0) apart again (OBJ_DSEQ), in the free pair under that one; the player's presets (OBJ_UPRE, 3.0) in
 * the next; the rest of the scenes' names (OBJ_NAMES, 3.0.1: past their sixteenth character; 3.0.3: and each scene's
 * LFO AMOUNTs and SWELL) in the next */
/* each scene's LEAD (card #144: app.c ldp_*; the FM-1's too, admin #741) in the next pair under the names (0xD0000,
 * inside the data area on both boards: the FM-1's 0x97000..0xDFFFF, the SMK-37's 0xC2000..0xE9FFF; the flash part,
 * Puya or UC25HQ80, the same map), the scene record as it was (the River's, the iPad's, Perform's). The settings pair
 * at PLAT_GLOB: the FM-1's 0xFC000, the SMK-37's 0xEB000 (card #111: its map's), the rest the same on both */
/* each scene's MIDI arpeggio and sequence (card #169: app/mseq.c md_*) in the next pair under the LEAD's (0xCE000, inside
 * the data area on both boards) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_SCENES, OBJ_BSEQ, OBJ_DSEQ, OBJ_UPRE, OBJ_NAMES, OBJ_LEAD, OBJ_MSEQ, OBJ_NOBJ };
#define APP_STORE_MAP {PLAT_GLOB, 0xDC000u, 0xDA000u, 0xD8000u, 0xD6000u, 0xD4000u, 0xD2000u, 0xD0000u, 0xCE000u}
/* backup and restore over USB (fm1.toml backup = true: device/backup.c): the settings and the scenes (their bass
 * sequences and their drone sequences too), never the panel's calibration (the unit's). The version stays 1 when a
 * record's layout changes: each object carries its own magic, and a restore reloads it as a boot does (an earlier
 * layout made over; a backup of an earlier FRASIL, without OBJ_DSEQ: every scene's drone sequence none; without OBJ_UPRE:
 * the player's presets stay as they are, app_backup_restored writes them back; without OBJ_NAMES: every name its first
 * sixteen characters, as that FRASIL had them, the AMOUNTs +100 %, SWELL 0; 3.0.1's OBJ_NAMES, FRNM: the AMOUNTs, SWELL so). An earlier FRASIL refuses a backup with more objects than it has (3.0:
 * this one's, OBJ_NAMES in it) */
#define APP_BACKUP_OBJS {OBJ_SETTINGS, OBJ_SCENES, OBJ_BSEQ, OBJ_DSEQ, OBJ_UPRE, OBJ_NAMES, OBJ_LEAD, OBJ_MSEQ}   /* (OBJ_MSEQ:
                                          * each scene's MIDI arpeggio and sequence, card #169; a backup without it: none,
                                          * every scene's MIDI its defaults; the River's model passes it on as given;
                                          * OBJ_LEAD:
                                          * each scene's LEAD and the player's LEAD presets, card #144; a FRASIL before
                                          * it refuses such a backup; one of it, without OBJ_LEAD: every scene's LEAD
                                          * MOSS, app.c ldp_load) */
#define APP_BACKUP_MAGIC 0x4B425246u     /* "FRBK" */
#define APP_BACKUP_VERSION 1u
#define APP_BACKUP_WHAT "replaces all scenes"
#define APP_BACKUP_YES B_SEL               /* the restore question: SEL yes, FX no, as every question here */
#if PLAT_SMK37
#define APP_BACKUP_YES_NAME "SCALE"      /* (the SMK-37's SEL, as its panel names it: controls.h; its pads too) */
#else
#define APP_BACKUP_YES_NAME "SEL"
#endif
#define APP_BACKUP_NO B_FX
#define APP_BACKUP_NO_NAME "FX"
#define APP_BACKUP_ASK                   /* ... drawn as a pop-up (ui.c) */
void app_backup_ask_screen(void);
int app_backup_playing(void);            /* something held or playing: no restore (STOP FIRST) */
void app_backup_flush(void);             /* a backup starts: MORPH and KEEP kept, if they changed */
void app_backup_hold(int on);            /* while a backup streams: nothing saved (a learned CC kept for after) */
void app_backup_restored(void);          /* everything from the store again, as at power-on */
/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved state (or the first scene) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */
#define APP_WEB                          /* the browser build's own exports (web.c): the River plays a shard */
/* the host simulator's checks (host/plat_host.c "expect") */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_SPEAKDUMP               /* "speakdump": every page, value, row and help card as a screen reader gets them
                                          * (speak.c, fm1-reader's audit) */
void app_speak_dump(void);
#define APP_HOST_CMD                     /* "oldstore": records in the layout before BASS (app.c) */
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#define APP_HOST_FAILS                   /* the app's own checks' failures (PERFORM's LIVE: pfop, pfcheck, pfpack) */
int app_host_fails(void);
