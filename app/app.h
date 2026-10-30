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
#define APP_TAGLINE "DRONE TEXTURES FROZEN IN MOTION"
#define APP_AUTHOR "by Styves Goupil"
#define APP_SITE "frasilsynth.com"               /* the logo's screen, up in the sky */
#define APP_SITE_NOTE "SCENES MANAGER & EDITOR"
/* the store: the app's objects and the panel calibration (the platform's), and their flash offsets. The scenes'
 * bass sequences apart (OBJ_BSEQ: the sixteen scenes no longer fit one object), in the free pair under the scenes */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_SCENES, OBJ_BSEQ, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDA000u, 0xD8000u}
/* backup and restore over USB (fm1.toml backup = true: device/backup.c): the settings and the scenes (their bass
 * sequences too), never the panel's calibration (the unit's). The version stays 1 when a record's layout changes:
 * each object carries its own magic, and a restore reloads it as a boot does (an earlier layout made over) */
#define APP_BACKUP_OBJS {OBJ_SETTINGS, OBJ_SCENES, OBJ_BSEQ}
#define APP_BACKUP_MAGIC 0x4B425246u     /* "FRBK" */
#define APP_BACKUP_VERSION 1u
#define APP_BACKUP_WHAT "replaces all scenes"
#define APP_BACKUP_YES B_SEL               /* the restore question: SEL yes, FX no, as every question here */
#define APP_BACKUP_YES_NAME "SEL"
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
#define APP_HOST_CMD                     /* "oldstore": records in the layout before BASS (app.c) */
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
