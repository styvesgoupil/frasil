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
#define APP_SPLASH                       /* the logo at power-on (ui.c), before the engine starts */
void app_splash(void);
#define APP_TICK                         /* 1 kHz, in the scan's interrupt: OCT's soft blink (app.c) */
void app_tick(void);
#define APP_TAGLINE "DRONE TEXTURES FROZEN IN MOTION"
#define APP_AUTHOR "by Styves Goupil"
/* the store: the app's objects and the panel calibration (the platform's), and their flash offsets */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_SCENES, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDA000u}
/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved state (or the first scene) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */
/* the host simulator's checks (host/plat_host.c "expect") */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD                     /* "oldstore": records in the layout before BASS (app.c) */
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
